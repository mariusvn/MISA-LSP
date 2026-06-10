#include "features/References.h"
#include <cctype>

namespace misa::features {

using namespace misa::lang;
using namespace misa::lsp;

static std::string wordAt(const Compilation& c, Position pos) {
    auto line = c.doc.lineText(pos.line);
    uint32_t offset = c.doc.positionToOffset(pos);
    uint32_t lstart = c.doc.positionToOffset({pos.line, 0});
    uint32_t col    = offset - lstart;
    if (col >= line.size()) return {};

    size_t start = col;
    while (start > 0 && (std::isalnum((unsigned char)line[start-1]) || line[start-1] == '_'))
        --start;
    size_t end = col;
    while (end < line.size() && (std::isalnum((unsigned char)line[end]) || line[end] == '_'))
        ++end;
    return std::string(line.substr(start, end - start));
}

std::vector<Location> provideReferences(const Compilation& c, Position pos,
                                         const std::string& uri,
                                         bool includeDeclaration) {
    std::string word = wordAt(c, pos);
    if (word.empty()) return {};

    // Resolve to qualified name
    const SymbolDef* def = c.symbols.find(word);
    std::string qname = word;
    if (!def) {
        for (const auto& d : c.symbols.definitions()) {
            size_t dot = d.name.rfind('.');
            if (dot != std::string::npos && d.name.substr(dot + 1) == word) {
                def = &d;
                qname = d.name;
                break;
            }
        }
    } else {
        qname = def->name;
    }

    std::vector<Location> result;

    // Include declaration
    if (includeDeclaration && def)
        result.push_back(Location{uri, def->range});

    // All references
    for (const auto* ref : c.symbols.allReferences(qname))
        result.push_back(Location{uri, ref->range});

    return result;
}

} // namespace misa::features
