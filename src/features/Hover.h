#pragma once
#include "lang/Compilation.h"
#include "protocol/LspTypes.h"
#include <optional>

namespace misa::features {

std::optional<lsp::Hover> provideHover(const lang::Compilation& c, const lang::SourceFile& f,
                                       lsp::Position pos);

inline std::optional<lsp::Hover> provideHover(const lang::Compilation& c, lsp::Position pos) {
    return provideHover(c, c.root(), pos);
}

} // namespace misa::features
