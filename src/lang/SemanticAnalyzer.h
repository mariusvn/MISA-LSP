#pragma once
#include "lang/Ast.h"
#include "lang/Compilation.h"
#include "lang/SymbolTable.h"
#include "kb/KnowledgeBase.h"
#include "protocol/LspTypes.h"
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace misa::lang {

// Two-pass semantic analysis over the include-expanded statement stream of a
// compilation unit. Writes the unit's symbol table and each file's diagnostics.
// Pass 1: collect labels (forward references are allowed for labels).
// Pass 2: constants (define-before-use), references, instruction validation.
class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(Compilation& unit);

    void analyze();

private:
    Compilation&             m_unit;
    SymbolTable&             m_symbols;
    const kb::KnowledgeBase& m_kb;

    FileId   m_curFile = 0; // file of the statement being analysed
    uint32_t m_seq     = 0; // its position in m_unit.order

    void pass1();
    void pass2();

    lsp::Range spanToRange(Span sp) const;
    void emit       (Span sp, lsp::DiagnosticSeverity sev, std::string msg);
    void emitError  (Span sp, std::string msg);
    void emitWarning(Span sp, std::string msg);
    void emitHint   (Span sp, std::string msg);

    void analyzeInstruction(const InstructionStmt& instr);
    void analyzeOperand(const OperandNode& op, kb::OperandKind expected);

    // Qualify a local name with the current global scope.
    std::string qualify(const std::string& name) const;

    // A label, or a constant that is currently defined (not undef'd / not yet def'd).
    bool isKnownSymbol(const std::string& qname) const;

    // Records a reference to `name` (as written) and resolves it. Returns false
    // when the name is unknown.
    bool addReference(const std::string& name, Span span);

    // Recursively record symbol references for every identifier in an expression
    // and report unknown ones.
    void collectExprRefs(const ExprNode& expr);

    // Assemble-time value of a constant expression (literals, built-ins and
    // constants only). isFloat follows the assembler's promotion rules.
    struct Value { double v; bool isFloat; };
    std::optional<Value> evaluate(const ExprNode& expr) const;

    std::string m_currentGlobal;                // enclosing global label
    std::unordered_set<std::string> m_liveConsts; // constants currently defined
};

} // namespace misa::lang
