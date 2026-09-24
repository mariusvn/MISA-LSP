#pragma once
#include "lang/Compilation.h"
#include "protocol/LspTypes.h"
#include <vector>

namespace misa::features {

// Diagnostics are generated during compilation (parser, unit builder and
// SemanticAnalyzer). This thin wrapper exposes those of one file.
inline const std::vector<lsp::Diagnostic>& getDiagnostics(const lang::SourceFile& f) {
    return f.diagnostics;
}

} // namespace misa::features
