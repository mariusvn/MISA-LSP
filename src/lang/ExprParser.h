#pragma once
#include "lang/Ast.h"
#include "lang/Token.h"
#include <span>
#include <vector>

namespace misa::lang {

// Pratt parser for MISA assemble-time expressions.
// Consumes tokens from a view; the caller manages the token stream.
class ExprParser {
public:
    // `diags`, when given, receives syntax errors such as a missing ')'.
    explicit ExprParser(std::span<const Token> tokens, size_t startPos = 0,
                        std::vector<SyntaxDiagnostic>* diags = nullptr);

    // Parse one expression. Returns the AST node.
    ExprNode parseExpr(int minPrec = 0);

    size_t pos() const { return m_pos; }

    bool atEnd() const;
    const Token& current() const;

private:
    std::span<const Token>         m_tokens;
    size_t                         m_pos;
    std::vector<SyntaxDiagnostic>* m_diags;

    const Token& peek(int offset = 0) const;
    const Token& consume();

    ExprNode parsePrefix();
    ExprNode parsePostfix(ExprNode left, int minPrec);

    static int infixPrecedence(const Token& tok);
    static int prefixPrecedence(const Token& tok);
    static bool isInfix(const Token& tok);
};

} // namespace misa::lang
