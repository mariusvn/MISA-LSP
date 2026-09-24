// misa-lint — standalone CLI that prints diagnostics for a .asm/.mnemo file and
// every file it includes. Useful for CI and for testing the analyzer without an
// LSP client.
//
// usage: misa-lint [--user-dir <dir>] [--sample-dir <dir>] <file.asm>
//   --user-dir    directory behind the @u/ virtual folder (default: auto-detected)
//   --sample-dir  directory behind the @s/ virtual folder (default: auto-detected)
#include "fs/Path.h"
#include "fs/SourceProvider.h"
#include "lang/Compilation.h"
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    using namespace misa;

    fs::PathConfig cfg = fs::detectDefaultPathConfig();
    std::string file;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--user-dir" && i + 1 < argc)        cfg.userProjectsDir   = fs::normalize(argv[++i]);
        else if (arg == "--sample-dir" && i + 1 < argc) cfg.sampleProjectsDir = fs::normalize(argv[++i]);
        else if (file.empty())                          file = arg;
        else { file.clear(); break; }
    }
    if (file.empty()) {
        std::cerr << "usage: misa-lint [--user-dir <dir>] [--sample-dir <dir>] <file.asm>\n";
        return 2;
    }

    std::error_code ec;
    auto abs = std::filesystem::absolute(std::filesystem::path(file), ec);
    auto u8  = abs.generic_u8string();
    std::string path = fs::normalize(std::string(u8.begin(), u8.end()));

    fs::DiskSourceProvider disk;
    if (!disk.exists(path)) {
        std::cerr << "error: cannot open " << file << "\n";
        return 2;
    }

    lang::Compilation::UnitOptions opts;
    opts.sources = &disk;
    opts.paths   = cfg;
    auto c = lang::Compilation::buildUnit(path, opts);

    // A library inside a project is checked through the project's main.asm when
    // main.asm includes it, like the assembler (and the language server) do.
    if (auto main = fs::findProjectMain(path, disk); main && disk.key(*main) != disk.key(path)) {
        auto unit = lang::Compilation::buildUnit(*main, opts);
        if (unit.findByKey(disk.key(path))) {
            std::cout << "(checked through " << *main << ")\n";
            c = std::move(unit);
        }
    }

    size_t total = 0;
    int errors = 0;
    for (const auto& f : c.files) {
        for (const auto& d : f.diagnostics) {
            using S = lsp::DiagnosticSeverity;
            const char* sev =
                d.severity == S::Error   ? "error"   :
                d.severity == S::Warning ? "warning" :
                d.severity == S::Hint    ? "hint"    : "info";
            if (d.severity == S::Error) ++errors;
            ++total;
            std::cout << (f.key == disk.key(path) ? file : f.path) << ":" << (d.range.start.line + 1) << ":"
                      << (d.range.start.character + 1) << ": " << sev << ": "
                      << d.message << "\n";
        }
    }
    std::cout << "-- " << c.files.size() << " file(s), " << total << " diagnostic(s), "
              << errors << " error(s)\n";
    return errors == 0 ? 0 : 1;
}
