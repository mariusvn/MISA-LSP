#pragma once
#include "lang/Compilation.h"
#include "protocol/LspTypes.h"
#include <vector>

namespace misa::features {

std::vector<lsp::FoldingRange> provideFoldingRanges(const lang::Compilation& c);

} // namespace misa::features
