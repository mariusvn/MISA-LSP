#include "lang/SymbolTable.h"
#include <algorithm>

namespace misa::lang {

void SymbolTable::addDefinition(SymbolDef def) {
    size_t idx = m_defs.size();
    m_defIndex[def.name] = idx; // last wins (reusable labels register multiple)
    m_defs.push_back(std::move(def));
}

void SymbolTable::addReference(SymbolRef ref) {
    m_refs.push_back(std::move(ref));
}

const SymbolDef* SymbolTable::find(const std::string& name) const {
    auto it = m_defIndex.find(name);
    return it != m_defIndex.end() ? &m_defs[it->second] : nullptr;
}

const SymbolDef* SymbolTable::findReusable(
    const std::string& baseName, uint32_t line, bool above) const
{
    const SymbolDef* best = nullptr;
    for (const auto& def : m_defs) {
        if (def.kind != SymbolKind::ReusableLabel) continue;
        // Reusable label names are stored as "@baseName"
        std::string nameWithAt = "@" + baseName;
        if (def.name != nameWithAt) continue;
        uint32_t defLine = def.range.start.line;
        if (above) {
            if (defLine <= line && (!best || defLine > best->range.start.line))
                best = &def;
        } else {
            if (defLine > line && (!best || defLine < best->range.start.line))
                best = &def;
        }
    }
    return best;
}

std::vector<const SymbolDef*> SymbolTable::allDefinitions(const std::string& name) const {
    std::vector<const SymbolDef*> result;
    for (const auto& def : m_defs)
        if (def.name == name) result.push_back(&def);
    return result;
}

std::vector<const SymbolRef*> SymbolTable::allReferences(const std::string& name) const {
    std::vector<const SymbolRef*> result;
    for (const auto& ref : m_refs)
        if (ref.name == name) result.push_back(&ref);
    return result;
}

} // namespace misa::lang
