#pragma once
#include "lang/Token.h"
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace misa::lang {

// ── Expressions (assemble-time) ───────────────────────────────────────────────

struct IntLitExpr   { int64_t value; Span span; };
struct FloatLitExpr { double  value; Span span; };
struct StringExpr   { std::string value; Span span; };
struct IdentExpr    { std::string name; Span span; }; // label, constant, built-in, $

struct UnaryExpr;
struct BinaryExpr;
struct CastExpr;

using ExprNode = std::variant<
    IntLitExpr, FloatLitExpr, StringExpr, IdentExpr,
    std::unique_ptr<UnaryExpr>,
    std::unique_ptr<BinaryExpr>,
    std::unique_ptr<CastExpr>
>;

struct UnaryExpr  { Token op; ExprNode operand; Span span; };
struct BinaryExpr { Token op; ExprNode left, right; Span span; };
struct CastExpr   { bool toFloat; ExprNode operand; Span span; }; // icast or fcast

Span exprSpan(const ExprNode& e);

// ── Operand nodes (instruction arguments) ────────────────────────────────────

// A register range like s0..s2 or t4.. (open end)
struct RegRange { std::string regStart; std::optional<std::string> regEnd; Span span; };

// One parsed operand: a register name, range, or expression
struct OperandNode {
    enum class Kind { Register, Range, Expr };
    Kind             kind;
    std::string      regName;   // for Register
    RegRange         range;     // for Range
    ExprNode         expr;      // for Expr (also holds strings/labels)
    Span             span;
};

// ── Statements ────────────────────────────────────────────────────────────────

struct LabelDefStmt {
    enum class Kind { Global, Local, Reusable };
    Kind        kind;
    std::string name;   // without ':', '.' or '@'
    Span        span;
};

struct InstructionStmt {
    Token                    mnemonic;
    std::vector<OperandNode> operands;
    Span                     span;
};

struct DefDirective {
    std::string          name;   // constant name (without 'def ')
    bool                 isLocal;
    ExprNode             value;
    Span                 span;
};

struct UndefDirective {
    std::string name;
    Span        span;
};

struct EmbDirective {
    std::string              typeName;
    std::vector<ExprNode>    values;
    Span                     span;
};

struct ResDirective {
    std::string              typeName;
    ExprNode                 count;
    std::optional<ExprNode>  fill;
    Span                     span;
};

struct BmkDirective {
    bool        isSub;
    std::string label;
    Span        span;
};

struct DocCommentStmt {
    std::string text;
    Span        span;
};

struct EmptyStmt { Span span; };

using Statement = std::variant<
    LabelDefStmt,
    InstructionStmt,
    DefDirective,
    UndefDirective,
    EmbDirective,
    ResDirective,
    BmkDirective,
    DocCommentStmt,
    EmptyStmt
>;

Span stmtSpan(const Statement& s);

} // namespace misa::lang
