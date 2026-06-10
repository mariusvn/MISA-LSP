#pragma once
#include "lang/Compilation.h"
#include "protocol/LspTypes.h"
#include <optional>

namespace misa::features {

std::optional<lsp::SignatureHelp> provideSignatureHelp(const lang::Compilation& c,
                                                        lsp::Position pos);

} // namespace misa::features
