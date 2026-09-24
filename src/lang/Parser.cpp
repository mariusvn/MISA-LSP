#include "lang/Parser.h"
#include "lang/ExprParser.h"
#include <algorithm>
#include <span>

namespace misa::lang {

Parser::Parser(std::vector<Token> tokens) : m_tokens(std::move(tokens)) {}

bool Parser::atEnd() const {
    return m_pos >= m_tokens.size() || m_tokens[m_pos].is(TokenType::Eof);
}

const Token& Parser::current() const {
    if (m_pos < m_tokens.size()) return m_tokens[m_pos];
    static Token eof{TokenType::Eof, {}, {}};
    return eof;
}

const Token& Parser::peek(int offset) const {
    size_t i = m_pos + static_cast<size_t>(offset);
    if (i < m_tokens.size()) return m_tokens[i];
    static Token eof{TokenType::Eof, {}, {}};
    return eof;
}

const Token& Parser::consume() {
    const Token& t = m_tokens[m_pos];
    ++m_pos;
    return t;
}

void Parser::error(Span sp, std::string msg) {
    m_diags.push_back({sp, lsp::DiagnosticSeverity::Error, std::move(msg)});
}

bool Parser::atLineEnd() const {
    return atEnd() || current().is(TokenType::Newline) ||
           current().is(TokenType::Comment) || current().is(TokenType::DocComment);
}

void Parser::skipNewlines() {
    while (!atEnd() && (current().is(TokenType::Newline) ||
                        current().is(TokenType::Comment)))
        ++m_pos;
}

void Parser::skipToNewline() {
    while (!atEnd() && !current().is(TokenType::Newline))
        ++m_pos;
}

// ── Operand parsing ───────────────────────────────────────────────────────────

ExprNode Parser::parseExprInLine() {
    // Feed remaining line tokens to ExprParser
    std::span<const Token> view(m_tokens.data(), m_tokens.size());
    ExprParser ep(view, m_pos, &m_diags);
    ExprNode result = ep.parseExpr();
    m_pos = ep.pos();
    return result;
}

OperandNode Parser::parseOperand() {
    const Token& tok = current();
    uint32_t start = tok.span.start;

    // Register range: ident '..' [ident]   (e.g. s0..s2 or open-ended t4..)
    if (tok.is(TokenType::Ident) && peek(1).is(TokenType::DotDot)) {
        std::string name = tok.text;
        Span s = tok.span;
        consume(); // ident
        consume(); // '..'
        std::optional<std::string> regEnd;
        if (!atEnd() && current().is(TokenType::Ident)) {
            regEnd = current().text;
            consume();
        }
        Span rspan{s.start, m_tokens[m_pos - 1].span.end};
        RegRange rr{name, regEnd, rspan};
        return OperandNode{OperandNode::Kind::Range, {}, rr, IntLitExpr{0, s}, rspan};
    }

    // Otherwise parse the operand as a full assemble-time expression. This handles
    // labels, constants, arithmetic (e.g. `A.B + C.D`), casts, parentheses, etc.
    ExprNode expr = parseExprInLine();
    Span sp{start, exprSpan(expr).end};

    // A single plain identifier keeps Register/keyword semantics so that register-
    // and keyword-position validation (Type/Condition/Syscall) works. Reusable
    // (@x, @x+, @x-) and local (.x) references stay as expressions — they are
    // resolved separately and must not be checked as strict identifiers.
    if (const auto* id = std::get_if<IdentExpr>(&expr)) {
        const std::string& nm = id->name;
        if (!nm.empty() && nm[0] != '@' && nm[0] != '.') {
            std::string name = nm;
            return OperandNode{OperandNode::Kind::Register, std::move(name), {}, std::move(expr), sp};
        }
    }
    return OperandNode{OperandNode::Kind::Expr, {}, {}, std::move(expr), sp};
}

std::vector<OperandNode> Parser::parseOperandList() {
    std::vector<OperandNode> ops;
    if (atEnd() || current().is(TokenType::Newline) || current().is(TokenType::Comment))
        return ops;

    ops.push_back(parseOperand());
    while (!atEnd() && current().is(TokenType::Comma)) {
        consume(); // consume ','
        if (atEnd() || current().is(TokenType::Newline)) break;
        ops.push_back(parseOperand());
    }
    return ops;
}

// ── Statement parsing ─────────────────────────────────────────────────────────

Statement Parser::parseLabelDef(TokenType kind) {
    const Token& tok = consume();
    std::string name = tok.text;
    // Remove trailing ':'  and leading '.' or '@'
    if (!name.empty() && name.back() == ':') name.pop_back();
    if (!name.empty() && (name[0] == '.' || name[0] == '@')) name = name.substr(1);

    LabelDefStmt::Kind k = LabelDefStmt::Kind::Global;
    if (kind == TokenType::LocalLabelDef)    k = LabelDefStmt::Kind::Local;
    if (kind == TokenType::ReusableLabelDef) k = LabelDefStmt::Kind::Reusable;
    return LabelDefStmt{k, std::move(name), tok.span};
}

Statement Parser::parseDef() {
    Span start = current().span;
    consume(); // 'def'

    bool isLocal = false;
    std::string name;
    Span nameSpan = start;

    if (!atEnd() && current().is(TokenType::LocalIdent)) {
        name = current().text;
        if (!name.empty() && name[0] == '.') name = name.substr(1);
        isLocal = true;
        nameSpan = consume().span;
    } else if (!atEnd() && current().is(TokenType::Ident)) {
        name = current().text;
        nameSpan = consume().span;
    }

    ExprNode value = IntLitExpr{0, current().span};
    if (!atLineEnd())
        value = parseExprInLine();
    else if (!name.empty())
        error(nameSpan, "Constant '" + name + "' has no value.");

    Span sp{start.start, std::max(nameSpan.end, exprSpan(value).end)};
    return DefDirective{std::move(name), isLocal, std::move(value), sp, nameSpan};
}

Statement Parser::parseUndef() {
    Span start = current().span;
    consume(); // 'undef'
    std::string name; // local constants keep their leading '.'
    if (!atEnd() && (current().is(TokenType::Ident) || current().is(TokenType::LocalIdent))) {
        name = current().text;
        consume();
    }
    return UndefDirective{std::move(name), Span{start.start, current().span.start}};
}

Statement Parser::parseEmb() {
    Span start = current().span;
    consume(); // 'emb'

    std::string typeName;
    if (!atEnd() && current().is(TokenType::Ident)) {
        typeName = current().text;
        consume();
    }
    // optional comma
    if (!atEnd() && current().is(TokenType::Comma)) consume();

    std::vector<ExprNode> values;
    while (!atEnd() && !current().is(TokenType::Newline) && !current().is(TokenType::Comment)) {
        values.push_back(parseExprInLine());
        if (!atEnd() && current().is(TokenType::Comma)) consume();
        else break;
    }

    Span sp{start.start, current().span.start};
    return EmbDirective{std::move(typeName), std::move(values), sp};
}

Statement Parser::parseRes() {
    Span start = current().span;
    consume(); // 'res'

    std::string typeName;
    if (!atEnd() && current().is(TokenType::Ident)) {
        typeName = current().text;
        consume();
    }
    if (!atEnd() && current().is(TokenType::Comma)) consume();

    ExprNode count = IntLitExpr{0, current().span};
    if (!atEnd() && !current().is(TokenType::Newline))
        count = parseExprInLine();

    std::optional<ExprNode> fill;
    if (!atEnd() && current().is(TokenType::Comma)) {
        consume();
        if (!atEnd() && !current().is(TokenType::Newline))
            fill = parseExprInLine();
    }

    Span sp{start.start, current().span.start};
    return ResDirective{std::move(typeName), std::move(count), std::move(fill), sp};
}

Statement Parser::parseBmk(bool isSub) {
    Span start = current().span;
    consume(); // 'bmk' or 'sbmk'
    std::string label;
    if (!atEnd() && current().is(TokenType::StringLit)) {
        label = current().text;
        consume();
    }
    return BmkDirective{isSub, std::move(label), Span{start.start, current().span.start}};
}

Statement Parser::parseInclude() {
    Span start = current().span;
    consume(); // 'include'
    IncludeDirective inc;
    if (!atEnd() && current().is(TokenType::StringLit)) {
        inc.path     = current().text;
        inc.hasPath  = true;
        inc.pathSpan = current().span;
        consume();
    } else {
        error(start, "'include' expects a quoted path, e.g. include \"lib/utils.asm\".");
    }
    inc.span = Span{start.start, inc.hasPath ? inc.pathSpan.end : start.end};
    return inc;
}

Statement Parser::parseInstruction(Token mnemonic) {
    std::vector<OperandNode> ops = parseOperandList();
    Span sp{mnemonic.span.start, ops.empty() ? mnemonic.span.end : ops.back().span.end};
    return InstructionStmt{std::move(mnemonic), std::move(ops), sp};
}

Statement Parser::parseLine() {
    const Token& tok = current();

    if (tok.is(TokenType::DocComment)) {
        DocCommentStmt s{tok.text, tok.span};
        consume();
        return s;
    }

    if (tok.is(TokenType::LabelDef))        return parseLabelDef(TokenType::LabelDef);
    if (tok.is(TokenType::LocalLabelDef))   return parseLabelDef(TokenType::LocalLabelDef);
    if (tok.is(TokenType::ReusableLabelDef))return parseLabelDef(TokenType::ReusableLabelDef);

    if (tok.is(TokenType::Ident)) {
        if (tok.text == "def")  return parseDef();
        if (tok.text == "undef")return parseUndef();
        if (tok.text == "emb")  return parseEmb();
        if (tok.text == "res")  return parseRes();
        if (tok.text == "bmk")  return parseBmk(false);
        if (tok.text == "sbmk") return parseBmk(true);
        if (tok.text == "include") return parseInclude();
        // Everything else is an instruction mnemonic
        Token mnemonic = consume();
        return parseInstruction(std::move(mnemonic));
    }

    // Unexpected token on a line — skip it. Error tokens were already reported
    // by the lexer.
    Span sp = tok.span;
    if (!tok.is(TokenType::Error))
        error(sp, "Unexpected '" + tok.text + "' at the start of a statement.");
    skipToNewline();
    return EmptyStmt{sp};
}

std::vector<Statement> Parser::parse() {
    std::vector<Statement> stmts;
    while (!atEnd()) {
        skipNewlines();
        if (atEnd()) break;
        stmts.push_back(parseLine());
        // A label may be followed by a statement on the same line (`lbl: emb …`).
        if (std::holds_alternative<LabelDefStmt>(stmts.back())) continue;
        if (!atLineEnd()) {
            const Token& stray = current();
            if (!stray.is(TokenType::Error))
                error(stray.span, "Unexpected '" + stray.text + "'; expected end of line.");
            skipToNewline();
        }
        // Consume trailing comment if any
        if (!atEnd() && current().is(TokenType::Comment)) consume();
        if (!atEnd() && current().is(TokenType::Newline)) consume();
    }
    return stmts;
}

} // namespace misa::lang
