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

private:
    std::string_view m_src;
    uint32_t         m_pos = 0;

    char peek(int offset = 0) const;
    char advance();
    bool atEnd() const;

    Token makeToken(TokenType t, uint32_t start, std::string text = {}) const;

    Token readIdent();          // ident, keyword, register, mnemonic
    Token readLocalIdent();     // .name or .name:
    Token readReusable();       // @name, @name:, @name+, @name-
    Token readNumber();         // 42, 0x1F, 0b101, 0o77, 1.0
    Token readString();         // "..."
    void  readLineComment(std::vector<Token>& out); // # or ##
};

} // namespace misa::lang
