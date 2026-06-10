#include "features/Hover.h"
#include "kb/KnowledgeBase.h"
#include <sstream>

namespace misa::features {

using namespace misa::lang;
using namespace misa::lsp;

// Find the word (identifier-like token) under a position.
static std::string wordAt(const Compilation& c, Position pos) {
    auto line = c.doc.lineText(pos.line);
    uint32_t offset = c.doc.positionToOffset(pos);
    uint32_t lineStart = c.doc.positionToOffset({pos.line, 0});
    uint32_t col = offset - lineStart;
    if (col >= line.size()) return {};

    // Walk backwards to start of word
    size_t start = col;
    while (start > 0 && (std::isalnum((unsigned char)line[start-1]) ||
                          line[start-1] == '_')) --start;
    // Walk forwards to end of word
    size_t end = col;
    while (end < line.size() && (std::isalnum((unsigned char)line[end]) ||
                                  line[end] == '_')) ++end;
    return std::string(line.substr(start, end - start));
}

static Range wordRange(const Compilation& c, Position pos) {
    auto line = c.doc.lineText(pos.line);
    uint32_t offset = c.doc.positionToOffset(pos);
    uint32_t lineStart = c.doc.positionToOffset({pos.line, 0});
    uint32_t col = offset - lineStart;
    if (col >= line.size()) return {pos, pos};

    size_t start = col;
    while (start > 0 && (std::isalnum((unsigned char)line[start-1]) || line[start-1] == '_'))
        --start;
    size_t end = col;
    while (end < line.size() && (std::isalnum((unsigned char)line[end]) || line[end] == '_'))
        ++end;
    return {Position{pos.line, (uint32_t)start}, Position{pos.line, (uint32_t)end}};
}

std::optional<Hover> provideHover(const Compilation& c, Position pos) {
    const auto& kb = kb::KnowledgeBase::get();
    std::string word = wordAt(c, pos);
    if (word.empty()) return std::nullopt;
    // Numeric literals (42, 0x2a, 0b101…) are not identifiers — never resolve
    // them to a symbol.
    if (std::isdigit((unsigned char)word[0])) return std::nullopt;
    Range range = wordRange(c, pos);

    // ── Instruction ───────────────────────────────────────────────────────────
    if (const auto* info = kb.lookupInstruction(word)) {
        std::ostringstream ss;
        ss << "**" << info->name << "** (`" << info->mnemonic << "`)";
        if (info->compact) ss << " `[c]`";
        ss << "\n\n";
        ss << "*Category:* " << info->category << "\n\n";
        ss << info->description << "\n\n";
        ss << "```\n" << info->pseudocode << "\n```";
        return Hover{MarkupContent{MarkupKind::Markdown, ss.str()}, range};
    }

    // ── Register ──────────────────────────────────────────────────────────────
    if (const auto* info = kb.lookupRegister(word)) {
        std::ostringstream ss;
        ss << "**Register `" << info->name << "`**  ";
        ss << "*(group: " << info->group << ")*\n\n";
        ss << info->role;
        if (info->readOnly) ss << "\n\n> ⚠️ Read-only register.";
        return Hover{MarkupContent{MarkupKind::Markdown, ss.str()}, range};
    }

    // ── Syscall ───────────────────────────────────────────────────────────────
    if (const auto* info = kb.lookupSyscall(word)) {
        std::ostringstream ss;
        ss << "**Syscall `" << info->name << "`**\n\n";
        ss << info->description << "\n\n";
        if (!info->args.empty()) {
            ss << "**Arguments:**\n";
            for (const auto& a : info->args)
                ss << "- `" << a.reg << "` — " << a.description
                   << (a.isFloat ? " *(float)*" : "") << "\n";
        }
        if (!info->returns.empty()) {
            ss << "\n**Returns:**\n";
            for (const auto& r : info->returns)
                ss << "- `" << r.reg << "` — " << r.description
                   << (r.isFloat ? " *(float)*" : "") << "\n";
        }
        return Hover{MarkupContent{MarkupKind::Markdown, ss.str()}, range};
    }

    // ── Type ──────────────────────────────────────────────────────────────────
    if (const auto* info = kb.lookupType(word)) {
        std::ostringstream ss;
        ss << "**Type `" << info->name << "`** — " << info->description;
        if (info->embedOnly) ss << "\n\n> Only valid in `emb` directives.";
        return Hover{MarkupContent{MarkupKind::Markdown, ss.str()}, range};
    }

    // ── Condition ─────────────────────────────────────────────────────────────
    if (const auto* info = kb.lookupCondition(word)) {
        std::ostringstream ss;
        ss << "**Condition `" << info->name << "`** — " << info->description;
        if (info->isFloat) ss << " *(float comparison)*";
        return Hover{MarkupContent{MarkupKind::Markdown, ss.str()}, range};
    }

    // ── Built-in symbol ───────────────────────────────────────────────────────
    if (const auto* info = kb.lookupBuiltin(word)) {
        std::ostringstream ss;
        ss << "**`" << info->name << "`** — " << info->note;
        if (word != "$") {
            ss << "\n\nValue: `";
            if (info->isFloat) ss << info->value;
            else               ss << (int64_t)info->value;
            ss << "`";
        }
        return Hover{MarkupContent{MarkupKind::Markdown, ss.str()}, range};
    }

    // ── User-defined symbol ───────────────────────────────────────────────────
    {
        const auto* def = c.symbols.find(word);
        if (!def) {
            // Try matching the local part of a qualified name, e.g. hovering
            // "MAPPING" resolves "PRINTER.MAPPING". Require a '.' boundary so a
            // bare word never matches an arbitrary suffix.
            const std::string dotted = "." + word;
            for (const auto& d : c.symbols.definitions()) {
                if (d.name.size() == dotted.size() ? false
                    : (d.name.size() > dotted.size() &&
                       d.name.compare(d.name.size() - dotted.size(), dotted.size(), dotted) == 0)) {
                    def = &d;
                    break;
                }
            }
        }
        if (def) {
            bool isConst = def->kind == misa::lang::SymbolKind::Constant ||
                           def->kind == misa::lang::SymbolKind::LocalConstant;
            std::ostringstream ss;
            ss << "**" << (isConst ? "Constant" : "Label") << " `" << def->name << "`**";
            if (!def->parent.empty()) ss << "  *(in `" << def->parent << "`)*";
            if (isConst && def->constValue)
                ss << "\n\nValue: `" << *def->constValue << "`";
            return Hover{MarkupContent{MarkupKind::Markdown, ss.str()}, range};
        }
    }

    return std::nullopt;
}

} // namespace misa::features
