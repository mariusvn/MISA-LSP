#include "features/References.h"
#include "features/FeatureUtil.h"

namespace misa::features {

using namespace misa::lang;
using namespace misa::lsp;

std::vector<Location> provideReferences(const Compilation& c, const SourceFile& f,
                                        Position pos, bool includeDeclaration) {
    int32_t idx = symbolAt(c, f, pos);
    if (idx < 0) return {};
    const SymbolDef& def = c.symbols.definitions()[idx];

    std::vector<Location> result;
    if (includeDeclaration)
        result.push_back(Location{c.files[def.file].uri, def.selRange});

    for (const auto* ref : c.symbols.referencesTo(idx))
        result.push_back(Location{c.files[ref->file].uri, ref->range});

    return result;
}

} // namespace misa::features
