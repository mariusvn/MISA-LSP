#include "features/Hover.h"
#include "features/FeatureUtil.h"
#include "kb/KnowledgeBase.h"
#include <sstream>

namespace misa::features {

using namespace misa::lang;
using namespace misa::lsp;

// Character literal ('a' … 'abcd') touching column `col` of a line, if any.
// Returns the decoded characters and the literal's column range.
struct CharLiteral { std::string value; size_t start, end; };

static std::optional<CharLiteral> charLiteralAt(std::string_view line, size_t col) {
    size_t i = 0;
    while (i < line.size()) {
        char ch = line[i];
        if (ch == '#') return std::nullopt;                  // comment
        if (ch == '"') {                                     // skip strings
            for (++i; i < line.size() && line[i] != '"'; ++i)
                if (line[i] == '\\') ++i;
            ++i;
            continue;
        }
        if (ch != '\'') { ++i; continue; }
        size_t start = i;
        std::string value;
        for (++i; i < line.size() && line[i] != '\''; ++i) {
            if (line[i] == '\\' && i + 1 < line.size()) {
                char e = line[++i];
                value += e == '0' ? '\0' : e == 't' ? '\t' : e == 'n' ? '\n' : e;
            } else {
                value += line[i];
            }
        }
        size_t end = i < line.size() ? i + 1 : i;
        if (col >= start && col <= end) return CharLiteral{value, start, end};
        i = end;
    }
    return std::nullopt;
}

static std::string hex(uint64_t v) {
    std::ostringstream ss;
    ss << "0x" << std::hex << v;
    return ss.str();
}

// Markdown for a user-defined label or constant.
static std::string describeSymbol(const Compilation& c, const SourceFile& f, const SymbolDef& def) {
    bool isConst = def.kind == lang::SymbolKind::Constant || def.kind == lang::SymbolKind::LocalConstant;
    std::ostringstream ss;
    ss << "**" << (isConst ? "Constant" : "Label") << " `" << def.name << "`**";
    if (!def.parent.empty() && def.kind != lang::SymbolKind::GlobalLabel)
        ss << "  *(in `" << def.parent << "`)*";
    if (isConst && def.constValue) {
        ss << "\n\nValue: `";
        if (def.isFloat) {
            ss << *def.constValue << "`";
        } else {
            auto iv = static_cast<int64_t>(*def.constValue);
            ss << iv << "`";
            if (iv < 0 || iv > 9) ss << " (`" << hex(static_cast<uint64_t>(iv) & 0xFFFFFFFFu) << "`)";
        }
    }
    const SourceFile& file = c.files[def.file];
    if (def.file != f.id)
        ss << "\n\nDefined in `" << displayPath(c, file.path.empty() ? file.uri : file.path)
           << "` (line " << (def.selRange.start.line + 1) << ")";

    // Doc comment (## lines) directly above the definition.
    std::vector<std::string> docLines;
    for (uint32_t line = def.range.start.line; line-- > 0;) {
        auto text = file.doc().lineText(line);
        size_t k = text.find_first_not_of(" \t");
        if (k == std::string_view::npos || text.compare(k, 2, "##") != 0) break;
        std::string_view body = text.substr(k + 2);
        if (!body.empty() && body[0] == ' ') body.remove_prefix(1);
        docLines.emplace_back(body);
    }
    if (!docLines.empty()) {
        ss << "\n\n---\n\n";
        for (auto it = docLines.rbegin(); it != docLines.rend(); ++it) ss << *it << "  \n";
    }
    return ss.str();
}

std::optional<Hover> provideHover(const Compilation& c, const SourceFile& f, Position pos) {
    const auto& kb = kb::KnowledgeBase::get();

    // ── include / emb file path ───────────────────────────────────────────────
    if (const auto* inc = includeAt(f, pos)) {
        if (inc->resolvedPath.empty()) return std::nullopt;
        std::ostringstream ss;
        ss << "**Include** `" << inc->resolvedPath << "`";
        using St = IncludeRecord::Status;
        if (inc->status == St::NotFound)        ss << "\n\n> ⚠️ File not found.";
        if (inc->status == St::AlreadyIncluded) ss << "\n\nAlready included earlier; this include has no effect.";
        if (inc->status == St::Cycle)           ss << "\n\nCircular include; ignored.";
        return Hover{MarkupContent{MarkupKind::Markdown, ss.str()}, f.range(inc->pathSpan)};
    }
    if (const auto* emb = embeddedFileAt(f, pos)) {
        if (emb->resolvedPath.empty()) return std::nullopt;
        std::ostringstream ss;
        ss << "**Embedded file** `" << emb->resolvedPath << "`";
        if (!emb->exists) ss << "\n\n> ⚠️ File not found.";
        return Hover{MarkupContent{MarkupKind::Markdown, ss.str()}, f.range(emb->pathSpan)};
    }

    // ── Character literal ─────────────────────────────────────────────────────
    {
        auto line = f.doc().lineText(pos.line);
        uint32_t lineStart = f.doc().positionToOffset({pos.line, 0});
        size_t col = f.doc().positionToOffset(pos) - lineStart;
        if (auto lit = charLiteralAt(line, col); lit && !lit->value.empty() && lit->value.size() <= 4) {
            uint64_t v = 0;
            for (char ch : lit->value) v = (v << 8) | static_cast<unsigned char>(ch);
            std::ostringstream ss;
            ss << "**Character literal** = `" << hex(v) << "` (" << v << ")";
            Range r{f.doc().offsetToPosition(lineStart + static_cast<uint32_t>(lit->start)),
                    f.doc().offsetToPosition(lineStart + static_cast<uint32_t>(lit->end))};
            return Hover{MarkupContent{MarkupKind::Markdown, ss.str()}, r};
        }
    }

    auto [word, range] = wordAt(f, pos);
    if (word.empty()) return std::nullopt;
    // Numeric literals (42, 0x2a, 0b101…) are not identifiers — never resolve
    // them to a symbol.
    if (std::isdigit((unsigned char)word[0])) return std::nullopt;

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
    if (int32_t idx = symbolAt(c, f, pos); idx >= 0)
        return Hover{MarkupContent{MarkupKind::Markdown,
                                   describeSymbol(c, f, c.symbols.definitions()[idx])}, range};

    return std::nullopt;
}

} // namespace misa::features
