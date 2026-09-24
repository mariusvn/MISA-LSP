#pragma once
#include "lang/Compilation.h"
#include "protocol/LspTypes.h"
#include <vector>

namespace misa::features {

// Definition of the symbol (or included file) at `pos` in file `f`, which may
// live in another file of the unit.
std::vector<lsp::Location> provideDefinition(const lang::Compilation& c, const lang::SourceFile& f,
                                             lsp::Position pos);

inline std::vector<lsp::Location> provideDefinition(const lang::Compilation& c, lsp::Position pos) {
    return provideDefinition(c, c.root(), pos);
}

} // namespace misa::features
