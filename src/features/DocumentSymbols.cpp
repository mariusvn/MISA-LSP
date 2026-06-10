#include "features/DocumentSymbols.h"
#include "kb/KnowledgeBase.h"
#include <algorithm>
#include <unordered_map>

namespace misa::features {

// Aliases to disambiguate the two SymbolKind enums.
namespace lsk = misa::lsp;    // lsk::SymbolKind = LSP symbol kind
namespace lk  = misa::lang;   // lk::SymbolKind  = internal kind

std::vector<lsk::DocumentSymbol> provideDocumentSymbols(const lk::Compilation& c) {
    std::vector<lsk::DocumentSymbol> globals;
    std::unordered_map<std::string, size_t> globalIdx;

    const auto& defs = c.symbols.definitions();

    // First pass: global labels
    for (const auto& def : defs) {
        if (def.kind == lk::SymbolKind::GlobalLabel) {
            lsk::DocumentSymbol sym;
            sym.name           = def.name;
            sym.kind           = lsk::SymbolKind::Function;
            sym.range          = def.range;
            sym.selectionRange = def.selRange;

            // Mark built-in entry points
            for (auto ep : kb::KnowledgeBase::ENTRY_POINTS) {
                if (def.name == std::string_view(ep)) {
                    sym.kind = lsk::SymbolKind::Event;
                    break;
                }
            }

            globalIdx[def.name] = globals.size();
            globals.push_back(std::move(sym));
        }
    }

    // Extend global ranges to just before the next global label.
    // Guard: the full range must always contain the selection range, otherwise
    // VS Code rejects the symbol ("selectionRange must be contained in fullRange").
    if (!globals.empty()) {
        uint32_t totalLines = c.doc.lineCount();
        for (size_t i = 0; i < globals.size(); ++i) {
            uint32_t nextLine = (i + 1 < globals.size())
                ? globals[i+1].range.start.line
                : totalLines;
            lsk::Position newEnd{nextLine > 0 ? nextLine - 1 : 0, 0};
            if (newEnd < globals[i].selectionRange.end)
                newEnd = globals[i].selectionRange.end;
            globals[i].range.end = newEnd;
        }
    }

    // Second pass: locals and constants → children of their parent
    for (const auto& def : defs) {
        if (def.kind == lk::SymbolKind::LocalLabel || def.kind == lk::SymbolKind::LocalConstant) {
            lsk::DocumentSymbol child;
            std::string shortName = def.name;
            size_t dot = def.name.find('.');
            if (dot != std::string::npos)
                shortName = def.name.substr(dot);

            child.name           = shortName;
            child.kind           = (def.kind == lk::SymbolKind::LocalConstant)
                                 ? lsk::SymbolKind::Constant : lsk::SymbolKind::Field;
            child.range          = def.range;
            child.selectionRange = def.selRange;

            auto it = globalIdx.find(def.parent);
            if (it != globalIdx.end())
                globals[it->second].children.push_back(std::move(child));
        } else if (def.kind == lk::SymbolKind::Constant) {
            lsk::DocumentSymbol sym;
            sym.name           = def.name;
            sym.kind           = lsk::SymbolKind::Constant;
            sym.range          = def.range;
            sym.selectionRange = def.selRange;
            globals.push_back(std::move(sym));
        }
    }

    std::sort(globals.begin(), globals.end(), [](const lsk::DocumentSymbol& a,
                                                  const lsk::DocumentSymbol& b) {
        return a.range.start.line < b.range.start.line;
    });

    return globals;
}

} // namespace misa::features
