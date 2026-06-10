#pragma once
#include "protocol/LspTypes.h"
#include <string>
#include <vector>

namespace misa::text {

// Stores source text and provides efficient position <-> byte-offset conversion.
// Positions use LSP semantics: 0-indexed, characters in UTF-16 code units.
class TextDocument {
public:
    explicit TextDocument(std::string text = {});

    void update(std::string text);

    const std::string& text() const { return m_text; }
    uint32_t           lineCount() const { return static_cast<uint32_t>(m_lineOffsets.size()); }

    // Convert a byte offset (in the UTF-8 text) to an LSP Position.
    lsp::Position offsetToPosition(uint32_t offset) const;

    // Convert an LSP Position to a byte offset.
    uint32_t positionToOffset(lsp::Position pos) const;

    // Return the text of a single line (without trailing newline).
    std::string_view lineText(uint32_t line) const;

    // Return the range of a full line (including newline if any).
    lsp::Range lineRange(uint32_t line) const;

private:
    std::string              m_text;
    std::vector<uint32_t>    m_lineOffsets; // byte offset of the first char of each line

    void buildIndex();
    // Count UTF-16 code units in a UTF-8 string slice [start, end).
    static uint32_t utf16Len(const char* start, const char* end);
};

} // namespace misa::text
