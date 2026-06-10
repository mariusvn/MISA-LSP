#pragma once
#include "lang/Token.h"
#include "protocol/LspTypes.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace misa::lang {

enum class SymbolKind {
    GlobalLabel,
    LocalLabel,
    ReusableLabel,
    Constant,
    LocalConstant,
};

struct SymbolDef {
    std::string       name;         // qualified name (e.g. "func.local")
    SymbolKind        kind;
    lsp::Range        range;        // definition site (whole label line)
    lsp::Range        selRange;     // name token range
    std::string       parent;       // enclosing global label name (for locals)
    std::optional<double> constValue; // resolved value for constants
    bool              isFloat = false;
};

struct SymbolRef {
    std::string  name;   // qualified
    lsp::Range   range;
    uint32_t     line;
};

// Symbol table built during the two-pass semantic analysis.
class SymbolTable {
public:
    void addDefinition(SymbolDef def);
    void addReference(SymbolRef ref);

    const SymbolDef* find(const std::string& name) const;

    // Look up the closest @name above or below a given line.
    const SymbolDef* findReusable(const std::string& baseName, uint32_t line, bool above) const;

    const std::vector<SymbolDef>&   definitions() const { return m_defs; }
    const std::vector<SymbolRef>&   references()  const { return m_refs; }

    // All definitions of a given qualified name (for reusable labels).
    std::vector<const SymbolDef*>   allDefinitions(const std::string& name) const;
    // All references to a given qualified name.
    std::vector<const SymbolRef*>   allReferences(const std::string& name) const;

    // The last seen global label name (for local scoping during analysis).
    std::string currentGlobalLabel;

private:
    std::vector<SymbolDef>  m_defs;
    std::vector<SymbolRef>  m_refs;
    // name → index in m_defs (last definition for duplicates)
    std::unordered_map<std::string, size_t> m_defIndex;
};

} // namespace misa::lang
