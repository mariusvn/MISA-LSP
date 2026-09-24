#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace misa::lsp {

// ── Positions ────────────────────────────────────────────────────────────────
// All positions are 0-indexed, characters counted in UTF-16 code units.

struct Position {
    uint32_t line      = 0;
    uint32_t character = 0;

    bool operator==(const Position& o) const { return line == o.line && character == o.character; }
    bool operator<(const Position& o) const {
        return line < o.line || (line == o.line && character < o.character);
    }
};

struct Range {
    Position start;
    Position end;
};

struct Location {
    std::string uri;
    Range       range;
};

// ── Document sync ─────────────────────────────────────────────────────────────

struct TextDocumentIdentifier        { std::string uri; };
struct VersionedTextDocumentIdentifier { std::string uri; int version = 0; };
struct TextDocumentItem { std::string uri, languageId, text; int version = 0; };
struct TextDocumentContentChangeEvent { std::string text; }; // Full sync only

// ── Diagnostics ───────────────────────────────────────────────────────────────

enum class DiagnosticSeverity : int { Error = 1, Warning = 2, Information = 3, Hint = 4 };
enum class DiagnosticTag : int { Unnecessary = 1, Deprecated = 2 };

struct Diagnostic {
    Range             range;
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string       message;
    std::optional<std::string> source;
    std::optional<std::string> code;
    std::vector<DiagnosticTag> tags;

    static Diagnostic make(Range range, DiagnosticSeverity severity, std::string message) {
        Diagnostic d;
        d.range    = range;
        d.severity = severity;
        d.message  = std::move(message);
        d.source   = "misa-lsp";
        return d;
    }

    bool operator==(const Diagnostic& o) const {
        return range.start == o.range.start && range.end == o.range.end &&
               severity == o.severity && message == o.message && tags == o.tags;
    }
};

// ── Markup ───────────────────────────────────────────────────────────────────

enum class MarkupKind { PlainText, Markdown };

struct MarkupContent {
    MarkupKind  kind  = MarkupKind::Markdown;
    std::string value;
};

// ── Hover ─────────────────────────────────────────────────────────────────────

struct Hover {
    MarkupContent          contents;
    std::optional<Range>   range;
};

// ── Completion ────────────────────────────────────────────────────────────────

enum class CompletionItemKind : int {
    Text = 1, Method = 2, Function = 3, Constructor = 4, Field = 5, Variable = 6,
    Class = 7, Interface = 8, Module = 9, Property = 10, Unit = 11, Value = 12,
    Enum = 13, Keyword = 14, Snippet = 15, Color = 16, File = 17, Reference = 18,
    Folder = 19, EnumMember = 20, Constant = 21, Struct = 22, Event = 23,
    Operator = 24, TypeParameter = 25
};

struct CompletionItem {
    std::string                   label;
    CompletionItemKind             kind   = CompletionItemKind::Text;
    std::optional<std::string>     detail;
    std::optional<MarkupContent>   documentation;
    std::optional<std::string>     insertText;
    std::optional<std::string>     sortText;
};

struct CompletionList {
    bool                       isIncomplete = false;
    std::vector<CompletionItem> items;
};

// ── Document symbols ─────────────────────────────────────────────────────────

enum class SymbolKind : int {
    File = 1, Module = 2, Namespace = 3, Package = 4, Class = 5, Method = 6,
    Property = 7, Field = 8, Constructor = 9, Enum = 10, Interface = 11,
    Function = 12, Variable = 13, Constant = 14, String = 15, Number = 16,
    Boolean = 17, Array = 18, Object = 19, Key = 20, Null = 21, EnumMember = 22,
    Struct = 23, Event = 24, Operator = 25, TypeParameter = 26
};

struct DocumentSymbol {
    std::string                   name;
    std::optional<std::string>    detail;
    SymbolKind                    kind            = SymbolKind::Function;
    Range                         range;
    Range                         selectionRange;
    std::vector<DocumentSymbol>   children;
};

// ── Signature help ───────────────────────────────────────────────────────────

struct ParameterInformation {
    std::string                label;
    std::optional<std::string> documentation;
};

struct SignatureInformation {
    std::string                        label;
    std::optional<std::string>         documentation;
    std::vector<ParameterInformation>  parameters;
};

struct SignatureHelp {
    std::vector<SignatureInformation>  signatures;
    std::optional<uint32_t>            activeSignature;
    std::optional<uint32_t>            activeParameter;
};

// ── Document links ────────────────────────────────────────────────────────────

struct DocumentLink {
    Range                      range;
    std::optional<std::string> target;  // URI
    std::optional<std::string> tooltip;
};

// ── Folding ranges ────────────────────────────────────────────────────────────

enum class FoldingRangeKind { Comment, Imports, Region };

struct FoldingRange {
    uint32_t                        startLine = 0;
    uint32_t                        endLine   = 0;
    std::optional<FoldingRangeKind> kind;
};

} // namespace misa::lsp
