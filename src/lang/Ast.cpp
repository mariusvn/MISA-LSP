#include "lang/Ast.h"

namespace misa::lang {

// exprSpan defined in ExprParser.cpp (where ExprNode variants are known).
// stmtSpan: thin visitor that returns the span field.
Span stmtSpan(const Statement& s) {
    return std::visit([](const auto& node) -> Span {
        return node.span;
    }, s);
}

} // namespace misa::lang
