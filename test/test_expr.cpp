#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "lang/Lexer.h"
#include "lang/ExprParser.h"
#include <span>

using namespace misa::lang;

static ExprNode parseExpr(const std::string& src) {
    Lexer lexer(src);
    auto tokens = lexer.tokenize();
    std::span<const Token> view(tokens.data(), tokens.size());
    ExprParser ep(view, 0);
    return ep.parseExpr();
}

static int64_t asInt(const ExprNode& e) {
    return std::get<IntLitExpr>(e).value;
}
static double asFloat(const ExprNode& e) {
    return std::get<FloatLitExpr>(e).value;
}

TEST_CASE("ExprParser: integer literal", "[expr]") {
    auto e = parseExpr("42");
    REQUIRE(asInt(e) == 42);
}

TEST_CASE("ExprParser: hex literal", "[expr]") {
    auto e = parseExpr("0x10");
    REQUIRE(asInt(e) == 16);
}

TEST_CASE("ExprParser: float literal", "[expr]") {
    auto e = parseExpr("3.14");
    REQUIRE_THAT(asFloat(e), Catch::Matchers::WithinRel(3.14, 1e-6));
}

TEST_CASE("ExprParser: identifier", "[expr]") {
    auto e = parseExpr("my_const");
    REQUIRE(std::get<IdentExpr>(e).name == "my_const");
}

TEST_CASE("ExprParser: dollar sign", "[expr]") {
    auto e = parseExpr("$");
    REQUIRE(std::get<IdentExpr>(e).name == "$");
}

TEST_CASE("ExprParser: unary negation", "[expr]") {
    auto e = parseExpr("-1");
    auto& u = *std::get<std::unique_ptr<UnaryExpr>>(e);
    REQUIRE(u.op.type == TokenType::Minus);
    REQUIRE(asInt(u.operand) == 1);
}

TEST_CASE("ExprParser: binary addition", "[expr]") {
    auto e = parseExpr("1 + 2");
    auto& b = *std::get<std::unique_ptr<BinaryExpr>>(e);
    REQUIRE(b.op.type == TokenType::Plus);
    REQUIRE(asInt(b.left) == 1);
    REQUIRE(asInt(b.right) == 2);
}

TEST_CASE("ExprParser: precedence: * over +", "[expr]") {
    // 2 + 3 * 4  →  BinExpr(+, 2, BinExpr(*, 3, 4))
    auto e = parseExpr("2 + 3 * 4");
    auto& b = *std::get<std::unique_ptr<BinaryExpr>>(e);
    REQUIRE(b.op.type == TokenType::Plus);
    REQUIRE(asInt(b.left) == 2);
    auto& r = *std::get<std::unique_ptr<BinaryExpr>>(b.right);
    REQUIRE(r.op.type == TokenType::Star);
}

TEST_CASE("ExprParser: parentheses override precedence", "[expr]") {
    // (2 + 3) * 4  →  BinExpr(*, BinExpr(+,2,3), 4)
    auto e = parseExpr("(2 + 3) * 4");
    auto& b = *std::get<std::unique_ptr<BinaryExpr>>(e);
    REQUIRE(b.op.type == TokenType::Star);
}

TEST_CASE("ExprParser: icast", "[expr]") {
    auto e = parseExpr("icast 3.14");
    auto& c = *std::get<std::unique_ptr<CastExpr>>(e);
    REQUIRE(c.toFloat == false);
}

TEST_CASE("ExprParser: fcast", "[expr]") {
    auto e = parseExpr("fcast 42");
    auto& c = *std::get<std::unique_ptr<CastExpr>>(e);
    REQUIRE(c.toFloat == true);
}

TEST_CASE("ExprParser: bitwise operators", "[expr]") {
    auto e = parseExpr("a & b");
    auto& b = *std::get<std::unique_ptr<BinaryExpr>>(e);
    REQUIRE(b.op.type == TokenType::Amp);
}

TEST_CASE("ExprParser: shift operators", "[expr]") {
    auto e = parseExpr("1 << 4");
    auto& b = *std::get<std::unique_ptr<BinaryExpr>>(e);
    REQUIRE(b.op.type == TokenType::LtLt);
}

TEST_CASE("ExprParser: power operator", "[expr]") {
    auto e = parseExpr("2 ** 8");
    auto& b = *std::get<std::unique_ptr<BinaryExpr>>(e);
    REQUIRE(b.op.type == TokenType::StarStar);
}
