#include <catch2/catch_test_macros.hpp>
#include "lang/Lexer.h"
#include "lang/Token.h"

using namespace misa::lang;

static std::vector<Token> lex(const std::string& src) {
    Lexer l(src);
    auto toks = l.tokenize();
    // Remove trailing Eof
    if (!toks.empty() && toks.back().is(TokenType::Eof))
        toks.pop_back();
    return toks;
}

TEST_CASE("Lexer: empty source", "[lexer]") {
    auto t = lex("");
    REQUIRE(t.empty());
}

TEST_CASE("Lexer: simple integer literal", "[lexer]") {
    auto t = lex("42");
    REQUIRE(t.size() == 1);
    REQUIRE(t[0].type == TokenType::IntLit);
    REQUIRE(t[0].text == "42");
}

TEST_CASE("Lexer: hex literal", "[lexer]") {
    auto t = lex("0xFF");
    REQUIRE(t[0].type == TokenType::IntLit);
    REQUIRE(t[0].text == "0xFF");
}

TEST_CASE("Lexer: binary literal", "[lexer]") {
    auto t = lex("0b1010");
    REQUIRE(t[0].type == TokenType::IntLit);
    REQUIRE(t[0].text == "0b1010");
}

TEST_CASE("Lexer: float literal", "[lexer]") {
    auto t = lex("3.14");
    REQUIRE(t[0].type == TokenType::FloatLit);
    REQUIRE(t[0].text == "3.14");
}

TEST_CASE("Lexer: integer with underscores stripped", "[lexer]") {
    auto t = lex("10_000");
    REQUIRE(t[0].type == TokenType::IntLit);
    REQUIRE(t[0].text == "10000");
}

TEST_CASE("Lexer: identifier", "[lexer]") {
    auto t = lex("my_var");
    REQUIRE(t[0].type == TokenType::Ident);
    REQUIRE(t[0].text == "my_var");
}

TEST_CASE("Lexer: global label def", "[lexer]") {
    auto t = lex("foo:");
    REQUIRE(t[0].type == TokenType::LabelDef);
    REQUIRE(t[0].text == "foo:");
}

TEST_CASE("Lexer: local label def", "[lexer]") {
    auto t = lex(".bar:");
    REQUIRE(t[0].type == TokenType::LocalLabelDef);
}

TEST_CASE("Lexer: reusable label def", "[lexer]") {
    auto t = lex("@loop:");
    REQUIRE(t[0].type == TokenType::ReusableLabelDef);
}

TEST_CASE("Lexer: reusable ref up/down", "[lexer]") {
    auto t = lex("@loop-  @end+");
    REQUIRE(t[0].type == TokenType::ReusableDown);
    REQUIRE(t[1].type == TokenType::ReusableUp);
}

TEST_CASE("Lexer: line comment", "[lexer]") {
    auto t = lex("# this is a comment");
    REQUIRE(t[0].type == TokenType::Comment);
}

TEST_CASE("Lexer: doc comment", "[lexer]") {
    auto t = lex("## doc comment");
    REQUIRE(t[0].type == TokenType::DocComment);
}

TEST_CASE("Lexer: string literal", "[lexer]") {
    auto t = lex("\"Hello, World!\"");
    REQUIRE(t[0].type == TokenType::StringLit);
    REQUIRE(t[0].text == "Hello, World!");
}

TEST_CASE("Lexer: range operator", "[lexer]") {
    auto t = lex("s0..s2");
    REQUIRE(t[0].type == TokenType::Ident);
    REQUIRE(t[1].type == TokenType::DotDot);
    REQUIRE(t[2].type == TokenType::Ident);
}

TEST_CASE("Lexer: power operator", "[lexer]") {
    auto t = lex("**");
    REQUIRE(t[0].type == TokenType::StarStar);
}

TEST_CASE("Lexer: logical operators", "[lexer]") {
    auto t = lex("&& ||");
    REQUIRE(t[0].type == TokenType::AmpAmp);
    REQUIRE(t[1].type == TokenType::PipePipe);
}

TEST_CASE("Lexer: newline token", "[lexer]") {
    auto t = lex("a\nb");
    REQUIRE(t[1].type == TokenType::Newline);
}

TEST_CASE("Lexer: instruction with operands", "[lexer]") {
    auto t = lex("add t0, t1, 42");
    REQUIRE(t[0].text == "add");
    REQUIRE(t[0].type == TokenType::Ident);
    REQUIRE(t[1].text == "t0");
    REQUIRE(t[2].type == TokenType::Comma);
    REQUIRE(t[5].type == TokenType::IntLit);
}

TEST_CASE("Lexer: dollar sign", "[lexer]") {
    auto t = lex("$");
    REQUIRE(t[0].type == TokenType::Dollar);
}
