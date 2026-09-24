#include <catch2/catch_test_macros.hpp>
#include "fs/SourceProvider.h"
#include "server/Workspace.h"

using namespace misa;
using namespace misa::server;
using namespace misa::lsp;

namespace {

// Workspace over an in-memory "disk" that the test can still modify.
struct Fixture {
    fs::InMemorySourceProvider* disk; // owned by the workspace
    Workspace ws;

    Fixture() : Fixture(new fs::InMemorySourceProvider()) {}
    explicit Fixture(fs::InMemorySourceProvider* d)
        : disk(d), ws(std::unique_ptr<fs::SourceProvider>(d)) {
        ws.setPathConfig({"/u", "/s"});
        ws.takePendingDiagnostics();
    }

    static std::string uri(const std::string& path) { return fs::pathToUri(path); }

    std::string rootOf(const std::string& path) const {
        auto t = ws.lookup(uri(path));
        REQUIRE(t.has_value());
        return t->unit->root().path;
    }

    size_t errors(const std::string& path) const {
        auto t = ws.lookup(uri(path));
        REQUIRE(t.has_value());
        size_t n = 0;
        for (const auto& d : t->file->diagnostics) n += d.severity == DiagnosticSeverity::Error;
        return n;
    }
};

const Workspace::Publish* findPublish(const std::vector<Workspace::Publish>& list, const std::string& uri) {
    for (const auto& p : list) if (p.uri == uri) return &p;
    return nullptr;
}

} // namespace

TEST_CASE("Workspace: a library opened alone is analysed from the project's main.asm", "[workspace]") {
    Fixture fx;
    fx.disk->set("/p/project.mnemonimov", "{}");
    fx.disk->set("/p/main.asm", "def WIDTH 10\ninclude \"lib/u.asm\"\n");
    fx.disk->set("/p/lib/u.asm", "draw:\n    mov a0, WIDTH\n    ret\n");

    fx.ws.open(Fixture::uri("/p/lib/u.asm"), "draw:\n    mov a0, WIDTH\n    ret\n");
    REQUIRE(fx.rootOf("/p/lib/u.asm") == "/p/main.asm");
    REQUIRE(fx.errors("/p/lib/u.asm") == 0); // WIDTH comes from main.asm
}

TEST_CASE("Workspace: a project file not included by main.asm is its own root", "[workspace]") {
    Fixture fx;
    fx.disk->set("/p/project.mnemonimov", "{}");
    fx.disk->set("/p/main.asm", "_start:\n    exit\n");
    fx.disk->set("/p/lib/other.asm", "x:\n    ret\n");

    fx.ws.open(Fixture::uri("/p/lib/other.asm"), "x:\n    ret\n");
    REQUIRE(fx.rootOf("/p/lib/other.asm") == "/p/lib/other.asm");
}

TEST_CASE("Workspace: files outside a project are their own root", "[workspace]") {
    Fixture fx;
    fx.ws.open(Fixture::uri("/loose/a.asm"), "_start:\n    exit\n");
    REQUIRE(fx.rootOf("/loose/a.asm") == "/loose/a.asm");
}

TEST_CASE("Workspace: open buffers win over the disk", "[workspace]") {
    Fixture fx;
    fx.disk->set("/p/main.asm", "include \"lib.asm\"\n_start:\n    mov a0, X\n    exit\n");
    fx.disk->set("/p/lib.asm", "");                       // X not defined on disk

    fx.ws.open(Fixture::uri("/p/main.asm"), "include \"lib.asm\"\n_start:\n    mov a0, X\n    exit\n");
    REQUIRE(fx.errors("/p/main.asm") == 1);

    fx.ws.open(Fixture::uri("/p/lib.asm"), "X: emb u32t 1\n");  // unsaved edit defines X
    REQUIRE(fx.errors("/p/main.asm") == 0);
}

TEST_CASE("Workspace: editing an included file republishes its includer", "[workspace]") {
    Fixture fx;
    fx.disk->set("/p/main.asm", "include \"lib.asm\"\n_start:\n    cal f\n    exit\n");
    fx.disk->set("/p/lib.asm", "f:\n    ret\n");
    fx.ws.open(Fixture::uri("/p/main.asm"), "include \"lib.asm\"\n_start:\n    cal f\n    exit\n");
    fx.ws.open(Fixture::uri("/p/lib.asm"), "f:\n    ret\n");
    fx.ws.takePendingDiagnostics();

    fx.ws.change(Fixture::uri("/p/lib.asm"), "g:\n    ret\n"); // f disappears
    auto pub = fx.ws.takePendingDiagnostics();
    const auto* mainPub = findPublish(pub, Fixture::uri("/p/main.asm"));
    REQUIRE(mainPub != nullptr);
    REQUIRE_FALSE(mainPub->diagnostics.empty());

    // An identical change publishes nothing new.
    fx.ws.change(Fixture::uri("/p/lib.asm"), "g:\n    ret\n");
    REQUIRE(fx.ws.takePendingDiagnostics().empty());
}

TEST_CASE("Workspace: closing clears diagnostics and falls back to the disk", "[workspace]") {
    Fixture fx;
    fx.disk->set("/p/main.asm", "include \"lib.asm\"\n_start:\n    cal f\n    exit\n");
    fx.disk->set("/p/lib.asm", "f:\n    ret\n");
    fx.ws.open(Fixture::uri("/p/main.asm"), "include \"lib.asm\"\n_start:\n    cal f\n    exit\n");
    fx.ws.open(Fixture::uri("/p/lib.asm"), "bad bad\n");
    REQUIRE(fx.errors("/p/main.asm") == 1); // f is missing in the buffer
    fx.ws.takePendingDiagnostics();

    fx.ws.close(Fixture::uri("/p/lib.asm"));
    auto pub = fx.ws.takePendingDiagnostics();
    const auto* libPub = findPublish(pub, Fixture::uri("/p/lib.asm"));
    REQUIRE(libPub != nullptr);
    REQUIRE(libPub->diagnostics.empty());
    REQUIRE(fx.errors("/p/main.asm") == 0);  // disk version defines f
    REQUIRE_FALSE(fx.ws.lookup(Fixture::uri("/p/lib.asm")).has_value());
}

TEST_CASE("Workspace: a file created on disk resolves a missing include", "[workspace]") {
    Fixture fx;
    fx.ws.open(Fixture::uri("/p/main.asm"), "include \"lib.asm\"\n");
    REQUIRE(fx.errors("/p/main.asm") == 1);

    fx.disk->set("/p/lib.asm", "");
    fx.ws.fileChanged(Fixture::uri("/p/lib.asm"), Workspace::FileChange::Created);
    REQUIRE(fx.errors("/p/main.asm") == 0);
}

TEST_CASE("Workspace: path configuration applies to @u/ includes", "[workspace]") {
    auto* disk = new fs::InMemorySourceProvider();
    disk->set("/elsewhere/gfx/main.asm", "G:\n    ret\n");
    Fixture fx(disk);
    fx.ws.open(Fixture::uri("/p/main.asm"), "include \"@u/gfx/main.asm\"\n");
    REQUIRE(fx.errors("/p/main.asm") == 1);

    fx.ws.setPathConfig({"/elsewhere", "/s"});
    REQUIRE(fx.errors("/p/main.asm") == 0);
}

TEST_CASE("Workspace: unsaved documents are analysed on their own", "[workspace]") {
    Fixture fx;
    fx.ws.open("untitled:Untitled-1", "_start:\n    mov a0, 'hi'\n    exit\n");
    auto t = fx.ws.lookup("untitled:Untitled-1");
    REQUIRE(t.has_value());
    REQUIRE(t->file->diagnostics.empty());
}
