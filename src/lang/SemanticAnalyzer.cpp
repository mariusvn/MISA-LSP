#include "lang/SemanticAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace misa::lang {

SemanticAnalyzer::SemanticAnalyzer(Compilation& unit)
    : m_unit(unit), m_symbols(unit.symbols), m_kb(kb::KnowledgeBase::get()) {}

lsp::Range SemanticAnalyzer::spanToRange(Span sp) const {
    return m_unit.files[m_curFile].range(sp);
}

void SemanticAnalyzer::emit(Span sp, lsp::DiagnosticSeverity sev, std::string msg) {
    m_unit.files[m_curFile].diagnostics.push_back(lsp::Diagnostic::make(spanToRange(sp), sev, std::move(msg)));
}
void SemanticAnalyzer::emitError  (Span sp, std::string msg) { emit(sp, lsp::DiagnosticSeverity::Error,   std::move(msg)); }
void SemanticAnalyzer::emitWarning(Span sp, std::string msg) { emit(sp, lsp::DiagnosticSeverity::Warning, std::move(msg)); }
void SemanticAnalyzer::emitHint   (Span sp, std::string msg) { emit(sp, lsp::DiagnosticSeverity::Hint,    std::move(msg)); }

std::string SemanticAnalyzer::qualify(const std::string& name) const {
    if (!name.empty() && name[0] == '.') {
        if (m_currentGlobal.empty()) return name.substr(1);
        return m_currentGlobal + name; // e.g. "foo.bar"
    }
    return name;
}

bool SemanticAnalyzer::isKnownSymbol(const std::string& qname) const {
    const SymbolDef* def = m_symbols.find(qname);
    if (!def) return false;
    if (def->kind == SymbolKind::Constant || def->kind == SymbolKind::LocalConstant)
        return m_liveConsts.count(qname) != 0;
    return true;
}

// ── Pass 1: collect labels ────────────────────────────────────────────────────

void SemanticAnalyzer::pass1() {
    m_currentGlobal.clear();
    for (m_seq = 0; m_seq < m_unit.order.size(); ++m_seq) {
        StmtRef ref = m_unit.order[m_seq];
        m_curFile = ref.file;
        const auto* ldef = std::get_if<LabelDefStmt>(&m_unit.stmt(ref));
        if (!ldef) continue;

        SymbolDef def;
        def.range    = spanToRange(ldef->span);
        def.selRange = def.range;
        def.file     = m_curFile;
        def.seq      = m_seq;

        switch (ldef->kind) {
            case LabelDefStmt::Kind::Global:
                if (m_symbols.find(ldef->name))
                    emitError(ldef->span, "Duplicate global label '" + ldef->name + "'.");
                m_currentGlobal = ldef->name;
                def.name = ldef->name;
                def.kind = SymbolKind::GlobalLabel;
                break;
            case LabelDefStmt::Kind::Local:
                if (m_currentGlobal.empty())
                    emitError(ldef->span,
                        "Local label '." + ldef->name + "' has no enclosing global label.");
                def.name   = m_currentGlobal.empty() ? ldef->name : m_currentGlobal + "." + ldef->name;
                def.kind   = SymbolKind::LocalLabel;
                def.parent = m_currentGlobal;
                if (m_symbols.find(def.name))
                    emitError(ldef->span, "Duplicate label '" + def.name + "'.");
                break;
            case LabelDefStmt::Kind::Reusable:
                def.name   = "@" + ldef->name;
                def.kind   = SymbolKind::ReusableLabel;
                def.parent = m_currentGlobal;
                break;
        }
        m_symbols.addDefinition(std::move(def));
    }
}

// ── Pass 2: resolve references, validate ─────────────────────────────────────

static bool isReadOnlyReg(const std::string& name) {
    return name == "pc" || name == "pa" || name == "ba";
}

bool SemanticAnalyzer::addReference(const std::string& name, Span span) {
    SymbolRef ref;
    ref.range = spanToRange(span);
    ref.file  = m_curFile;
    ref.seq   = m_seq;

    if (!name.empty() && name[0] == '@') {
        // Reusable label: @name- (nearest above) or @name+ (nearest below).
        char dir = name.back();
        if (dir != '+' && dir != '-') {
            emitError(span, "Reusable label reference '" + name + "' needs a direction: '" +
                            name + "-' (above) or '" + name + "+' (below).");
            return true;
        }
        std::string base = name.substr(1, name.size() - 2);
        ref.name   = "@" + base;
        ref.target = m_symbols.findReusable(base, m_seq, dir == '-');
        bool found = ref.target >= 0;
        if (!found)
            emitError(span, "No '@" + base + "' label " +
                            (dir == '-' ? "above" : "below") + " this reference.");
        m_symbols.addReference(std::move(ref));
        return true; // already reported
    }

    ref.name = qualify(name);
    bool known = isKnownSymbol(ref.name);
    if (known) ref.target = m_symbols.indexOf(ref.name);
    m_symbols.addReference(std::move(ref));
    return known;
}

void SemanticAnalyzer::analyzeOperand(const OperandNode& op, kb::OperandKind expected) {
    using OK = kb::OperandKind;

    if (op.kind == OperandNode::Kind::Register) {
        const std::string& name = op.regName;

        // Keyword-position operands (Type / Condition / Syscall): validate against
        // the expected enum kind instead of treating them as registers/identifiers.
        if (expected == OK::TypeK) {
            const auto* type = m_kb.lookupType(name);
            if (!type)
                emitError(op.span,
                    "Expected a type (i8t, u8t, i16t, u16t, i32t, u32t, f32t) but got '"
                    + name + "'.");
            else if (type->embedOnly)
                emitError(op.span, "Type '" + name + "' can only be used with emb.");
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

        if (!m_kb.isRegister(name) && !m_kb.isBuiltin(name) && !m_kb.isInstruction(name)) {
            if (!addReference(name, op.span))
                emitError(op.span, "Unknown identifier '" + name + "'.");
            else if (expected == OK::RegW)
                emitError(op.span, "Destination must be a register, not '" + name + "'.");
        } else if (!m_kb.isRegister(name) && expected == OK::RegW) {
            emitError(op.span, "Destination must be a register, not '" + name + "'.");
        }

        // Writing to read-only registers
        if (expected == OK::RegW && isReadOnlyReg(name))
            emitWarning(op.span, "Register '" + name +
                        "' is read-only; writing to it has no effect.");

        if (const auto* sym = m_symbols.find(qualify(name)); sym && sym->constValue) {
            if (expected == OK::RegRInt && sym->isFloat)
                emitWarning(op.span, "Constant '" + name + "' is a float where an integer is expected.");
            if (expected == OK::RegRFlt && !sym->isFloat)
                emitWarning(op.span, "Constant '" + name + "' is an integer where a float is expected.");
        }
        return;
    }

    if (op.kind == OperandNode::Kind::Expr) {
        if (expected == OK::RegW) {
            emitError(op.span, "Destination must be a register; an immediate cannot be written to.");
        }
        // Check for float literal in integer position or vice-versa
        if (expected == OK::RegRInt && std::holds_alternative<FloatLitExpr>(op.expr)) {
            emitError(op.span,
                "Float literal used where integer is expected. "
                "Use an integer literal (e.g. replace '1.0' with '1').");
        }
        if (expected == OK::RegRFlt && std::holds_alternative<IntLitExpr>(op.expr)) {
            emitError(op.span,
                "Integer literal used where float is expected. "
                "Use a float literal (e.g. replace '1' with '1.0').");
        }
        if (std::holds_alternative<StringExpr>(op.expr)) {
            emitError(op.span, "String literals can only be used with emb, bmk, sbmk and include.");
        }

        // Track identifier references (recursively, for compound expressions).
        collectExprRefs(op.expr);
    }
}

void SemanticAnalyzer::collectExprRefs(const ExprNode& expr) {
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, IdentExpr>) {
            if (node.name == "$" || m_kb.isBuiltin(node.name)) return;
            if (!addReference(node.name, node.span)) {
                if (m_kb.isRegister(node.name))
                    emitError(node.span, "Registers cannot be used inside an expression.");
                else
                    emitError(node.span, "Unknown identifier '" + node.name + "'.");
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

void SemanticAnalyzer::analyzeInstruction(const InstructionStmt& instr) {
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
    bool compact = info->compact && !expectedOps.empty() &&
                   actualOps.size() == expectedOps.size() - 1;
    size_t expected = compact ? expectedOps.size() - 1 : expectedOps.size();

    // syscall has exactly 1 operand (SysK).
    if (mnemonic == "syscall") {
        if (actualOps.empty()) {
            emitError(instr.span, "syscall requires a syscall name.");
            return;
        }
        const auto& op = actualOps[0];
        if (op.kind != OperandNode::Kind::Register || !m_kb.isSyscall(op.regName))
            emitError(op.span, "Unknown syscall '" +
                (op.kind == OperandNode::Kind::Register ? op.regName : std::string("?")) + "'.");
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
            analyzeOperand(actualOps[i], expectedOps[expectedIdx]);
    }
}

void SemanticAnalyzer::pass2() {
    m_currentGlobal.clear();

    // Entry points and whether their scope contains an exit.
    struct EntryInfo { FileId file; Span span; bool hasExit = false; };
    std::vector<std::pair<std::string, EntryInfo>> entryPoints;
    int currentEntry = -1; // index into entryPoints

    for (m_seq = 0; m_seq < m_unit.order.size(); ++m_seq) {
        StmtRef sref = m_unit.order[m_seq];
        m_curFile = sref.file;

        std::visit([&](const auto& s) {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, LabelDefStmt>) {
                if (s.kind == LabelDefStmt::Kind::Global) {
                    m_currentGlobal = s.name;
                    currentEntry = -1;
                    for (auto ep : kb::KnowledgeBase::ENTRY_POINTS) {
                        if (s.name == ep) {
                            entryPoints.push_back({s.name, EntryInfo{m_curFile, s.span}});
                            currentEntry = static_cast<int>(entryPoints.size()) - 1;
                        }
                    }
                }
            }

            else if constexpr (std::is_same_v<T, InstructionStmt>) {
                analyzeInstruction(s);
                if (currentEntry >= 0 && (s.mnemonic.text == "exit" || s.mnemonic.text == "yield"))
                    entryPoints[currentEntry].second.hasExit = true;
            }

            else if constexpr (std::is_same_v<T, DefDirective>) {
                if (s.name.empty()) {
                    emitError(s.span, "def expects a constant name and a value.");
                    return;
                }
                collectExprRefs(s.value);

                std::string qname = s.isLocal
                    ? (m_currentGlobal.empty() ? s.name : m_currentGlobal + "." + s.name)
                    : s.name;
                if (const auto* existing = m_symbols.find(qname);
                    existing && (existing->kind == SymbolKind::GlobalLabel ||
                                 existing->kind == SymbolKind::LocalLabel))
                    emitError(s.span, "'" + qname + "' is already defined as a label.");
                else if (m_liveConsts.count(qname))
                    emitWarning(s.span, "Constant '" + qname + "' is redefined without undef.");

                SymbolDef def;
                def.name     = qname;
                def.kind     = s.isLocal ? SymbolKind::LocalConstant : SymbolKind::Constant;
                def.range    = spanToRange(s.span);
                def.selRange = spanToRange(s.nameSpan);
                def.parent   = m_currentGlobal;
                def.file     = m_curFile;
                def.seq      = m_seq;
                if (auto v = evaluate(s.value)) {
                    def.constValue = v->v;
                    def.isFloat    = v->isFloat;
                }
                m_symbols.addDefinition(std::move(def));
                m_liveConsts.insert(qname);
            }

            else if constexpr (std::is_same_v<T, UndefDirective>) {
                if (s.name.empty()) {
                    emitError(s.span, "undef expects a constant name.");
                    return;
                }
                std::string qname = qualify(s.name);
                if (!m_liveConsts.count(qname)) {
                    emitWarning(s.span, "Constant '" + s.name + "' is not defined.");
                    return;
                }
                addReference(s.name, s.span);
                m_liveConsts.erase(qname);
            }

            else if constexpr (std::is_same_v<T, EmbDirective>) {
                const auto* type = m_kb.lookupType(s.typeName);
                if (!type)
                    emitError(s.span, "emb expects a type (i8t … f32t, string, file) but got '" +
                                      s.typeName + "'.");
                if (s.values.empty())
                    emitError(s.span, "emb expects at least one value.");
                for (const auto& v : s.values) {
                    bool isString = std::holds_alternative<StringExpr>(v);
                    if (type && type->embedOnly != isString)
                        emitError(exprSpan(v), type->embedOnly
                            ? "'" + s.typeName + "' expects a string literal."
                            : "String literals require the 'string' or 'file' type.");
                    if (!isString) collectExprRefs(v);
                }
            }

            else if constexpr (std::is_same_v<T, ResDirective>) {
                const auto* type = m_kb.lookupType(s.typeName);
                if (!type)
                    emitError(s.span, "res expects a scalar type (i8t … f32t) but got '" +
                                      s.typeName + "'.");
                else if (type->embedOnly)
                    emitError(s.span, "res only accepts scalar types, not '" + s.typeName + "'.");
                collectExprRefs(s.count);
                if (s.fill) collectExprRefs(*s.fill);
            }

        }, m_unit.stmt(sref));
    }

    // Hints for entry points without exit
    for (const auto& [name, info] : entryPoints) {
        if (info.hasExit) continue;
        m_curFile = info.file;
        emitHint(info.span, "Entry point '" + name + "' may be missing a final 'exit'.");
    }
}

// ── Constant evaluation ───────────────────────────────────────────────────────

std::optional<SemanticAnalyzer::Value> SemanticAnalyzer::evaluate(const ExprNode& expr) const {
    return std::visit([&](const auto& n) -> std::optional<Value> {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, IntLitExpr>) {
            return Value{static_cast<double>(n.value), false};
        } else if constexpr (std::is_same_v<T, FloatLitExpr>) {
            return Value{n.value, true};
        } else if constexpr (std::is_same_v<T, IdentExpr>) {
            if (n.name == "$") return std::nullopt;
            if (const auto* b = m_kb.lookupBuiltin(n.name)) return Value{b->value, b->isFloat};
            std::string qname = qualify(n.name);
            const auto* def = m_symbols.find(qname);
            if (def && def->constValue && m_liveConsts.count(qname))
                return Value{*def->constValue, def->isFloat};
            return std::nullopt;
        } else if constexpr (std::is_same_v<T, std::unique_ptr<CastExpr>>) {
            auto v = evaluate(n->operand);
            if (!v) return std::nullopt;
            if (n->toFloat) return Value{v->v, true};
            return Value{std::trunc(v->v), false};
        } else if constexpr (std::is_same_v<T, std::unique_ptr<UnaryExpr>>) {
            auto v = evaluate(n->operand);
            if (!v) return std::nullopt;
            switch (n->op.type) {
                case TokenType::Minus: return Value{-v->v, v->isFloat};
                case TokenType::Bang:  return Value{v->v == 0 ? 1.0 : 0.0, false};
                case TokenType::Tilde:
                    if (v->isFloat) return std::nullopt;
                    return Value{static_cast<double>(~static_cast<int64_t>(v->v)), false};
                default: return std::nullopt;
            }
        } else if constexpr (std::is_same_v<T, std::unique_ptr<BinaryExpr>>) {
            auto l = evaluate(n->left), r = evaluate(n->right);
            if (!l || !r) return std::nullopt;
            bool f = l->isFloat || r->isFloat;
            double a = l->v, b = r->v;
            auto ia = static_cast<int64_t>(a), ib = static_cast<int64_t>(b);
            switch (n->op.type) {
                case TokenType::Plus:  return Value{a + b, f};
                case TokenType::Minus: return Value{a - b, f};
                case TokenType::Star:  return Value{a * b, f};
                case TokenType::Slash:
                    if (b == 0) return std::nullopt;
                    return f ? Value{a / b, true} : Value{static_cast<double>(ia / ib), false};
                case TokenType::Percent: // sign of the divisor
                    if (b == 0) return std::nullopt;
                    if (f) { double m = std::fmod(a, b); if (m != 0 && ((m < 0) != (b < 0))) m += b; return Value{m, true}; }
                    { int64_t m = ia % ib; if (m != 0 && ((m < 0) != (ib < 0))) m += ib; return Value{static_cast<double>(m), false}; }
                case TokenType::PercentPercent: // sign of the dividend
                    if (b == 0) return std::nullopt;
                    return f ? Value{std::fmod(a, b), true} : Value{static_cast<double>(ia % ib), false};
                case TokenType::StarStar: {
                    double p = std::pow(a, b);
                    return Value{f ? p : std::trunc(p), f};
                }
                case TokenType::LtLt:  if (f) return std::nullopt; return Value{static_cast<double>(ia << (ib & 63)), false};
                case TokenType::GtGt:  if (f) return std::nullopt; return Value{static_cast<double>(ia >> (ib & 63)), false};
                case TokenType::Amp:   if (f) return std::nullopt; return Value{static_cast<double>(ia & ib), false};
                case TokenType::Pipe:  if (f) return std::nullopt; return Value{static_cast<double>(ia | ib), false};
                case TokenType::Caret: if (f) return std::nullopt; return Value{static_cast<double>(ia ^ ib), false};
                case TokenType::AmpAmp:   return Value{(a != 0 && b != 0) ? 1.0 : 0.0, false};
                case TokenType::PipePipe: return Value{(a != 0 || b != 0) ? 1.0 : 0.0, false};
                case TokenType::EqEq:  return Value{a == b ? 1.0 : 0.0, false};
                case TokenType::BangEq:return Value{a != b ? 1.0 : 0.0, false};
                case TokenType::Lt:    return Value{a <  b ? 1.0 : 0.0, false};
                case TokenType::Gt:    return Value{a >  b ? 1.0 : 0.0, false};
                case TokenType::LtEq:  return Value{a <= b ? 1.0 : 0.0, false};
                case TokenType::GtEq:  return Value{a >= b ? 1.0 : 0.0, false};
                default: return std::nullopt;
            }
        } else {
            return std::nullopt; // strings
        }
    }, expr);
}

void SemanticAnalyzer::analyze() {
    pass1();
    pass2();
}

} // namespace misa::lang
