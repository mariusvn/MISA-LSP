#pragma once
#include "lang/Compilation.h"
#include "protocol/LspTypes.h"

namespace misa::features {

lsp::CompletionList provideCompletion(const lang::Compilation& c,
                                      lsp::Position pos);

} // namespace misa::features
