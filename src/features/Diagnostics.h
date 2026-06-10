#pragma once
#include "lang/Compilation.h"
#include "protocol/LspTypes.h"
#include <vector>

namespace misa::features {

// Diagnostics are generated during compilation (SemanticAnalyzer).
// This thin wrapper just exposes them.
inline const std::vector<lsp::Diagnostic>& getDiagnostics(const lang::Compilation& c) {
    return c.diagnostics;
}

} // namespace misa::features
