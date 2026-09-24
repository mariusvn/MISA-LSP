#pragma once
#include "lang/Compilation.h"
#include "protocol/LspTypes.h"

namespace misa::features {

lsp::CompletionList provideCompletion(const lang::Compilation& c, const lang::SourceFile& f,
                                      lsp::Position pos);

inline lsp::CompletionList provideCompletion(const lang::Compilation& c, lsp::Position pos) {
    return provideCompletion(c, c.root(), pos);
}

} // namespace misa::features
