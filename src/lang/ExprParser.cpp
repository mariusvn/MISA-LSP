#include "lang/ExprParser.h"
#include <stdexcept>

namespace misa::lang {

ExprParser::ExprParser(std::span<const Token> tokens, size_t startPos)
    : m_tokens(tokens), m_pos(startPos) {}

bool ExprParser::atEnd() const {
    return m_pos >= m_tokens.size() || m_tokens[m_pos].is(TokenType::Eof)
        || m_tokens[m_pos].is(TokenType::Newline);
}

const Token& ExprParser::current() const {
    if (m_pos < m_tokens.size()) return m_tokens[m_pos];
    static Token eof{TokenType::Eof, {}, {}};
    return eof;
}

const Token& ExprParser::peek(int offset) const {
    size_t i = m_pos + static_cast<size_t>(offset);
    if (i < m_tokens.size()) return m_tokens[i];
    static Token eof{TokenType::Eof, {}, {}};
    return eof;
}

const Token& ExprParser::consume() {
    const Token& t = m_tokens[m_pos];
    ++m_pos;
    return t;
}

// ── Precedence table (C-like, matching MISA spec) ─────────────────────────────
// Higher number = tighter binding.

int ExprParser::infixPrecedence(const Token& tok) {
    switch (tok.type) {
        case TokenType::PipePipe:        return 2;
        case TokenType::AmpAmp:          return 3;
        case TokenType::Pipe:            return 4;
        case TokenType::Caret:           return 5;
        case TokenType::Amp:             return 6;
        case TokenType::EqEq:
        case TokenType::BangEq:          return 7;
        case TokenType::Lt:
        case TokenType::Gt:
        case TokenType::LtEq:
        case TokenType::GtEq:            return 8;
        case TokenType::LtLt:
        case TokenType::GtGt:            return 9;
        case TokenType::Plus:
        case TokenType::Minus:           return 10;
        case TokenType::Star:
        case TokenType::Slash:
        case TokenType::Percent:
        case TokenType::PercentPercent:  return 11;
        case TokenType::StarStar:        return 12; // right-associative (power)
        default:                         return -1;
    }
}

bool ExprParser::isInfix(const Token& tok) {
    return infixPrecedence(tok) >= 0;
}

int ExprParser::prefixPrecedence(const Token& tok) {
    switch (tok.type) {
        case TokenType::Minus:
        case TokenType::Bang:
        case TokenType::Tilde: return 13;
        default:               return -1;
    }
}

// ── Pratt parsing ─────────────────────────────────────────────────────────────

ExprNode ExprParser::parsePrefix() {
    const Token& tok = current();

    // Grouping
    if (tok.is(TokenType::LParen)) {
        consume();
        ExprNode inner = parseExpr(0);
        if (!atEnd() && current().is(TokenType::RParen)) consume();
        return inner;
    }

    // Unary prefix operators
    if (prefixPrecedence(tok) >= 0) {
        Token op = tok;
        consume();
        ExprNode operand = parseExpr(prefixPrecedence(op));
        Span span{op.span.start, exprSpan(operand).end};
        auto node = std::make_unique<UnaryExpr>(UnaryExpr{op, std::move(operand), span});
        return node;
    }

    // Cast
    if (tok.is(TokenType::Ident) && (tok.text == "icast" || tok.text == "fcast")) {
        bool toFloat = tok.text == "fcast";
        consume();
        ExprNode operand = parseExpr(13); // unary precedence
        Span span{tok.span.start, exprSpan(operand).end};
        auto node = std::make_unique<CastExpr>(CastExpr{toFloat, std::move(operand), span});
        return node;
    }

    // Literals
    if (tok.is(TokenType::IntLit)) {
        consume();
        int64_t val = 0;
        try {
            const std::string& t = tok.text;
            if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X'))
                val = (int64_t)std::stoull(t, nullptr, 16);
            else if (t.size() > 2 && t[0] == '0' && (t[1] == 'b' || t[1] == 'B'))
                val = (int64_t)std::stoull(t.substr(2), nullptr, 2);
            else if (t.size() > 2 && t[0] == '0' && (t[1] == 'o' || t[1] == 'O'))
                val = (int64_t)std::stoull(t.substr(2), nullptr, 8);
            else
                val = std::stoll(t);
        } catch (...) {}
        return IntLitExpr{val, tok.span};
    }

    if (tok.is(TokenType::FloatLit)) {
        consume();
        double val = 0.0;
        try { val = std::stod(tok.text); } catch (...) {}
        return FloatLitExpr{val, tok.span};
    }

    if (tok.is(TokenType::StringLit)) {
        consume();
        return StringExpr{tok.text, tok.span};
    }

    if (tok.is(TokenType::Dollar)) {
        consume();
        return IdentExpr{"$", tok.span};
    }

    // Identifier (label, constant, built-in)
    if (tok.is(TokenType::Ident) || tok.is(TokenType::LocalIdent) ||
        tok.is(TokenType::ReusableUp) || tok.is(TokenType::ReusableDown) ||
        tok.is(TokenType::ReusableRef)) {
        consume();
        return IdentExpr{tok.text, tok.span};
    }

    // Unknown: return 0 literal to avoid hard failure
    return IntLitExpr{0, tok.span};
}

ExprNode ExprParser::parsePostfix(ExprNode left, int minPrec) {
    while (!atEnd()) {
        const Token& op = current();
        int prec = infixPrecedence(op);
        if (prec < minPrec) break;

        consume();
        // ** is right-associative
        int nextPrec = (op.is(TokenType::StarStar)) ? prec : prec + 1;
        ExprNode right = parseExpr(nextPrec);

        Span span{exprSpan(left).start, exprSpan(right).end};
        auto node = std::make_unique<BinaryExpr>(BinaryExpr{op, std::move(left), std::move(right), span});
        left = std::move(node);
    }
    return left;
}

ExprNode ExprParser::parseExpr(int minPrec) {
    ExprNode left = parsePrefix();
    return parsePostfix(std::move(left), minPrec);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

Span exprSpan(const ExprNode& e) {
    return std::visit([](const auto& n) -> Span {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, std::unique_ptr<UnaryExpr>>)       return n->span;
        else if constexpr (std::is_same_v<T, std::unique_ptr<BinaryExpr>>) return n->span;
        else if constexpr (std::is_same_v<T, std::unique_ptr<CastExpr>>)   return n->span;
        else                                                                return n.span;
    }, e);
}

} // namespace misa::lang
