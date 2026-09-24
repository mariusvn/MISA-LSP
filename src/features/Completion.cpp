#include "features/Completion.h"
#include "kb/KnowledgeBase.h"
#include <cctype>
#include <sstream>

namespace misa::features {

using namespace misa::lang;
using namespace misa::lsp;

// Determine the word the user is currently typing (to filter completions).
static std::string prefixAt(const SourceFile& f, Position pos) {
    auto line = f.doc().lineText(pos.line);
    uint32_t offset = f.doc().positionToOffset(pos);
    uint32_t lineStart = f.doc().positionToOffset({pos.line, 0});
    uint32_t col = offset - lineStart;
    if (col > line.size()) col = (uint32_t)line.size();

    size_t start = col;
    while (start > 0 && (std::isalnum((unsigned char)line[start-1]) || line[start-1] == '_'))
        --start;
    return std::string(line.substr(start, col - start));
}

// Where on the line is the cursor, semantically?
struct CompletionContext {
    bool        inMnemonic = true; // still typing the first token (mnemonic/directive)
    std::string mnemonic;          // first token (valid when !inMnemonic)
    int         slot = 0;          // operand index 0-based (valid when !inMnemonic)
};

static CompletionContext analyzeContext(const SourceFile& f, Position pos) {
    auto line = f.doc().lineText(pos.line);
    uint32_t col = f.doc().positionToOffset(pos) - f.doc().positionToOffset({pos.line, 0});
    if (col > line.size()) col = (uint32_t)line.size();
    std::string before(line.substr(0, col));

    CompletionContext ctx;

    size_t i = 0;
    while (i < before.size() && (before[i] == ' ' || before[i] == '\t')) ++i;

    // Strip a leading "label:" so "foo: mov …" is handled like "mov …".
    {
        size_t j = i;
        while (j < before.size() && (std::isalnum((unsigned char)before[j]) ||
               before[j] == '_' || before[j] == '.' || before[j] == '@')) ++j;
        if (j < before.size() && before[j] == ':') {
            i = j + 1;
            while (i < before.size() && (before[i] == ' ' || before[i] == '\t')) ++i;
        }
    }

    // Read the first token (the mnemonic / directive).
    size_t mnStart = i;
    while (i < before.size() && (std::isalnum((unsigned char)before[i]) || before[i] == '_'))
        ++i;
    ctx.mnemonic = before.substr(mnStart, i - mnStart);

    // No whitespace between the first token and the cursor → still typing it.
    if (i >= before.size()) {
        ctx.inMnemonic = true;
        return ctx;
    }

    // Otherwise we are in operand position; the slot is the number of commas seen.
    ctx.inMnemonic = false;
    int commas = 0;
    for (size_t k = i; k < before.size(); ++k)
        if (before[k] == ',') ++commas;
    ctx.slot = commas;
    return ctx;
}

// Build completion item for an instruction.
static CompletionItem instrItem(const kb::InstructionInfo& info) {
    CompletionItem item;
    item.label  = std::string(info.mnemonic);
    item.kind   = CompletionItemKind::Function;
    item.detail = std::string(info.name) + " — " + std::string(info.category);

    std::ostringstream doc;
    doc << info.description;
    if (!info.pseudocode.empty()) doc << "\n\n```\n" << info.pseudocode << "\n```";
    if (info.compact) doc << "\n\n*Supports compact `[c]` form.*";
    item.documentation = MarkupContent{MarkupKind::Markdown, doc.str()};
    item.sortText = "0_" + item.label; // sort before other items
    return item;
}

static CompletionItem regItem(const kb::RegisterInfo& info) {
    CompletionItem item;
    item.label  = std::string(info.name);
    item.kind   = CompletionItemKind::Variable;
    item.detail = std::string(info.group) + " register";
    item.documentation = MarkupContent{MarkupKind::Markdown, std::string(info.role)};
    item.sortText = "1_" + item.label;
    return item;
}

static CompletionItem typeItem(const kb::TypeInfo& info) {
    CompletionItem item;
    item.label  = std::string(info.name);
    item.kind   = CompletionItemKind::TypeParameter;
    item.detail = std::string(info.description);
    item.sortText = "2_" + item.label;
    return item;
}

static CompletionItem condItem(const kb::ConditionInfo& info) {
    CompletionItem item;
    item.label  = std::string(info.name);
    item.kind   = CompletionItemKind::EnumMember;
    item.detail = std::string(info.description);
    item.sortText = "2_" + item.label;
    return item;
}

static CompletionItem syscallItem(const kb::SyscallInfo& info) {
    CompletionItem item;
    item.label  = std::string(info.name);
    item.kind   = CompletionItemKind::Module;
    item.detail = std::string(info.description);
    item.sortText = "1_" + item.label;
    return item;
}

static CompletionItem builtinItem(const kb::BuiltinSymbol& info) {
    CompletionItem item;
    item.label  = std::string(info.name);
    item.kind   = CompletionItemKind::Constant;
    item.detail = std::string(info.note);
    item.sortText = "2_" + item.label;
    return item;
}

static CompletionItem labelItem(const Compilation& c, const SourceFile& f, const lang::SymbolDef& def) {
    CompletionItem item;
    bool isConst = def.kind == lang::SymbolKind::Constant || def.kind == lang::SymbolKind::LocalConstant;
    item.label  = def.name;
    item.kind   = isConst ? CompletionItemKind::Constant : CompletionItemKind::Reference;
    std::string what = isConst ? "constant" : "label";
    item.detail = (def.parent.empty() || def.kind == lang::SymbolKind::GlobalLabel)
                ? what : what + " in " + def.parent;
    if (def.file != f.id) {
        const auto& other = c.files[def.file];
        item.detail = *item.detail + " — " + fs::filename(other.path.empty() ? other.uri : other.path);
    }
    item.sortText = "3_" + item.label;
    return item;
}

CompletionList provideCompletion(const Compilation& c, const SourceFile& f, Position pos) {
    const auto& kb = kb::KnowledgeBase::get();
    CompletionList list;
    list.isIncomplete = false;

    std::string prefix = prefixAt(f, pos);
    CompletionContext ctx = analyzeContext(f, pos);

    auto matches = [&](std::string_view name) -> bool {
        if (prefix.empty()) return true;
        if (name.size() < prefix.size()) return false;
        for (size_t i = 0; i < prefix.size(); ++i)
            if (std::tolower((unsigned char)name[i]) != std::tolower((unsigned char)prefix[i]))
                return false;
        return true;
    };

    // ── First token on a line → instruction mnemonic or directive ─────────────
    if (ctx.inMnemonic) {
        for (const auto& info : kb.instructions())
            if (matches(info.mnemonic))
                list.items.push_back(instrItem(info));

        // Built-in directives
        for (const auto& kw : {"def", "undef", "emb", "res", "bmk", "sbmk", "include"}) {
            if (matches(kw)) {
                CompletionItem item;
                item.label = kw;
                item.kind  = CompletionItemKind::Keyword;
                item.sortText = "0_" + item.label;
                list.items.push_back(item);
            }
        }
        // Entry points
        for (auto ep : kb::KnowledgeBase::ENTRY_POINTS) {
            if (matches(ep)) {
                CompletionItem item;
                item.label  = std::string(ep);
                item.kind   = CompletionItemKind::Event;
                item.detail = "built-in entry point";
                item.sortText = "0_" + item.label;
                list.items.push_back(item);
            }
        }
        return list;
    }

    // ── Operand context: filter by the instruction's expected operand kind ────
    const std::string& mnemonic = ctx.mnemonic;
    const int commaCount = ctx.slot;

    // ── syscall argument → show SYS_ names ───────────────────────────────────
    if (mnemonic == "syscall" && commaCount == 0) {
        for (const auto& s : kb.syscalls())
            if (matches(s.name))
                list.items.push_back(syscallItem(s));
        return list;
    }

    // ── cmp first arg → conditions ────────────────────────────────────────────
    if (mnemonic == "cmp" && commaCount == 0) {
        for (const auto& cond : kb.conditions())
            if (matches(cond.name))
                list.items.push_back(condItem(cond));
        return list;
    }

    // ── include path / bookmark title: nothing sensible to suggest ───────────
    if (mnemonic == "include" || mnemonic == "bmk" || mnemonic == "sbmk")
        return list;

    // ── emb/res first arg → types (res only takes scalar types) ──────────────
    if ((mnemonic == "emb" || mnemonic == "res") && commaCount == 0) {
        for (const auto& t : kb.types())
            if (matches(t.name) && !(mnemonic == "res" && t.embedOnly))
                list.items.push_back(typeItem(t));
        return list;
    }

    // ── lod/str/lde/ste first arg → types (non-embed-only) ───────────────────
    if ((mnemonic == "lod" || mnemonic == "str" ||
         mnemonic == "lde" || mnemonic == "ste") && commaCount == 0) {
        for (const auto& t : kb.types())
            if (!t.embedOnly && matches(t.name))
                list.items.push_back(typeItem(t));
        return list;
    }

    // ── General: registers + built-ins + user labels/constants ────────────────
    for (const auto& r : kb.registers())
        if (matches(r.name))
            list.items.push_back(regItem(r));

    for (const auto& b : kb.builtins())
        if (b.name != "$" && matches(b.name))
            list.items.push_back(builtinItem(b));

    // User-defined labels and constants
    // Reusable labels are referenced as @name- / @name+, not by their plain name.
    for (const auto& def : c.symbols.definitions())
        if (def.kind != lang::SymbolKind::ReusableLabel && matches(def.name))
            list.items.push_back(labelItem(c, f, def));

    return list;
}

} // namespace misa::features
