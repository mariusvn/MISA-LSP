#include "features/Definition.h"
#include "features/FeatureUtil.h"

namespace misa::features {

using namespace misa::lang;
using namespace misa::lsp;

std::vector<Location> provideDefinition(const Compilation& c, const SourceFile& f, Position pos) {
    // include "path" → the included file
    if (const auto* inc = includeAt(f, pos)) {
        if (inc->target) return {Location{c.files[*inc->target].uri, Range{}}};
        return {};
    }
    // emb file "path" → the embedded file
    if (const auto* emb = embeddedFileAt(f, pos)) {
        if (emb->exists) return {Location{fs::pathToUri(emb->resolvedPath), Range{}}};
        return {};
    }

    int32_t idx = symbolAt(c, f, pos);
    if (idx < 0) return {};
    const SymbolDef& def = c.symbols.definitions()[idx];
    return {Location{c.files[def.file].uri, def.selRange}};
}

} // namespace misa::features
