#pragma once
#include "lang/Compilation.h"
#include "protocol/LspTypes.h"
#include <vector>

namespace misa::features {

// Clickable links for `include` and `emb file` paths that resolve to a file.
std::vector<lsp::DocumentLink> provideDocumentLinks(const lang::Compilation& c,
                                                    const lang::SourceFile& f);

} // namespace misa::features
