#pragma once
#include "lang/Compilation.h"
#include "protocol/LspTypes.h"
#include <vector>

namespace misa::features {

std::vector<lsp::Location> provideReferences(const lang::Compilation& c,
                                              lsp::Position pos,
                                              const std::string& uri,
                                              bool includeDeclaration);

} // namespace misa::features
