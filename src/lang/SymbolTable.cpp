#include "lang/SymbolTable.h"
#include <algorithm>

namespace misa::lang {

size_t SymbolTable::addDefinition(SymbolDef def) {
    size_t idx = m_defs.size();
    m_defIndex[def.name] = idx; // last wins (reusable labels register multiple)
    m_defs.push_back(std::move(def));
    return idx;
}

void SymbolTable::addReference(SymbolRef ref) {
    m_refs.push_back(std::move(ref));
}

const SymbolDef* SymbolTable::find(const std::string& name) const {
    auto it = m_defIndex.find(name);
    return it != m_defIndex.end() ? &m_defs[it->second] : nullptr;
}

int32_t SymbolTable::indexOf(const std::string& name) const {
    auto it = m_defIndex.find(name);
    return it != m_defIndex.end() ? static_cast<int32_t>(it->second) : -1;
}

int32_t SymbolTable::findReusable(const std::string& baseName, uint32_t seq, bool above) const {
    const std::string nameWithAt = "@" + baseName; // stored as "@baseName"
    int32_t best = -1;
    for (size_t i = 0; i < m_defs.size(); ++i) {
        const auto& def = m_defs[i];
        if (def.kind != SymbolKind::ReusableLabel || def.name != nameWithAt) continue;
        if (above) {
            if (def.seq <= seq && (best < 0 || def.seq > m_defs[best].seq))
                best = static_cast<int32_t>(i);
        } else {
            if (def.seq > seq && (best < 0 || def.seq < m_defs[best].seq))
                best = static_cast<int32_t>(i);
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

std::vector<const SymbolRef*> SymbolTable::referencesTo(int32_t defIndex) const {
    std::vector<const SymbolRef*> result;
    if (defIndex < 0) return result;
    for (const auto& ref : m_refs)
        if (ref.target == defIndex) result.push_back(&ref);
    return result;
}

} // namespace misa::lang
