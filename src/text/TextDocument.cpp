#include "text/TextDocument.h"
#include <algorithm>
#include <cstring>

namespace misa::text {

TextDocument::TextDocument(std::string text) : m_text(std::move(text)) {
    buildIndex();
}

void TextDocument::update(std::string text) {
    m_text = std::move(text);
    buildIndex();
}

void TextDocument::buildIndex() {
    m_lineOffsets.clear();
    m_lineOffsets.push_back(0);
    for (uint32_t i = 0; i < m_text.size(); ++i) {
        if (m_text[i] == '\n')
            m_lineOffsets.push_back(i + 1);
    }
}

uint32_t TextDocument::utf16Len(const char* start, const char* end) {
    uint32_t count = 0;
    while (start < end) {
        unsigned char c = static_cast<unsigned char>(*start);
        uint32_t cp;
        if      (c < 0x80)  { cp = c;                start += 1; }
        else if (c < 0xE0)  { cp = c & 0x1F;         start += 2; }
        else if (c < 0xF0)  { cp = c & 0x0F;         start += 3; }
        else                 { cp = c & 0x07;         start += 4; }

        // Supplementary plane (U+10000..U+10FFFF) needs a surrogate pair → 2 UTF-16 units
        count += (cp >= 0x10000) ? 2 : 1;
    }
    return count;
}

lsp::Position TextDocument::offsetToPosition(uint32_t offset) const {
    if (m_lineOffsets.empty()) return {0, 0};

    // Binary search for the line
    auto it = std::upper_bound(m_lineOffsets.begin(), m_lineOffsets.end(), offset);
    --it;
    uint32_t line       = static_cast<uint32_t>(std::distance(m_lineOffsets.begin(), it));
    uint32_t lineStart  = *it;
    uint32_t character  = utf16Len(m_text.data() + lineStart, m_text.data() + offset);
    return {line, character};
}

uint32_t TextDocument::positionToOffset(lsp::Position pos) const {
    if (pos.line >= m_lineOffsets.size()) return static_cast<uint32_t>(m_text.size());

    uint32_t lineStart = m_lineOffsets[pos.line];

    // Walk forward counting UTF-16 units until we reach pos.character
    uint32_t utf16 = 0;
    const char* p  = m_text.data() + lineStart;
    const char* end = m_text.data() + m_text.size();

    while (p < end && utf16 < pos.character && *p != '\n') {
        unsigned char c = static_cast<unsigned char>(*p);
        uint32_t cpLen;
        uint32_t cp;
        if      (c < 0x80)  { cp = c;       cpLen = 1; }
        else if (c < 0xE0)  { cp = c & 0x1F; cpLen = 2; }
        else if (c < 0xF0)  { cp = c & 0x0F; cpLen = 3; }
        else                 { cp = c & 0x07; cpLen = 4; }

        uint32_t utf16Units = (cp >= 0x10000) ? 2 : 1;
        if (utf16 + utf16Units > pos.character) break;
        utf16 += utf16Units;
        p += cpLen;
    }
    return static_cast<uint32_t>(p - m_text.data());
}

std::string_view TextDocument::lineText(uint32_t line) const {
    if (line >= m_lineOffsets.size()) return {};
    uint32_t start = m_lineOffsets[line];
    uint32_t end   = (line + 1 < m_lineOffsets.size())
                   ? m_lineOffsets[line + 1]
                   : static_cast<uint32_t>(m_text.size());
    // Strip newline
    if (end > start && m_text[end - 1] == '\n') --end;
    if (end > start && m_text[end - 1] == '\r') --end;
    return std::string_view(m_text).substr(start, end - start);
}

lsp::Range TextDocument::lineRange(uint32_t line) const {
    lsp::Position start{line, 0};
    auto lineStr = lineText(line);
    lsp::Position end{line, utf16Len(lineStr.data(), lineStr.data() + lineStr.size())};
    return {start, end};
}

} // namespace misa::text
