#include "lang/Lexer.h"
#include <cctype>
#include <string>

namespace misa::lang {

Lexer::Lexer(std::string_view source) : m_src(source) {}

bool Lexer::atEnd() const { return m_pos >= m_src.size(); }

char Lexer::peek(int offset) const {
    uint32_t i = m_pos + static_cast<uint32_t>(offset);
    return (i < m_src.size()) ? m_src[i] : '\0';
}

char Lexer::advance() {
    return atEnd() ? '\0' : m_src[m_pos++];
}

Token Lexer::makeToken(TokenType t, uint32_t start, std::string text) const {
    if (text.empty()) text = std::string(m_src.substr(start, m_pos - start));
    return Token{t, std::move(text), Span{start, m_pos}};
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static bool isIdentStart(char c) { return std::isalpha((unsigned char)c) || c == '_'; }
static bool isIdentBody(char c)  { return std::isalnum((unsigned char)c) || c == '_'; }

// ── Comment ───────────────────────────────────────────────────────────────────

void Lexer::readLineComment(std::vector<Token>& out) {
    uint32_t start = m_pos;
    advance(); // consume first '#'
    bool isDoc = false;
    if (!atEnd() && peek() == '#') {
        advance(); // consume second '#'
        isDoc = true;
    }
    // Read to end of line
    while (!atEnd() && peek() != '\n') advance();
    std::string text = std::string(m_src.substr(start, m_pos - start));
    out.push_back(Token{isDoc ? TokenType::DocComment : TokenType::Comment,
                        std::move(text), Span{start, m_pos}});
}

// ── Identifier ────────────────────────────────────────────────────────────────

Token Lexer::readIdent() {
    uint32_t start = m_pos;
    while (!atEnd() && isIdentBody(peek())) advance();

    // Qualified name: foo.bar (a single '.' immediately followed by an identifier
    // char). This must NOT swallow the '..' range operator (s0..s2), so we require
    // the char after the dot to start an identifier.
    while (!atEnd() && peek() == '.' && isIdentStart(peek(1))) {
        advance(); // consume '.'
        while (!atEnd() && isIdentBody(peek())) advance();
    }

    // Check if it's a label definition (followed by ':')
    if (!atEnd() && peek() == ':') {
        advance(); // consume ':'
        return makeToken(TokenType::LabelDef, start);
    }
    return makeToken(TokenType::Ident, start);
}

// ── Local identifier (.name or .name:) ────────────────────────────────────────

Token Lexer::readLocalIdent() {
    uint32_t start = m_pos;
    advance(); // consume '.'
    while (!atEnd() && isIdentBody(peek())) advance();

    if (!atEnd() && peek() == ':') {
        advance();
        return makeToken(TokenType::LocalLabelDef, start);
    }
    return makeToken(TokenType::LocalIdent, start);
}

// ── Reusable label (@name, @name:, @name+, @name-) ────────────────────────────

Token Lexer::readReusable() {
    uint32_t start = m_pos;
    advance(); // consume '@'
    while (!atEnd() && isIdentBody(peek())) advance();

    if (!atEnd()) {
        if (peek() == ':') { advance(); return makeToken(TokenType::ReusableLabelDef, start); }
        if (peek() == '+') { advance(); return makeToken(TokenType::ReusableUp,   start); }
        if (peek() == '-') { advance(); return makeToken(TokenType::ReusableDown, start); }
    }
    return makeToken(TokenType::ReusableRef, start);
}

// ── Number ────────────────────────────────────────────────────────────────────

Token Lexer::readNumber() {
    uint32_t start = m_pos;
    bool isFloat = false;
    std::string norm; // normalised (underscores stripped)

    auto consumeDigits = [&](auto pred) {
        while (!atEnd() && (pred(peek()) || peek() == '_')) {
            if (peek() != '_') norm += peek();
            advance();
        }
    };

    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
        norm += "0x"; advance(); advance();
        consumeDigits([](char c){ return std::isxdigit((unsigned char)c); });
    } else if (peek() == '0' && (peek(1) == 'b' || peek(1) == 'B')) {
        norm += "0b"; advance(); advance();
        consumeDigits([](char c){ return c == '0' || c == '1'; });
    } else if (peek() == '0' && (peek(1) == 'o' || peek(1) == 'O')) {
        norm += "0o"; advance(); advance();
        consumeDigits([](char c){ return c >= '0' && c <= '7'; });
    } else {
        consumeDigits([](char c){ return std::isdigit((unsigned char)c); });
        if (!atEnd() && peek() == '.' && std::isdigit((unsigned char)peek(1))) {
            isFloat = true;
            norm += '.'; advance();
            consumeDigits([](char c){ return std::isdigit((unsigned char)c); });
        }
    }

    TokenType t = isFloat ? TokenType::FloatLit : TokenType::IntLit;
    return Token{t, norm, Span{start, m_pos}};
}

// ── Escapes, strings and characters ──────────────────────────────────────────

void Lexer::error(Span sp, std::string msg) {
    m_diags.push_back({sp, lsp::DiagnosticSeverity::Error, std::move(msg)});
}

void Lexer::readEscape(uint32_t backslashPos, std::string& out) {
    if (atEnd() || peek() == '\n') {
        error(Span{backslashPos, m_pos}, "Incomplete escape sequence.");
        return;
    }
    char esc = advance();
    switch (esc) {
        case '0':  out += '\0'; break;
        case 't':  out += '\t'; break;
        case 'n':  out += '\n'; break;
        case '\'': out += '\''; break;
        case '"':  out += '"';  break;
        case '\\': out += '\\'; break;
        default:
            error(Span{backslashPos, m_pos},
                  std::string("Unknown escape sequence '\\") + esc +
                  "'. Supported: \\0 \\t \\n \\' \\\" \\\\.");
            out += '\\';
            out += esc;
            break;
    }
}

Token Lexer::readString(bool rawPath) {
    uint32_t start = m_pos;
    advance(); // consume opening '"'
    std::string value;
    while (!atEnd() && peek() != '"' && peek() != '\n') {
        uint32_t at = m_pos;
        char c = advance();
        if (c == '\\' && !rawPath) readEscape(at, value);
        else                       value += c;
    }
    if (!atEnd() && peek() == '"') advance(); // consume closing '"'
    else error(Span{start, m_pos}, "Unterminated string literal.");
    return Token{TokenType::StringLit, std::move(value), Span{start, m_pos}};
}

Token Lexer::readChar() {
    uint32_t start = m_pos;
    advance(); // consume opening '\''
    std::string value;
    while (!atEnd() && peek() != '\'' && peek() != '\n') {
        uint32_t at = m_pos;
        char c = advance();
        if (c == '\\') readEscape(at, value);
        else           value += c;
    }
    bool closed = !atEnd() && peek() == '\'';
    if (closed) advance();

    Span sp{start, m_pos};
    if (!closed)
        error(sp, "Unterminated character literal.");
    else if (value.empty())
        error(sp, "Empty character literal.");
    else if (value.size() > MAX_CHAR_LITERAL)
        error(sp, "Character literal has " + std::to_string(value.size()) +
                  " characters; at most " + std::to_string(MAX_CHAR_LITERAL) + " are allowed.");
    for (char c : value) {
        if (static_cast<unsigned char>(c) >= 0x80) {
            error(sp, "Character literals only support ASCII characters.");
            break;
        }
    }
    return Token{TokenType::CharLit, std::move(value), sp};
}

// ── Main tokenise loop ────────────────────────────────────────────────────────

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!atEnd()) {
        // Skip spaces and carriage returns (but not newlines)
        while (!atEnd() && (peek() == ' ' || peek() == '\t' || peek() == '\r'))
            advance();

        if (atEnd()) break;

        uint32_t start = m_pos;
        char c = peek();

        // Newline (logical line terminator)
        if (c == '\n') {
            advance();
            tokens.push_back(makeToken(TokenType::Newline, start, "\n"));
            continue;
        }

        // Comment
        if (c == '#') { readLineComment(tokens); continue; }

        // Identifiers and label defs
        if (isIdentStart(c)) { tokens.push_back(readIdent()); continue; }

        // Local label / identifier
        if (c == '.') {
            if (isIdentBody(peek(1))) { tokens.push_back(readLocalIdent()); continue; }
            // Could be '..' range op — fall through to operator handling
        }

        // Reusable label
        if (c == '@') { tokens.push_back(readReusable()); continue; }

        // Numbers
        if (std::isdigit((unsigned char)c)) { tokens.push_back(readNumber()); continue; }

        // String literal. Paths after `include` / `emb file` are taken verbatim so
        // Windows separators (C:\Users\...) are not read as escape sequences.
        if (c == '"') {
            bool rawPath = !tokens.empty() && tokens.back().is(TokenType::Ident) &&
                           (tokens.back().text == "include" || tokens.back().text == "file");
            tokens.push_back(readString(rawPath));
            continue;
        }

        // Character literal
        if (c == '\'') { tokens.push_back(readChar()); continue; }

        // $ (current address)
        if (c == '$') { advance(); tokens.push_back(makeToken(TokenType::Dollar, start)); continue; }

        // Multi-character operators
        advance(); // consume c
        switch (c) {
            case '+': tokens.push_back(makeToken(TokenType::Plus,    start)); break;
            case '-': tokens.push_back(makeToken(TokenType::Minus,   start)); break;
            case '~': tokens.push_back(makeToken(TokenType::Tilde,   start)); break;
            case '^': tokens.push_back(makeToken(TokenType::Caret,   start)); break;
            case '(': tokens.push_back(makeToken(TokenType::LParen,  start)); break;
            case ')': tokens.push_back(makeToken(TokenType::RParen,  start)); break;
            case ',': tokens.push_back(makeToken(TokenType::Comma,   start)); break;
            case '&':
                if (!atEnd() && peek() == '&') { advance(); tokens.push_back(makeToken(TokenType::AmpAmp,   start)); }
                else                           { tokens.push_back(makeToken(TokenType::Amp, start)); }
                break;
            case '|':
                if (!atEnd() && peek() == '|') { advance(); tokens.push_back(makeToken(TokenType::PipePipe, start)); }
                else                           { tokens.push_back(makeToken(TokenType::Pipe, start)); }
                break;
            case '=':
                if (!atEnd() && peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::EqEq,     start)); }
                else                           { tokens.push_back(makeToken(TokenType::Error, start)); }
                break;
            case '!':
                if (!atEnd() && peek() == '=') { advance(); tokens.push_back(makeToken(TokenType::BangEq,   start)); }
                else                           { tokens.push_back(makeToken(TokenType::Bang, start)); }
                break;
            case '<':
                if (!atEnd() && peek() == '<')       { advance(); tokens.push_back(makeToken(TokenType::LtLt,  start)); }
                else if (!atEnd() && peek() == '=')  { advance(); tokens.push_back(makeToken(TokenType::LtEq,  start)); }
                else                                 { tokens.push_back(makeToken(TokenType::Lt,    start)); }
                break;
            case '>':
                if (!atEnd() && peek() == '>')       { advance(); tokens.push_back(makeToken(TokenType::GtGt,  start)); }
                else if (!atEnd() && peek() == '=')  { advance(); tokens.push_back(makeToken(TokenType::GtEq,  start)); }
                else                                 { tokens.push_back(makeToken(TokenType::Gt,    start)); }
                break;
            case '*':
                if (!atEnd() && peek() == '*')       { advance(); tokens.push_back(makeToken(TokenType::StarStar, start)); }
                else                                 { tokens.push_back(makeToken(TokenType::Star,  start)); }
                break;
            case '/':
                tokens.push_back(makeToken(TokenType::Slash, start)); break;
            case '%':
                if (!atEnd() && peek() == '%')       { advance(); tokens.push_back(makeToken(TokenType::PercentPercent, start)); }
                else                                 { tokens.push_back(makeToken(TokenType::Percent, start)); }
                break;
            case '.':
                if (!atEnd() && peek() == '.')       { advance(); tokens.push_back(makeToken(TokenType::DotDot, start)); }
                else                                 { tokens.push_back(makeToken(TokenType::Error, start)); }
                break;
            default:
                tokens.push_back(makeToken(TokenType::Error, start, std::string(1, c)));
                break;
        }
        if (tokens.back().is(TokenType::Error)) {
            std::string msg = "Unexpected character '" + tokens.back().text + "'.";
            if (c == '=') msg += " Did you mean '=='?";
            error(tokens.back().span, std::move(msg));
        }
    }

    tokens.push_back(Token{TokenType::Eof, {}, Span{m_pos, m_pos}});
    return tokens;
}

} // namespace misa::lang
