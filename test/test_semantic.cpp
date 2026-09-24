#include <catch2/catch_test_macros.hpp>
#include "lang/Compilation.h"

using namespace misa::lang;
using namespace misa::lsp;

static Compilation compile(const std::string& src) {
    return Compilation::build("test://test.mnemo", src);
}

static bool hasDiag(const Compilation& c, DiagnosticSeverity sev, const std::string& substr) {
    for (const auto& d : c.root().diagnostics) {
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
    REQUIRE(c.root().diagnostics.empty());
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
    for (const auto& d : c.root().diagnostics)
        if (d.severity == DiagnosticSeverity::Hint) hasHint = true;
    REQUIRE(!hasHint);
}

TEST_CASE("Semantic: unknown syscall -> error", "[semantic]") {
    auto c = compile("syscall SYS_NONEXISTENT_CALL");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "Unknown syscall"));
}

TEST_CASE("Semantic: valid syscall -> no error", "[semantic]") {
    auto c = compile("syscall SYS_PRINT_INT");
    REQUIRE(c.root().diagnostics.empty());
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
    REQUIRE(c.root().diagnostics.empty());
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

// ── Manual v0.1.6 additions ───────────────────────────────────────────────────

TEST_CASE("Semantic: new instructions, condition, syscalls and built-ins", "[semantic][v016]") {
    const std::string src = R"(
_start:
    fma t0, t1, t2, t3
    fma t0, t1, t2
    divu t0, t1
    mlhu t0, t1, t2
    clpu t0, 0, 10
    cmp gtu, t0, t1
    jmpa _start
    cala _start
    jtra _start
    jfsa _start
    syscall SYS_PRINT_LINE_STRING
    syscall SYS_GET_KEYBOARD_INPUT
    syscall SYS_READ_TERMINAL_INPUT
    syscall SYS_ALLOW_UNSAFE_JUMP
    and t0, a1, KBE_SHIFT
    mov t1, KEY_ENTER
    mov t2, MOUSE_BTN_LEFT
    mov t3, MAX_TERMINAL_INPUT_SIZE
    mov t4, 'misa'
    exit
_keyboard_input:
    exit
)";
    auto c = compile(src);
    REQUIRE(c.root().diagnostics.empty());
}

TEST_CASE("Semantic: new entry points get the exit hint", "[semantic][v016]") {
    auto c = compile("_terminal_input:\n    nop\n");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Hint, "_terminal_input"));
}

TEST_CASE("Semantic: undef removes a constant", "[semantic]") {
    auto c = compile("def K 1\nundef K\n_start:\n    mov t0, K\n    exit\n");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "Unknown identifier 'K'"));
}

TEST_CASE("Semantic: constants must be defined before use", "[semantic]") {
    auto c = compile("_start:\n    mov t0, K\n    exit\ndef K 1\n");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "Unknown identifier 'K'"));
}

TEST_CASE("Semantic: forward reference to a qualified local label", "[semantic]") {
    auto c = compile("_start:\n    jmp DATA.end\n    exit\nDATA:\n.end:\n    exit\n");
    REQUIRE(!hasDiag(c, DiagnosticSeverity::Error, "Unknown identifier"));
}

TEST_CASE("Semantic: reusable labels resolve to the right definition", "[semantic]") {
    auto c = compile("f:\n@l:\n    jmp @l+\n@l:\n    jmp @l-\n    jmp @x-\n    exit\n");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "No '@x' label above"));
    const auto& refs = c.symbols.references();
    // @l+ on line 2 → the definition on line 3; @l- on line 4 → line 3 too.
    REQUIRE(refs[0].target >= 0);
    REQUIRE(refs[1].target >= 0);
    REQUIRE(c.symbols.definitions()[refs[0].target].range.start.line == 3);
    REQUIRE(c.symbols.definitions()[refs[1].target].range.start.line == 3);
}

TEST_CASE("Semantic: unknown identifiers inside expressions and data", "[semantic]") {
    auto c = compile("_start:\n    mov t0, (NOPE + 1)\n    exit\nD: emb u32t OTHER\nR: res u8t SIZE\n");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "Unknown identifier 'NOPE'"));
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "Unknown identifier 'OTHER'"));
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "Unknown identifier 'SIZE'"));
}

TEST_CASE("Semantic: immediates cannot be destinations", "[semantic]") {
    auto c = compile("_start:\n    mov 42, t0\n    exit\n");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "Destination must be a register"));
}

TEST_CASE("Semantic: res only accepts scalar types", "[semantic]") {
    auto c = compile("B: res string 4\n");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "scalar"));
}

TEST_CASE("Semantic: stray tokens after a statement", "[semantic]") {
    auto c = compile("_start:\n    mov t0, 1 2\n    exit\n");
    REQUIRE(hasDiag(c, DiagnosticSeverity::Error, "expected end of line"));
}

TEST_CASE("Semantic: constant values are evaluated", "[semantic]") {
    auto c = compile("def A 'ab'\ndef B (A << 8) | 1\ndef C fcast 3 / 2\n");
    REQUIRE(c.symbols.find("A")->constValue == 0x6162);
    REQUIRE(c.symbols.find("B")->constValue == 0x616201);
    REQUIRE(c.symbols.find("C")->isFloat);
    REQUIRE(c.symbols.find("C")->constValue == 1.5);
}
