#pragma once
#include "lang/Compilation.h"
#include "protocol/LspTypes.h"
#include <vector>

namespace misa::features {

// Outline of file `f` (symbols defined in other files of the unit are skipped).
std::vector<lsp::DocumentSymbol> provideDocumentSymbols(const lang::Compilation& c,
                                                        const lang::SourceFile& f);

inline std::vector<lsp::DocumentSymbol> provideDocumentSymbols(const lang::Compilation& c) {
    return provideDocumentSymbols(c, c.root());
}

} // namespace misa::features
