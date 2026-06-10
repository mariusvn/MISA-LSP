#pragma once
#include "lang/Ast.h"
#include "lang/SymbolTable.h"
#include "kb/KnowledgeBase.h"
#include "protocol/LspTypes.h"
#include "text/TextDocument.h"
#include <vector>

namespace misa::lang {

// Two-pass semantic analysis over a parsed Statement list.
// Pass 1: collect global labels (forward-ref support).
// Pass 2: resolve references, validate instructions, emit diagnostics.
class SemanticAnalyzer {
public:
    SemanticAnalyzer(const std::vector<Statement>& stmts,
                     const text::TextDocument& doc);

    void analyze();

    SymbolTable& symbolTable() { return m_symbols; }
    std::vector<lsp::Diagnostic>& diagnostics() { return m_diags; }

private:
    const std::vector<Statement>& m_stmts;
    const text::TextDocument&     m_doc;
    SymbolTable                   m_symbols;
    std::vector<lsp::Diagnostic>  m_diags;
    const kb::KnowledgeBase&      m_kb;

    void pass1(); // collect global labels
    void pass2(); // resolve + validate

    lsp::Range spanToRange(Span sp) const;
    void emitError  (Span sp, std::string msg);
    void emitWarning(Span sp, std::string msg);
    void emitHint   (Span sp, std::string msg);

    void analyzeInstruction(const InstructionStmt& instr, uint32_t lineIdx);
    void analyzeOperand(const OperandNode& op, kb::OperandKind expected,
                        uint32_t lineIdx, bool compact);

    // Qualify a local name with the current global scope.
    std::string qualify(const std::string& name) const;

    // Recursively record symbol references for every identifier in an expression.
    void collectExprRefs(const ExprNode& expr);

    std::string m_currentGlobal;  // name of current enclosing global label
    std::unordered_map<std::string, uint32_t> m_constDefLine; // for order check
};

} // namespace misa::lang
