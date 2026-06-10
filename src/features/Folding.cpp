#include "features/Folding.h"
#include <algorithm>

namespace misa::features {

namespace lk = misa::lang;
namespace lsk = misa::lsp;
using lk::Compilation;
using lk::SymbolDef;
using lk::BmkDirective;
using lk::DocCommentStmt;
using lsk::FoldingRange;
using lsk::FoldingRangeKind;

std::vector<FoldingRange> provideFoldingRanges(const Compilation& c) {
    std::vector<FoldingRange> ranges;

    // Fold 1: each global label scope (from its line to the line before the next global label)
    const auto& defs = c.symbols.definitions();
    std::vector<const SymbolDef*> globals;
    for (const auto& d : defs)
        if (d.kind == lk::SymbolKind::GlobalLabel)
            globals.push_back(&d);

    std::sort(globals.begin(), globals.end(),
        [](const SymbolDef* a, const SymbolDef* b) {
            return a->range.start.line < b->range.start.line;
        });

    uint32_t totalLines = c.doc.lineCount();
    for (size_t i = 0; i < globals.size(); ++i) {
        uint32_t start = globals[i]->range.start.line;
        uint32_t end   = (i + 1 < globals.size())
                       ? (globals[i+1]->range.start.line > 0
                          ? globals[i+1]->range.start.line - 1 : 0)
                       : (totalLines > 0 ? totalLines - 1 : 0);
        if (end > start) {
            FoldingRange fr;
            fr.startLine = start;
            fr.endLine   = end;
            fr.kind      = FoldingRangeKind::Region;
            ranges.push_back(fr);
        }
    }

    // Fold 2: bookmark sections (bmk/sbmk to next bmk/sbmk at same or higher level)
    struct BmkEntry { uint32_t line; bool isSub; };
    std::vector<BmkEntry> bookmarks;

    for (const auto& stmt : c.statements) {
        if (const auto* bmk = std::get_if<BmkDirective>(&stmt)) {
            uint32_t line = c.doc.offsetToPosition(bmk->span.start).line;
            bookmarks.push_back({line, bmk->isSub});
        }
    }

    for (size_t i = 0; i < bookmarks.size(); ++i) {
        // Find next bookmark at same or higher level
        uint32_t end = totalLines > 0 ? totalLines - 1 : 0;
        for (size_t j = i + 1; j < bookmarks.size(); ++j) {
            if (!bookmarks[j].isSub || !bookmarks[i].isSub) {
                end = bookmarks[j].line > 0 ? bookmarks[j].line - 1 : 0;
                break;
            }
        }
        if (end > bookmarks[i].line) {
            FoldingRange fr;
            fr.startLine = bookmarks[i].line;
            fr.endLine   = end;
            fr.kind      = FoldingRangeKind::Region;
            ranges.push_back(fr);
        }
    }

    // Fold 3: consecutive doc-comment blocks
    {
        uint32_t blockStart = UINT32_MAX;
        uint32_t blockEnd   = 0;
        auto flush = [&]() {
            if (blockStart != UINT32_MAX && blockEnd > blockStart) {
                FoldingRange fr;
                fr.startLine = blockStart;
                fr.endLine   = blockEnd;
                fr.kind      = FoldingRangeKind::Comment;
                ranges.push_back(fr);
            }
            blockStart = UINT32_MAX;
        };

        for (const auto& stmt : c.statements) {
            if (const auto* dc = std::get_if<DocCommentStmt>(&stmt)) {
                uint32_t line = c.doc.offsetToPosition(dc->span.start).line;
                if (blockStart == UINT32_MAX) blockStart = line;
                blockEnd = line;
            } else {
                flush();
            }
        }
        flush();
    }

    return ranges;
}

} // namespace misa::features
