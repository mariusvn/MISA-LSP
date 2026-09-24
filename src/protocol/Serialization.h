#pragma once
#include "protocol/LspTypes.h"
#include <nlohmann/json.hpp>

// to_json / from_json for all LSP types — included by server & feature providers.

namespace misa::lsp {

// ── Position / Range / Location ───────────────────────────────────────────────

inline void to_json(nlohmann::json& j, const Position& p) {
    j = {{"line", p.line}, {"character", p.character}};
}
inline void from_json(const nlohmann::json& j, Position& p) {
    p.line      = j.value("line",      0u);
    p.character = j.value("character", 0u);
}

inline void to_json(nlohmann::json& j, const Range& r) {
    j = {{"start", r.start}, {"end", r.end}};
}
inline void from_json(const nlohmann::json& j, Range& r) {
    r.start = j.value("start", Position{});
    r.end   = j.value("end",   Position{});
}

inline void to_json(nlohmann::json& j, const Location& l) {
    j = {{"uri", l.uri}, {"range", l.range}};
}

// ── Diagnostic ────────────────────────────────────────────────────────────────

inline void to_json(nlohmann::json& j, const Diagnostic& d) {
    j = {
        {"range",    d.range},
        {"severity", static_cast<int>(d.severity)},
        {"message",  d.message},
        {"source",   d.source.value_or("misa-lsp")}
    };
    if (d.code) j["code"] = *d.code;
    if (!d.tags.empty()) {
        auto& tags = j["tags"] = nlohmann::json::array();
        for (auto t : d.tags) tags.push_back(static_cast<int>(t));
    }
}

// ── DocumentLink ──────────────────────────────────────────────────────────────

inline void to_json(nlohmann::json& j, const DocumentLink& l) {
    j = {{"range", l.range}};
    if (l.target)  j["target"]  = *l.target;
    if (l.tooltip) j["tooltip"] = *l.tooltip;
}

// ── MarkupContent ─────────────────────────────────────────────────────────────

inline void to_json(nlohmann::json& j, const MarkupContent& m) {
    j = {
        {"kind",  m.kind == MarkupKind::Markdown ? "markdown" : "plaintext"},
        {"value", m.value}
    };
}

// ── Hover ─────────────────────────────────────────────────────────────────────

inline void to_json(nlohmann::json& j, const Hover& h) {
    j = {{"contents", h.contents}};
    if (h.range) j["range"] = *h.range;
}

// ── Completion ────────────────────────────────────────────────────────────────

inline void to_json(nlohmann::json& j, const CompletionItem& c) {
    j = {{"label", c.label}, {"kind", static_cast<int>(c.kind)}};
    if (c.detail)        j["detail"]       = *c.detail;
    if (c.documentation) j["documentation"] = *c.documentation;
    if (c.insertText)    j["insertText"]    = *c.insertText;
    if (c.sortText)      j["sortText"]      = *c.sortText;
}

inline void to_json(nlohmann::json& j, const CompletionList& cl) {
    j = {{"isIncomplete", cl.isIncomplete}, {"items", cl.items}};
}

// ── DocumentSymbol ────────────────────────────────────────────────────────────

inline void to_json(nlohmann::json& j, const DocumentSymbol& s);
inline void to_json(nlohmann::json& j, const DocumentSymbol& s) {
    j = {
        {"name",            s.name},
        {"kind",            static_cast<int>(s.kind)},
        {"range",           s.range},
        {"selectionRange",  s.selectionRange}
    };
    if (s.detail)           j["detail"]   = *s.detail;
    if (!s.children.empty()) j["children"] = s.children;
}

// ── SignatureHelp ─────────────────────────────────────────────────────────────

inline void to_json(nlohmann::json& j, const ParameterInformation& p) {
    j = {{"label", p.label}};
    if (p.documentation) j["documentation"] = *p.documentation;
}

inline void to_json(nlohmann::json& j, const SignatureInformation& s) {
    j = {{"label", s.label}, {"parameters", s.parameters}};
    if (s.documentation) j["documentation"] = *s.documentation;
}

inline void to_json(nlohmann::json& j, const SignatureHelp& sh) {
    j = {{"signatures", sh.signatures}};
    if (sh.activeSignature) j["activeSignature"] = *sh.activeSignature;
    if (sh.activeParameter) j["activeParameter"] = *sh.activeParameter;
}

// ── FoldingRange ──────────────────────────────────────────────────────────────

inline void to_json(nlohmann::json& j, const FoldingRange& f) {
    j = {{"startLine", f.startLine}, {"endLine", f.endLine}};
    if (f.kind) {
        switch (*f.kind) {
            case FoldingRangeKind::Comment: j["kind"] = "comment"; break;
            case FoldingRangeKind::Imports: j["kind"] = "imports"; break;
            case FoldingRangeKind::Region:  j["kind"] = "region";  break;
        }
    }
}

// ── Server capabilities helpers ───────────────────────────────────────────────

inline nlohmann::json makeServerCapabilities() {
    return {
        {"textDocumentSync", 1},  // Full sync
        {"hoverProvider", true},
        {"completionProvider", {
            {"triggerCharacters", {" ", ".", "@", "_", "S", "B"}},
            {"resolveProvider", false}
        }},
        {"definitionProvider", true},
        {"referencesProvider", true},
        {"documentSymbolProvider", true},
        {"signatureHelpProvider", {
            {"triggerCharacters", {" ", ","}}
        }},
        {"foldingRangeProvider", true},
        {"documentLinkProvider", {{"resolveProvider", false}}}
    };
}

} // namespace misa::lsp
