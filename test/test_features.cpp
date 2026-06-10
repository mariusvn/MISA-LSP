#include <catch2/catch_test_macros.hpp>
#include "lang/Compilation.h"
#include "features/Completion.h"
#include "features/Hover.h"

using namespace misa::lang;
using namespace misa::lsp;
using namespace misa::features;

static Compilation compile(const std::string& src) {
    return Compilation::build("test://t.mnemo", src);
}

// Find the cursor position marked by '|' in a single-line source, returning the
// source without the marker and the position.
static std::pair<std::string, Position> withCursor(const std::string& src) {
    auto pos = src.find('|');
    std::string clean = src.substr(0, pos) + src.substr(pos + 1);
    uint32_t line = 0, col = 0;
    for (size_t i = 0; i < pos; ++i) {
        if (clean[i] == '\n') { line++; col = 0; } else col++;
    }
    return {clean, Position{line, col}};
}

static bool hasLabel(const CompletionList& l, const std::string& label) {
    for (const auto& it : l.items) if (it.label == label) return true;
    return false;
}

// ── Completion context ────────────────────────────────────────────────────────

TEST_CASE("Completion: typing the mnemonic suggests instructions", "[completion]") {
    auto [src, pos] = withCursor(" st|");
    auto c = compile(src);
    auto list = provideCompletion(c, pos);
    REQUIRE(hasLabel(list, "ste"));
    REQUIRE(hasLabel(list, "str"));
}

TEST_CASE("Completion: first operand of ste suggests only types", "[completion]") {
    auto [src, pos] = withCursor("ste |");
    auto c = compile(src);
    auto list = provideCompletion(c, pos);
    REQUIRE(hasLabel(list, "u16t"));
    REQUIRE(hasLabel(list, "i8t"));
    // Must NOT suggest instructions / directives / entry points here.
    REQUIRE(!hasLabel(list, "abs"));
    REQUIRE(!hasLabel(list, "def"));
    REQUIRE(!hasLabel(list, "_draw"));
}

TEST_CASE("Completion: first operand of cmp suggests conditions", "[completion]") {
    auto [src, pos] = withCursor("cmp |");
    auto c = compile(src);
    auto list = provideCompletion(c, pos);
    REQUIRE(hasLabel(list, "eq"));
    REQUIRE(hasLabel(list, "flt"));
    REQUIRE(!hasLabel(list, "abs"));
}

TEST_CASE("Completion: syscall operand suggests SYS_ names", "[completion]") {
    auto [src, pos] = withCursor("syscall |");
    auto c = compile(src);
    auto list = provideCompletion(c, pos);
    REQUIRE(hasLabel(list, "SYS_PRINT_INT"));
    REQUIRE(!hasLabel(list, "add"));
}

TEST_CASE("Completion: register operand suggests registers, not types", "[completion]") {
    auto [src, pos] = withCursor("add t0, |");
    auto c = compile(src);
    auto list = provideCompletion(c, pos);
    REQUIRE(hasLabel(list, "t1"));
    REQUIRE(!hasLabel(list, "u16t"));
    REQUIRE(!hasLabel(list, "add"));
}

TEST_CASE("Completion: lod first operand excludes embed-only types", "[completion]") {
    auto [src, pos] = withCursor("lod |");
    auto c = compile(src);
    auto list = provideCompletion(c, pos);
    REQUIRE(hasLabel(list, "u32t"));
    REQUIRE(!hasLabel(list, "string"));  // embed-only
    REQUIRE(!hasLabel(list, "file"));
}

// ── Hover ─────────────────────────────────────────────────────────────────────

TEST_CASE("Hover: numeric literal does not resolve to a label", "[hover]") {
    // Hovering the literal 0 must not be matched against any label.
    auto [src, p] = withCursor("__L0:\n    mov s1, |0\n");
    auto c = compile(src);
    auto hv = provideHover(c, p);
    REQUIRE(!hv.has_value());
}

TEST_CASE("Hover: instruction shows documentation", "[hover]") {
    auto [src, p] = withCursor("    ad|d t0, t1\n");
    auto c = compile(src);
    auto hv = provideHover(c, p);
    REQUIRE(hv.has_value());
    REQUIRE(hv->contents.value.find("Add") != std::string::npos);
}

TEST_CASE("Hover: register shows ABI role", "[hover]") {
    auto [src, p] = withCursor("    mov t|0, 0\n");
    auto c = compile(src);
    auto hv = provideHover(c, p);
    REQUIRE(hv.has_value());
    REQUIRE(hv->contents.value.find("emporary") != std::string::npos);
}
