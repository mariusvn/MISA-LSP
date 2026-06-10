// misa-lint — standalone CLI that prints diagnostics for a .mnemo/.asm file.
// Useful for CI and for testing the analyzer without an LSP client.
#include "lang/Compilation.h"
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: misa-lint <file.mnemo>\n";
        return 2;
    }

    std::ifstream f(argv[1], std::ios::binary);
    if (!f) {
        std::cerr << "error: cannot open " << argv[1] << "\n";
        return 2;
    }
    std::stringstream ss;
    ss << f.rdbuf();

    auto c = misa::lang::Compilation::build(std::string("file://") + argv[1], ss.str());

    int errors = 0;
    for (const auto& d : c.diagnostics) {
        using S = misa::lsp::DiagnosticSeverity;
        const char* sev =
            d.severity == S::Error   ? "error"   :
            d.severity == S::Warning ? "warning" :
            d.severity == S::Hint    ? "hint"    : "info";
        if (d.severity == S::Error) ++errors;
        std::cout << argv[1] << ":" << (d.range.start.line + 1) << ":"
                  << (d.range.start.character + 1) << ": " << sev << ": "
                  << d.message << "\n";
    }
    std::cout << "-- " << c.diagnostics.size() << " diagnostic(s), "
              << errors << " error(s)\n";
    return errors == 0 ? 0 : 1;
}
