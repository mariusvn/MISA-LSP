#pragma once
#include "lang/Token.h"
#include <string_view>
#include <vector>

namespace misa::lang {

// Converts MISA source text into a flat list of tokens.
// Newline tokens separate logical lines; the parser uses them as statement
// terminators. Comments are emitted as tokens so the document-symbols and
// folding providers can access doc-comment text.
class Lexer {
public:
    explicit Lexer(std::string_view source);

    // Tokenise the entire source and return the token list.
    std::vector<Token> tokenize();

    // Malformed literals and unexpected characters found by tokenize().
    const std::vector<SyntaxDiagnostic>& diagnostics() const { return m_diags; }

    // Maximum number of characters in a character literal ('abcd').
    static constexpr size_t MAX_CHAR_LITERAL = 4;

private:
    std::string_view              m_src;
    uint32_t                      m_pos = 0;
    std::vector<SyntaxDiagnostic> m_diags;

    char peek(int offset = 0) const;
    char advance();
    bool atEnd() const;

    Token makeToken(TokenType t, uint32_t start, std::string text = {}) const;

    Token readIdent();          // ident, keyword, register, mnemonic
    Token readLocalIdent();     // .name or .name:
    Token readReusable();       // @name, @name:, @name+, @name-
    Token readNumber();         // 42, 0x1F, 0b101, 0o77, 1.0
    Token readString(bool rawPath); // "..." (rawPath: no escape processing)
    Token readChar();           // 'a' … 'abcd'
    // Consumes the escape sequence after a '\' and appends the decoded character.
    void  readEscape(uint32_t backslashPos, std::string& out);
    void  error(Span sp, std::string msg);
    void  readLineComment(std::vector<Token>& out); // # or ##
};

} // namespace misa::lang
