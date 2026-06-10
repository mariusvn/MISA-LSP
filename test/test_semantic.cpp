#include <catch2/catch_test_macros.hpp>
#include "lang/Compilation.h"

using namespace misa::lang;
using namespace misa::lsp;

static Compilation compile(const std::string& src) {
    return Compilation::build("test://test.mnemo", src);
}

static bool hasDiag(const Compilation& c, DiagnosticSeverity sev, const std::string& substr) {
    for (const auto& d : c.diagnostics) {
        if (d.severity == sev && d.message.find(substr) != std::string::npos)
            return true;
    }
    return false;
}

TEST_CASE("Semantic: unknown instruction produces error", "[semantic]") {
    auto c = compile("frobnicatex t0, t1");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "Unknown instruction"));
}

TEST_CASE("Semantic: valid instruction no error", "[semantic]") {
    auto c = compile("add t0, t1, t2");
    REQUIRE(c.diagnostics.empty());
}

TEST_CASE("Semantic: float literal in int position", "[semantic]") {
    auto c = compile("add t0, t1, 1.0");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "Float literal"));
}

TEST_CASE("Semantic: int literal in float position", "[semantic]") {
    auto c = compile("fadd t0, t1, 1");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "Integer literal"));
}

TEST_CASE("Semantic: global labels collected", "[semantic]") {
    auto c = compile("foo:\n    exit\nbar:\n    exit\n");
    REQUIRE(c.symbols.find("foo") != nullptr);
    REQUIRE(c.symbols.find("bar") != nullptr);
}

TEST_CASE("Semantic: local label scoped to parent", "[semantic]") {
    auto c = compile("foo:\n.local:\n    exit\n");
    REQUIRE(c.symbols.find("foo.local") != nullptr);
}

TEST_CASE("Semantic: local label without global parent -> error", "[semantic]") {
    auto c = compile(".local:\n    exit\n");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "no enclosing global label"));
}

TEST_CASE("Semantic: write to read-only pc -> warning", "[semantic]") {
    auto c = compile("mov pc, t0");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Warning, "read-only"));
}

TEST_CASE("Semantic: entry point without exit -> hint", "[semantic]") {
    auto c = compile("_start:\n    mov t0, 0\n");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Hint, "missing a final 'exit'"));
}

TEST_CASE("Semantic: entry point with exit -> no hint", "[semantic]") {
    auto c = compile("_start:\n    mov t0, 0\n    exit\n");
    bool hasHint = false;
    for (const auto& d : c.diagnostics)
        if (d.severity == DiagnosticSeverity::Hint) hasHint = true;
    REQUIRE(!hasHint);
}

TEST_CASE("Semantic: unknown syscall -> error", "[semantic]") {
    auto c = compile("syscall SYS_NONEXISTENT_CALL");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "Unknown syscall"));
}

TEST_CASE("Semantic: valid syscall -> no error", "[semantic]") {
    auto c = compile("syscall SYS_PRINT_INT");
    REQUIRE(c.diagnostics.empty());
}

TEST_CASE("Semantic: wrong arity -> error", "[semantic]") {
    auto c = compile("add t0");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "expects"));
}

TEST_CASE("Semantic: type keyword in lod is not an unknown identifier", "[semantic]") {
    auto c = compile("lab: emb u16t 0\nfoo:\n    lod u16t, a0, lab\n");
    REQUIRE(!hasDiag(c, DiagnosticSeverity::Error, "Unknown identifier"));
    REQUIRE(!hasDiag(c, DiagnosticSeverity::Error, "Expected a type"));
}

TEST_CASE("Semantic: bad type keyword in lod -> error", "[semantic]") {
    auto c = compile("foo:\n    lod u17t, a0, foo\n");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "Expected a type"));
}

TEST_CASE("Semantic: condition keyword in cmp is not an unknown identifier", "[semantic]") {
    auto c = compile("cmp eq, s4, 0");
    REQUIRE(!hasDiag(c, DiagnosticSeverity::Error, "Unknown identifier"));
    REQUIRE(c.diagnostics.empty());
}

TEST_CASE("Semantic: bad condition keyword in cmp -> error", "[semantic]") {
    auto c = compile("cmp zz, s4, 0");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "Expected a condition"));
}

TEST_CASE("Semantic: qualified name resolves as reference", "[semantic]") {
    const std::string src = "PRINTER:\n.MAPPING: emb string \"abc\"\n    mov s2, PRINTER.MAPPING\n";
    auto c = compile(src);
    REQUIRE(c.symbols.find("PRINTER.MAPPING") != nullptr);
    REQUIRE(!hasDiag(c, DiagnosticSeverity::Error, "Unknown identifier"));
}

TEST_CASE("Semantic: compound expression operand parses fully (no arity error)", "[semantic]") {
    const std::string src =
        "A:\n.X: emb u32t 0\nB:\ndef .Y 4\nfoo:\n"
        "    str u32t, A.X + B.Y, t0\n";
    auto c = compile(src);
    REQUIRE(!hasDiag(c, DiagnosticSeverity::Error, "expects"));
    REQUIRE(!hasDiag(c, DiagnosticSeverity::Error, "Unknown identifier"));
}

TEST_CASE("Semantic: reusable label reference is not an unknown identifier", "[semantic]") {
    const std::string src =
        "foo:\n@loop:\n    add t0, 1\n    jmp @loop-\n    jmp @end+\n@end:\n    exit\n";
    auto c = compile(src);
    REQUIRE(!hasDiag(c, DiagnosticSeverity::Error, "Unknown identifier"));
}

TEST_CASE("Semantic: symbols in document", "[semantic]") {
    const std::string src = R"(
_start:
    mov t0, 0
    exit
_update:
    exit
)";
    auto c = compile(src);
    REQUIRE(c.symbols.find("_start")  != nullptr);
    REQUIRE(c.symbols.find("_update") != nullptr);
}
