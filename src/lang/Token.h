#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace misa::lang {

// Byte-offset span in the original source text.
struct Span {
    uint32_t start = 0;
    uint32_t end   = 0; // exclusive
    uint32_t length() const { return end - start; }
};

enum class TokenType : uint8_t {
    // Structural
    Ident,          // identifier, mnemonic, or keyword (not yet classified)
    LabelDef,       // name:
    LocalLabelDef,  // .name:
    ReusableLabelDef, // @name:
    LocalIdent,     // .name (reference)
    ReusableRef,    // @name without :
    ReusableUp,     // @name+
    ReusableDown,   // @name-

    // Literals
    IntLit,    // decimal / hex / bin / oct
    FloatLit,  // 1.0, 3.14
    StringLit, // "..."

    // Register range syntax
    DotDot,    // ..

    // Operators (expression)
    Plus,      // +
    Minus,     // -
    Star,      // *
    Slash,     // /
    Percent,   // %
    StarStar,  // **
    PercentPercent, // %%
    Tilde,     // ~
    Amp,       // &
    Pipe,      // |
    Caret,     // ^
    LtLt,      // <<
    GtGt,      // >>
    Bang,      // !
    AmpAmp,    // &&
    PipePipe,  // ||
    EqEq,      // ==
    BangEq,    // !=
    Lt,        // <
    Gt,        // >
    LtEq,      // <=
    GtEq,      // >=

    // Punctuation
    LParen,    // (
    RParen,    // )
    Comma,     // ,

    // Comments
    Comment,   // # text
    DocComment, // ## text

    // Special
    Dollar,    // $ (current address)
    Newline,
    Eof,
    Error,     // unexpected character
};

struct Token {
    TokenType   type;
    std::string text; // lexeme (normalised: no underscores in numbers)
    Span        span;

    bool is(TokenType t) const { return type == t; }
};

} // namespace misa::lang
