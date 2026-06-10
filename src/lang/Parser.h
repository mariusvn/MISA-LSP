#pragma once
#include "lang/Ast.h"
#include "lang/Token.h"
#include <vector>

namespace misa::lang {

// Line-oriented MISA parser.
// Each non-empty logical line produces one Statement.
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    std::vector<Statement> parse();

private:
    std::vector<Token> m_tokens;
    size_t             m_pos = 0;

    const Token& current() const;
    const Token& peek(int offset = 0) const;
    const Token& consume();
    bool atEnd() const;

    // Advance past Newline/Comment/DocComment tokens, return true if on a new line.
    void skipNewlines();
    // Consume everything until the next Newline/Eof (error recovery).
    void skipToNewline();

    Statement parseLine();

    Statement parseLabelDef(TokenType kind);
    Statement parseInstruction(Token mnemonic);
    Statement parseDef();
    Statement parseUndef();
    Statement parseEmb();
    Statement parseRes();
    Statement parseBmk(bool isSub);

    OperandNode parseOperand();
    std::vector<OperandNode> parseOperandList();
    ExprNode parseExprInLine();
};

} // namespace misa::lang
