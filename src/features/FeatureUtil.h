#pragma once
#include "lang/Compilation.h"
#include "protocol/LspTypes.h"
#include <cctype>
#include <string>

// Helpers shared by the feature providers.
namespace misa::features {

inline bool isWordChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

struct WordAt {
    std::string word;
    lsp::Range  range;
};

// The identifier-like word ([A-Za-z0-9_]) touching `pos` in file `f`.
inline WordAt wordAt(const lang::SourceFile& f, lsp::Position pos) {
    auto line = f.doc().lineText(pos.line);
    uint32_t col = f.doc().positionToOffset(pos) - f.doc().positionToOffset({pos.line, 0});
    if (col > line.size()) col = static_cast<uint32_t>(line.size());

    size_t start = col;
    while (start > 0 && isWordChar(line[start - 1])) --start;
    size_t end = col;
    while (end < line.size() && isWordChar(line[end])) ++end;

    lsp::Position s = f.doc().offsetToPosition(f.doc().positionToOffset({pos.line, 0}) + static_cast<uint32_t>(start));
    lsp::Position e = f.doc().offsetToPosition(f.doc().positionToOffset({pos.line, 0}) + static_cast<uint32_t>(end));
    return {std::string(line.substr(start, end - start)), {s, e}};
}

inline bool contains(const lsp::Range& r, lsp::Position p) {
    return !(p < r.start) && !(r.end < p);
}

// Index of the symbol definition referenced or defined at `pos`, -1 if none.
// Uses the analyser's resolved references first (exact and scope-aware), then
// definition sites, then falls back to matching the word under the cursor.
inline int32_t symbolAt(const lang::Compilation& c, const lang::SourceFile& f, lsp::Position pos) {
    for (const auto& ref : c.symbols.references())
        if (ref.file == f.id && ref.target >= 0 && contains(ref.range, pos))
            return ref.target;

    const auto& defs = c.symbols.definitions();
    for (size_t i = 0; i < defs.size(); ++i)
        if (defs[i].file == f.id && contains(defs[i].selRange, pos))
            return static_cast<int32_t>(i);

    std::string word = wordAt(f, pos).word;
    if (word.empty() || std::isdigit(static_cast<unsigned char>(word[0]))) return -1;
    if (int32_t idx = c.symbols.indexOf(word); idx >= 0) return idx;
    // Local part of a qualified name: "MAPPING" → "PRINTER.MAPPING".
    const std::string dotted = "." + word;
    for (size_t i = 0; i < defs.size(); ++i) {
        const auto& n = defs[i].name;
        if (n.size() > dotted.size() &&
            n.compare(n.size() - dotted.size(), dotted.size(), dotted) == 0)
            return static_cast<int32_t>(i);
    }
    return -1;
}

// The include directive whose path string contains `pos`, if any.
inline const lang::IncludeRecord* includeAt(const lang::SourceFile& f, lsp::Position pos) {
    for (const auto& inc : f.includes)
        if (contains(f.range(inc.pathSpan), pos)) return &inc;
    return nullptr;
}

// The `emb file` path string containing `pos`, if any.
inline const lang::EmbeddedFileRecord* embeddedFileAt(const lang::SourceFile& f, lsp::Position pos) {
    for (const auto& emb : f.embeddedFiles)
        if (contains(f.range(emb.pathSpan), pos)) return &emb;
    return nullptr;
}

// A path relative to the unit root's directory when possible, for display.
inline std::string displayPath(const lang::Compilation& c, const std::string& path) {
    std::string base = c.root().path.empty() ? std::string() : fs::dirname(c.root().path);
    if (!base.empty() && path.size() > base.size() + 1 &&
        path.compare(0, base.size(), base) == 0 && path[base.size()] == '/')
        return path.substr(base.size() + 1);
    return path;
}

} // namespace misa::features
