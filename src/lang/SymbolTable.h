#pragma once
#include "lang/Token.h"
#include "protocol/LspTypes.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace misa::lang {

// Index of a source file inside a compilation unit (0 = root file).
using FileId = uint32_t;

enum class SymbolKind {
    GlobalLabel,
    LocalLabel,
    ReusableLabel,
    Constant,
    LocalConstant,
};

struct SymbolDef {
    std::string       name;         // qualified name (e.g. "func.local", "@loop")
    SymbolKind        kind;
    lsp::Range        range;        // definition site (whole label line)
    lsp::Range        selRange;     // name token range
    std::string       parent;       // enclosing global label name (for locals)
    std::optional<double> constValue; // resolved value for constants
    bool              isFloat = false;
    FileId            file = 0;     // file holding the definition
    uint32_t          seq  = 0;     // position in the include-expanded statement stream
};

struct SymbolRef {
    std::string  name;        // qualified ("@loop" for reusable references)
    lsp::Range   range;
    FileId       file = 0;
    uint32_t     seq  = 0;    // position in the include-expanded statement stream
    int32_t      target = -1; // index into definitions(), -1 if unresolved
};

// Symbol table built during the two-pass semantic analysis of a whole unit.
class SymbolTable {
public:
    // Returns the index of the new definition.
    size_t addDefinition(SymbolDef def);
    void   addReference(SymbolRef ref);

    const SymbolDef* find(const std::string& name) const;
    int32_t          indexOf(const std::string& name) const; // -1 if unknown

    // Index of the closest "@baseName" definition before (above) or after a
    // stream position, -1 if none.
    int32_t findReusable(const std::string& baseName, uint32_t seq, bool above) const;

    const std::vector<SymbolDef>&   definitions() const { return m_defs; }
    const std::vector<SymbolRef>&   references()  const { return m_refs; }
    std::vector<SymbolRef>&         references()        { return m_refs; }
    SymbolDef&                      definition(size_t i) { return m_defs[i]; }

    // All definitions of a given qualified name (for reusable labels).
    std::vector<const SymbolDef*>   allDefinitions(const std::string& name) const;
    // All references to a given qualified name.
    std::vector<const SymbolRef*>   allReferences(const std::string& name) const;
    // All references resolved to the definition at `defIndex`.
    std::vector<const SymbolRef*>   referencesTo(int32_t defIndex) const;

private:
    std::vector<SymbolDef>  m_defs;
    std::vector<SymbolRef>  m_refs;
    // name → index in m_defs (last definition for duplicates)
    std::unordered_map<std::string, size_t> m_defIndex;
};

} // namespace misa::lang
