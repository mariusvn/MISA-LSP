#include "features/Definition.h"
#include <cctype>

namespace misa::features {

using namespace misa::lang;
using namespace misa::lsp;

static std::string wordAt(const Compilation& c, Position pos) {
    auto line = c.doc.lineText(pos.line);
    uint32_t offset  = c.doc.positionToOffset(pos);
    uint32_t lstart  = c.doc.positionToOffset({pos.line, 0});
    uint32_t col     = offset - lstart;
    if (col >= line.size()) return {};

    size_t start = col;
    while (start > 0 && (std::isalnum((unsigned char)line[start-1]) || line[start-1] == '_'))
        --start;
    size_t end = col;
    while (end < line.size() && (std::isalnum((unsigned char)line[end]) || line[end] == '_'))
        ++end;
    return std::string(line.substr(start, end - start));
}

std::vector<Location> provideDefinition(const Compilation& c, Position pos,
                                         const std::string& uri) {
    std::string word = wordAt(c, pos);
    if (word.empty()) return {};

    // Try exact match first, then prefix-qualified match
    const SymbolDef* def = c.symbols.find(word);
    if (!def) {
        for (const auto& d : c.symbols.definitions()) {
            size_t dot = d.name.rfind('.');
            if (dot != std::string::npos && d.name.substr(dot + 1) == word) {
                def = &d;
                break;
            }
        }
    }
    if (!def) return {};

    return {Location{uri, def->range}};
}

} // namespace misa::features
