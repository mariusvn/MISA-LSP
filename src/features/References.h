#pragma once
#include "lang/Compilation.h"
#include "protocol/LspTypes.h"
#include <vector>

namespace misa::features {

// All references (across the unit's files) to the symbol at `pos` in file `f`.
std::vector<lsp::Location> provideReferences(const lang::Compilation& c, const lang::SourceFile& f,
                                             lsp::Position pos, bool includeDeclaration);

inline std::vector<lsp::Location> provideReferences(const lang::Compilation& c, lsp::Position pos,
                                                    bool includeDeclaration) {
    return provideReferences(c, c.root(), pos, includeDeclaration);
}

} // namespace misa::features
