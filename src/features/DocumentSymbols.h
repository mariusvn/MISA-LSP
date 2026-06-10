#pragma once
#include "lang/Compilation.h"
#include "protocol/LspTypes.h"
#include <vector>

namespace misa::features {

std::vector<lsp::DocumentSymbol> provideDocumentSymbols(const lang::Compilation& c);

} // namespace misa::features
