#pragma once
#include "lang/Compilation.h"
#include "protocol/LspTypes.h"
#include <vector>

namespace misa::features {

std::vector<lsp::FoldingRange> provideFoldingRanges(const lang::Compilation& c,
                                                   const lang::SourceFile& f);

inline std::vector<lsp::FoldingRange> provideFoldingRanges(const lang::Compilation& c) {
    return provideFoldingRanges(c, c.root());
}

} // namespace misa::features
