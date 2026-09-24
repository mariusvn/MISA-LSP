#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include "features/Definition.h"
#include "features/DocumentLinks.h"
#include "features/DocumentSymbols.h"
#include "features/Hover.h"
#include "features/References.h"
#include "fs/SourceProvider.h"
#include "lang/Compilation.h"

using namespace misa;
using namespace misa::lang;
using namespace misa::lsp;

namespace {

struct Project {
    fs::InMemorySourceProvider files;
    fs::PathConfig             paths{"/u", "/s"};

    explicit Project(bool caseInsensitive = false) : files(caseInsensitive) {}

    Project& add(const std::string& path, const std::string& text) {
        files.set(path, text);
        return *this;
    }
    Compilation build(const std::string& root = "/p/main.asm") const {
        Compilation::UnitOptions o;
        o.sources = &files;
        o.paths   = paths;
        return Compilation::buildUnit(root, o);
    }
};

bool hasDiag(const SourceFile& f, DiagnosticSeverity sev, const std::string& substr) {
    for (const auto& d : f.diagnostics)
        if (d.severity == sev && d.message.find(substr) != std::string::npos) return true;
    return false;
}

size_t errorCount(const SourceFile& f) {
    size_t n = 0;
    for (const auto& d : f.diagnostics) n += d.severity == DiagnosticSeverity::Error;
    return n;
}

const SourceFile& fileNamed(const Compilation& c, const char* path) {
    const SourceFile* f = c.findByKey(path);
    REQUIRE(f != nullptr);
    return *f;
}

} // namespace

TEST_CASE("Include: expands the file at the directive position", "[include]") {
    Project p;
    p.add("/p/main.asm", "_start:\n    mov a0, B\ninclude \"lib.asm\"\n    exit\n")
     .add("/p/lib.asm", "B: emb u32t 1\n");
    auto c = p.build();

    REQUIRE(c.files.size() == 2);
    REQUIRE(c.files[1].path == "/p/lib.asm");
    REQUIRE(c.symbols.find("B") != nullptr);
    REQUIRE(c.symbols.find("B")->file == 1);
    REQUIRE(errorCount(c.root()) == 0);

    // Stream order: main's first 3 statements, then lib, then the rest of main.
    REQUIRE(c.order[2].file == 0);
    REQUIRE(c.order[3].file == 1);
    REQUIRE(c.order.back().file == 0);
}

TEST_CASE("Include: constants must be defined before use across files", "[include]") {
    Project p;
    p.add("/p/main.asm", "_start:\n    mov a0, K\ninclude \"k.asm\"\n    mov a1, K\n    exit\n")
     .add("/p/k.asm", "def K 7\n");
    auto c = p.build();

    REQUIRE(hasDiag(c.root(), DiagnosticSeverity::Error, "Unknown identifier 'K'"));
    REQUIRE(errorCount(c.root()) == 1); // only the use before the include
    REQUIRE(c.files[1].diagnostics.empty());
}

TEST_CASE("Include: local labels attach to the includer's current global", "[include]") {
    Project p;
    p.add("/p/main.asm", "STRUCT:\ninclude \"fields.asm\"\n_start:\n    mov a0, STRUCT.x\n    exit\n")
     .add("/p/fields.asm", ".x: emb u32t 1\n");
    auto c = p.build();

    const auto* x = c.symbols.find("STRUCT.x");
    REQUIRE(x != nullptr);
    REQUIRE(x->file == 1);
    REQUIRE(errorCount(c.root()) == 0);
}

TEST_CASE("Include: same file twice is included once", "[include]") {
    Project p;
    p.add("/p/main.asm", "include \"lib.asm\"\ninclude \"./sub/../lib.asm\"\n")
     .add("/p/lib.asm", "F:\n    ret\n");
    auto c = p.build();

    REQUIRE(c.files.size() == 2);
    REQUIRE(c.root().includes.size() == 2);
    REQUIRE(c.root().includes[1].status == IncludeRecord::Status::AlreadyIncluded);
    REQUIRE(hasDiag(c.root(), DiagnosticSeverity::Hint, "already included"));
    REQUIRE_FALSE(hasDiag(c.root(), DiagnosticSeverity::Error, "Duplicate"));
}

TEST_CASE("Include: cycles terminate", "[include]") {
    Project p;
    p.add("/p/main.asm", "include \"b.asm\"\nA:\n    ret\n")
     .add("/p/b.asm", "include \"main.asm\"\nB:\n    ret\n");
    auto c = p.build();

    REQUIRE(c.files.size() == 2);
    const auto& b = fileNamed(c, "/p/b.asm");
    REQUIRE(b.includes[0].status == IncludeRecord::Status::Cycle);
    REQUIRE(hasDiag(b, DiagnosticSeverity::Hint, "Circular include"));
}

TEST_CASE("Include: diamond includes the shared file at its first position", "[include]") {
    Project p;
    p.add("/p/main.asm", "include \"b.asm\"\ninclude \"c.asm\"\n")
     .add("/p/b.asm", "include \"d.asm\"\nB:\n    ret\n")
     .add("/p/c.asm", "include \"d.asm\"\nC:\n    ret\n")
     .add("/p/d.asm", "D:\n    ret\n");
    auto c = p.build();

    REQUIRE(c.files.size() == 4);
    REQUIRE(c.files[2].path == "/p/d.asm"); // expanded inside b.asm, before c.asm
    REQUIRE(fileNamed(c, "/p/c.asm").includes[0].status == IncludeRecord::Status::AlreadyIncluded);
}

TEST_CASE("Include: relative paths resolve from the including file", "[include]") {
    Project p;
    p.add("/p/main.asm", "include \"lib/a.asm\"\n")
     .add("/p/lib/a.asm", "include \"b.asm\"\n")      // → /p/lib/b.asm
     .add("/p/lib/b.asm", "B:\n    ret\n");
    auto c = p.build();
    REQUIRE(c.files.size() == 3);
    REQUIRE(c.symbols.find("B") != nullptr);
}

TEST_CASE("Include: virtual folders", "[include]") {
    Project p;
    p.add("/p/main.asm", "include \"@u/gfx/main.asm\"\ninclude \"@s/lander/util.asm\"\n")
     .add("/u/gfx/main.asm", "GFX:\n    ret\n")
     .add("/s/lander/util.asm", "UTIL:\n    ret\n");
    auto c = p.build();
    REQUIRE(c.files.size() == 3);
    REQUIRE(c.symbols.find("GFX") != nullptr);
    REQUIRE(c.symbols.find("UTIL") != nullptr);

    p.paths = {};
    auto c2 = p.build();
    REQUIRE(hasDiag(c2.root(), DiagnosticSeverity::Error, "@u/"));
}

TEST_CASE("Include: missing file is an error on the path", "[include]") {
    Project p;
    p.add("/p/main.asm", "include \"nope.asm\"\n");
    auto c = p.build();
    REQUIRE(hasDiag(c.root(), DiagnosticSeverity::Error, "Cannot open included file"));
    REQUIRE(c.root().diagnostics[0].range.start.character == 8);
}

TEST_CASE("Include: errors in an included file are summarised on the include", "[include]") {
    Project p;
    p.add("/p/main.asm", "include \"lib.asm\"\n")
     .add("/p/lib.asm", "F:\n    frob t0\n");
    auto c = p.build();
    REQUIRE(hasDiag(c.files[1], DiagnosticSeverity::Error, "Unknown instruction 'frob'"));
    REQUIRE(c.files[1].diagnostics[0].range.start.line == 1);
    REQUIRE(hasDiag(c.root(), DiagnosticSeverity::Warning, "'lib.asm' has 1 error(s)"));
}

TEST_CASE("Include: .misa files are MISA sources", "[include]") {
    Project p;
    p.add("/p/main.asm", "include \"lib.misa\"\ninclude \"notes.txt\"\n")
     .add("/p/lib.misa", "F:\n    ret\n")
     .add("/p/notes.txt", "");
    auto c = p.build();
    REQUIRE(c.files.size() == 3);
    REQUIRE(c.root().includes[0].status == IncludeRecord::Status::Ok);
    size_t warnings = 0;
    for (const auto& d : c.root().diagnostics)
        if (d.severity == DiagnosticSeverity::Warning &&
            d.message.find("expected to be source files") != std::string::npos) ++warnings;
    REQUIRE(warnings == 1); // only notes.txt
}

TEST_CASE("Include: case-insensitive file systems see one file", "[include]") {
    Project p(/*caseInsensitive*/ true);
    p.add("/p/main.asm", "include \"LIB.ASM\"\ninclude \"lib.asm\"\n")
     .add("/p/lib.asm", "F:\n    ret\n");
    auto c = p.build();
    REQUIRE(c.files.size() == 2);
    REQUIRE(c.root().includes[1].status == IncludeRecord::Status::AlreadyIncluded);
}

TEST_CASE("Include: paths are verbatim (backslashes are not escapes)", "[include]") {
    Project p;
    p.add("/p/main.asm", "include \"lib\\new.asm\"\n")
     .add("/p/lib/new.asm", "N:\n    ret\n");
    auto c = p.build();
    REQUIRE(c.root().diagnostics.empty());
    REQUIRE(c.files.size() == 2);
}

TEST_CASE("Include: missing path is a syntax error", "[include]") {
    auto c = Compilation::build("test://t.asm", "include\n");
    REQUIRE(hasDiag(c.root(), DiagnosticSeverity::Error, "expects a quoted path"));
}

TEST_CASE("Include: reusable labels resolve across files", "[include]") {
    Project p;
    p.add("/p/main.asm", "_start:\n@loop:\ninclude \"body.asm\"\n    exit\n")
     .add("/p/body.asm", "    jmp @loop-\n");
    auto c = p.build();
    REQUIRE(errorCount(c.files[1]) == 0);
    const auto& refs = c.symbols.references();
    auto it = std::find_if(refs.begin(), refs.end(), [](const SymbolRef& r) { return r.name == "@loop"; });
    REQUIRE(it != refs.end());
    REQUIRE(it->target >= 0);
    REQUIRE(c.symbols.definitions()[it->target].file == 0);
}

TEST_CASE("Include: emb file paths", "[include]") {
    Project p;
    p.add("/p/main.asm",
          "A: emb file \"assets/a.png\"\nB: emb file \"assets/missing.png\"\nC: emb file \"x.txt\"\n")
     .add("/p/assets/a.png", "PNG");
    auto c = p.build();
    REQUIRE(c.root().embeddedFiles.size() == 3);
    REQUIRE(c.root().embeddedFiles[0].exists);
    REQUIRE(hasDiag(c.root(), DiagnosticSeverity::Warning, "File not found"));
    REQUIRE(hasDiag(c.root(), DiagnosticSeverity::Error, "Only .png and .bin"));
}

// ── Features across files ─────────────────────────────────────────────────────

TEST_CASE("Include: go to definition jumps into the included file", "[include][features]") {
    Project p;
    p.add("/p/main.asm", "include \"lib.asm\"\n_start:\n    cal helper\n    exit\n")
     .add("/p/lib.asm", "helper:\n    ret\n");
    auto c = p.build();

    auto locs = features::provideDefinition(c, c.root(), Position{2, 9});
    REQUIRE(locs.size() == 1);
    REQUIRE(locs[0].uri == c.files[1].uri);
    REQUIRE(locs[0].range.start.line == 0);

    // On the include path itself: the file.
    auto fileLoc = features::provideDefinition(c, c.root(), Position{0, 11});
    REQUIRE(fileLoc.size() == 1);
    REQUIRE(fileLoc[0].uri == c.files[1].uri);
}

TEST_CASE("Include: references span every file of the unit", "[include][features]") {
    Project p;
    p.add("/p/main.asm", "include \"lib.asm\"\n_start:\n    cal helper\n    exit\n")
     .add("/p/lib.asm", "helper:\n    ret\nagain:\n    jmp helper\n");
    auto c = p.build();

    auto refs = features::provideReferences(c, c.files[1], Position{0, 2}, /*includeDeclaration*/ true);
    REQUIRE(refs.size() == 3);
    size_t inMain = std::count_if(refs.begin(), refs.end(),
                                  [&](const Location& l) { return l.uri == c.root().uri; });
    REQUIRE(inMain == 1);
}

TEST_CASE("Include: outline only lists the file's own symbols", "[include][features]") {
    Project p;
    p.add("/p/main.asm", "include \"lib.asm\"\n_start:\n    exit\n")
     .add("/p/lib.asm", "helper:\n    ret\n");
    auto c = p.build();

    auto rootSyms = features::provideDocumentSymbols(c, c.root());
    REQUIRE(rootSyms.size() == 1);
    REQUIRE(rootSyms[0].name == "_start");
    auto libSyms = features::provideDocumentSymbols(c, c.files[1]);
    REQUIRE(libSyms.size() == 1);
    REQUIRE(libSyms[0].name == "helper");
}

TEST_CASE("Include: document links point at the included file", "[include][features]") {
    Project p;
    p.add("/p/main.asm", "include \"lib.asm\"\ninclude \"nope.asm\"\n")
     .add("/p/lib.asm", "");
    auto c = p.build();
    auto links = features::provideDocumentLinks(c, c.root());
    REQUIRE(links.size() == 1);
    REQUIRE(links[0].target == c.files[1].uri);
    REQUIRE(links[0].range.start.character == 9); // inside the quotes
    REQUIRE(links[0].range.end.character == 16);
}

TEST_CASE("Include: hover names the defining file", "[include][features]") {
    Project p;
    p.add("/p/main.asm", "include \"lib/k.asm\"\n_start:\n    mov a0, SIZE\n    exit\n")
     .add("/p/lib/k.asm", "## Buffer size.\ndef SIZE 0x40\n");
    auto c = p.build();
    auto hv = features::provideHover(c, c.root(), Position{2, 13});
    REQUIRE(hv.has_value());
    REQUIRE(hv->contents.value.find("Defined in `lib/k.asm`") != std::string::npos);
    REQUIRE(hv->contents.value.find("Value: `64`") != std::string::npos);
    REQUIRE(hv->contents.value.find("Buffer size.") != std::string::npos);
}
