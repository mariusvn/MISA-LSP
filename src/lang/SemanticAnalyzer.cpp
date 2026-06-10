#include "lang/SemanticAnalyzer.h"
#include <algorithm>
#include <sstream>

namespace misa::lang {

SemanticAnalyzer::SemanticAnalyzer(const std::vector<Statement>& stmts,
                                   const text::TextDocument& doc)
    : m_stmts(stmts), m_doc(doc), m_kb(kb::KnowledgeBase::get()) {}

lsp::Range SemanticAnalyzer::spanToRange(Span sp) const {
    return lsp::Range{m_doc.offsetToPosition(sp.start),
                      m_doc.offsetToPosition(sp.end)};
}

void SemanticAnalyzer::emitError(Span sp, std::string msg) {
    m_diags.push_back({spanToRange(sp), lsp::DiagnosticSeverity::Error,
                       std::move(msg), "misa-lsp"});
}
void SemanticAnalyzer::emitWarning(Span sp, std::string msg) {
    m_diags.push_back({spanToRange(sp), lsp::DiagnosticSeverity::Warning,
                       std::move(msg), "misa-lsp"});
}
void SemanticAnalyzer::emitHint(Span sp, std::string msg) {
    m_diags.push_back({spanToRange(sp), lsp::DiagnosticSeverity::Hint,
                       std::move(msg), "misa-lsp"});
}

std::string SemanticAnalyzer::qualify(const std::string& name) const {
    if (!name.empty() && name[0] == '.') {
        if (m_currentGlobal.empty()) return name;
        return m_currentGlobal + name; // e.g. "foo.bar"
    }
    return name;
}

// ── Pass 1: collect global labels ─────────────────────────────────────────────

void SemanticAnalyzer::pass1() {
    for (const auto& stmt : m_stmts) {
        if (const auto* ldef = std::get_if<LabelDefStmt>(&stmt)) {
            if (ldef->kind == LabelDefStmt::Kind::Global) {
                // Check for duplicate global labels
                std::string qname = ldef->name;
                if (m_symbols.find(qname)) {
                    emitError(ldef->span,
                        "Duplicate global label '" + ldef->name + "'.");
                }
                SymbolDef def;
                def.name     = qname;
                def.kind     = SymbolKind::GlobalLabel;
                def.range    = spanToRange(ldef->span);
                def.selRange = def.range;
                m_symbols.addDefinition(std::move(def));
            }
        }
    }
}

// ── Pass 2: resolve references, validate ─────────────────────────────────────

static bool isReadOnlyReg(const std::string& name) {
    return name == "pc" || name == "pa" || name == "ba";
}

void SemanticAnalyzer::analyzeOperand(
    const OperandNode& op,
    kb::OperandKind expected,
    uint32_t /*lineIdx*/,
    bool /*compact*/)
{
    using OK = kb::OperandKind;

    if (op.kind == OperandNode::Kind::Register) {
        const std::string& name = op.regName;

        // Keyword-position operands (Type / Condition / Syscall): validate against
        // the expected enum kind instead of treating them as registers/identifiers.
        if (expected == OK::TypeK) {
            if (!m_kb.isType(name))
                emitError(op.span,
                    "Expected a type (i8t, u8t, i16t, u16t, i32t, u32t, f32t) but got '"
                    + name + "'.");
            return;
        }
        if (expected == OK::CondK) {
            if (!m_kb.isCondition(name))
                emitError(op.span,
                    "Expected a condition (eq, neq, lt, gt, flt, fgt, …) but got '"
                    + name + "'.");
            return;
        }
        if (expected == OK::SysK) {
            if (!m_kb.isSyscall(name))
                emitError(op.span, "Unknown syscall '" + name + "'.");
            return;
        }

        // Track reference
        m_symbols.addReference(SymbolRef{name, spanToRange(op.span),
                                         op.span.start});

        // Validate it's actually a register
        if (!m_kb.isRegister(name)) {
            // Could be a label/const used as register position — emit warning
            if (!m_kb.isBuiltin(name) && m_symbols.find(qualify(name)) == nullptr
                && !m_kb.isInstruction(name)) {
                emitError(op.span, "Unknown identifier '" + name + "'.");
            }
        }

        // Writing to read-only registers
        if (expected == OK::RegW && isReadOnlyReg(name))
            emitWarning(op.span, "Register '" + name +
                        "' is read-only; writing to it has no effect.");
    }

    if (op.kind == OperandNode::Kind::Expr) {
        // Check for float literal in integer position or vice-versa
        if (expected == OK::RegRInt || expected == OK::RegW) {
            if (std::holds_alternative<FloatLitExpr>(op.expr)) {
                emitError(op.span,
                    "Float literal used where integer is expected. "
                    "Use an integer literal (e.g. replace '1.0' with '1').");
            }
        }
        if (expected == OK::RegRFlt) {
            if (std::holds_alternative<IntLitExpr>(op.expr)) {
                emitError(op.span,
                    "Integer literal used where float is expected. "
                    "Use a float literal (e.g. replace '1' with '1.0').");
            }
        }

        // Track identifier references (recursively, for compound expressions).
        collectExprRefs(op.expr);
    }
}

void SemanticAnalyzer::collectExprRefs(const ExprNode& expr) {
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, IdentExpr>) {
            if (node.name != "$" && !m_kb.isBuiltin(node.name)) {
                std::string qname = qualify(node.name);
                m_symbols.addReference(SymbolRef{qname, spanToRange(node.span),
                                                 node.span.start});
            }
        } else if constexpr (std::is_same_v<T, std::unique_ptr<UnaryExpr>>) {
            collectExprRefs(node->operand);
        } else if constexpr (std::is_same_v<T, std::unique_ptr<CastExpr>>) {
            collectExprRefs(node->operand);
        } else if constexpr (std::is_same_v<T, std::unique_ptr<BinaryExpr>>) {
            collectExprRefs(node->left);
            collectExprRefs(node->right);
        }
        // IntLitExpr / FloatLitExpr / StringExpr: nothing to record.
    }, expr);
}

void SemanticAnalyzer::analyzeInstruction(const InstructionStmt& instr, uint32_t /*lineIdx*/) {
    const std::string& mnemonic = instr.mnemonic.text;

    // Check mnemonic is known
    const auto* info = m_kb.lookupInstruction(mnemonic);
    if (!info) {
        emitError(instr.mnemonic.span,
            "Unknown instruction '" + mnemonic + "'.");
        return;
    }

    const auto& expectedOps = info->operands;
    const auto& actualOps   = instr.operands;

    // Handle compact form [c]: one fewer operand (dest == first src)
    bool compact = false;
    if (info->compact && !expectedOps.empty()) {
        size_t baseArity = expectedOps.size();
        if (actualOps.size() == baseArity - 1)
            compact = true;
    }

    size_t expected = compact ? expectedOps.size() - 1 : expectedOps.size();

    // Special case: emb/res can have variable operands — handled in parsers.
    // syscall has exactly 1 operand (SysK).
    if (mnemonic == "syscall") {
        if (actualOps.empty()) {
            emitError(instr.span, "syscall requires a syscall name.");
            return;
        }
        const auto& op = actualOps[0];
        if (op.kind == OperandNode::Kind::Register) {
            if (!m_kb.isSyscall(op.regName))
                emitError(op.span, "Unknown syscall '" + op.regName + "'.");
        }
        return;
    }

    // Validate arity
    if (actualOps.size() != expected) {
        std::ostringstream ss;
        ss << "'" << mnemonic << "' expects " << expected << " operand(s)";
        if (info->compact)
            ss << " (or " << (expected + 1) << " in base form)";
        ss << ", got " << actualOps.size() << ".";
        emitError(instr.span, ss.str());
        // Still validate what we have
    }

    // Validate each operand. In compact form the destination is omitted, so the
    // i-th actual operand maps to the (i+1)-th expected operand.
    for (size_t i = 0; i < actualOps.size(); ++i) {
        size_t expectedIdx = compact ? i + 1 : i;
        if (expectedIdx < expectedOps.size())
            analyzeOperand(actualOps[i], expectedOps[expectedIdx],
                           instr.mnemonic.span.start, compact);
    }

    // (cmp's condition operand is validated generically via the CondK operand kind.)

    // Check entry-point exit heuristic (simple: warn if last instr of global scope is not exit)
    // Handled in post-analysis below.
}

void SemanticAnalyzer::pass2() {
    m_currentGlobal = "";
    uint32_t stmtIdx = 0;

    // Track which entry points we saw and whether they have an exit
    struct EntryInfo { bool hasExit = false; uint32_t lastLine = 0; };
    std::unordered_map<std::string, EntryInfo> entryPoints;

    for (const auto& stmt : m_stmts) {
        std::visit([&](const auto& s) {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, LabelDefStmt>) {
                if (s.kind == LabelDefStmt::Kind::Global) {
                    m_currentGlobal = s.name;
                    m_symbols.currentGlobalLabel = s.name;
                    // Check if entry point
                    for (auto ep : kb::KnowledgeBase::ENTRY_POINTS) {
                        if (s.name == ep) entryPoints[s.name];
                    }
                } else {
                    // Local or reusable label
                    std::string qname = (s.kind == LabelDefStmt::Kind::Local)
                        ? m_currentGlobal + "." + s.name
                        : "@" + s.name;

                    if (m_symbols.find(qname) && s.kind != LabelDefStmt::Kind::Reusable) {
                        emitError(s.span, "Duplicate label '" + qname + "'.");
                    }

                    if (s.kind == LabelDefStmt::Kind::Local && m_currentGlobal.empty()) {
                        emitError(s.span,
                            "Local label '." + s.name + "' has no enclosing global label.");
                    }

                    SymbolDef def;
                    def.name     = qname;
                    def.kind     = (s.kind == LabelDefStmt::Kind::Reusable)
                                 ? SymbolKind::ReusableLabel : SymbolKind::LocalLabel;
                    def.range    = spanToRange(s.span);
                    def.selRange = def.range;
                    def.parent   = m_currentGlobal;
                    m_symbols.addDefinition(std::move(def));
                }
            }

            else if constexpr (std::is_same_v<T, InstructionStmt>) {
                analyzeInstruction(s, stmtIdx);
                // Track exit for entry-point heuristic
                if (!m_currentGlobal.empty()) {
                    auto it = entryPoints.find(m_currentGlobal);
                    if (it != entryPoints.end()) {
                        if (s.mnemonic.text == "exit") it->second.hasExit = true;
                        it->second.lastLine = s.span.start;
                    }
                }
            }

            else if constexpr (std::is_same_v<T, DefDirective>) {
                std::string qname = s.isLocal
                    ? (m_currentGlobal.empty() ? s.name : m_currentGlobal + "." + s.name)
                    : s.name;

                // Constants must be defined before use (enforced by reference check later)
                SymbolDef def;
                def.name     = qname;
                def.kind     = s.isLocal ? SymbolKind::LocalConstant : SymbolKind::Constant;
                def.range    = spanToRange(s.span);
                def.selRange = def.range;
                def.parent   = m_currentGlobal;
                m_symbols.addDefinition(std::move(def));
                m_constDefLine[qname] = s.span.start;
            }

            else if constexpr (std::is_same_v<T, UndefDirective>) {
                // Remove from constant tracking (simplified)
                m_constDefLine.erase(s.name);
            }

        }, stmt);
        ++stmtIdx;
    }

    // Emit hints for entry points without exit
    for (const auto& [name, info] : entryPoints) {
        if (!info.hasExit) {
            // Find the label def span
            const auto* def = m_symbols.find(name);
            if (def) {
                emitHint(Span{m_doc.positionToOffset(def->range.start),
                              m_doc.positionToOffset(def->range.end)},
                    "Entry point '" + name + "' may be missing a final 'exit'.");
            }
        }
    }
}

void SemanticAnalyzer::analyze() {
    pass1();
    pass2();
}

} // namespace misa::lang
