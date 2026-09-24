#pragma once
#include "fs/Path.h"
#include "fs/SourceProvider.h"
#include "lang/Ast.h"
#include "lang/SymbolTable.h"
#include "protocol/LspTypes.h"
#include "text/TextDocument.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace misa::lang {

// One lexed and parsed source file. Immutable, so it can be shared between
// compilation units and reused across rebuilds while its text is unchanged.
struct ParsedFile {
    text::TextDocument            doc;
    std::vector<Statement>        statements;
    std::vector<SyntaxDiagnostic> syntaxDiagnostics; // lexer + parser

    static std::shared_ptr<const ParsedFile> parse(std::string text);
};

// What happened to one `include` directive.
struct IncludeRecord {
    enum class Status {
        Ok,              // file expanded at this position
        AlreadyIncluded, // include-once: expanded earlier, no effect here
        Cycle,           // file is one of the includers: ignored
        NotFound,        // resolved path does not exist
        Unresolved,      // path could not be resolved (virtual folder unknown, …)
    };
    uint32_t              stmtIndex = 0;  // IncludeDirective in the including file
    Span                  pathSpan;       // string literal, with quotes
    Status                status = Status::Unresolved;
    std::string           resolvedPath;   // empty when Unresolved
    std::optional<FileId> target;         // included file when it is part of the unit
};

// A path used by `emb file`, resolved for document links and hover.
struct EmbeddedFileRecord {
    Span        pathSpan;
    std::string resolvedPath; // empty when unresolved
    bool        exists = false;
};

struct SourceFile {
    FileId                            id = 0;
    std::string                       uri;
    std::string                       path; // normalised; empty for non-file URIs
    std::string                       key;  // SourceProvider::key, identity for include-once
    std::shared_ptr<const ParsedFile> parsed;
    std::vector<IncludeRecord>        includes;
    std::vector<EmbeddedFileRecord>   embeddedFiles;
    std::vector<lsp::Diagnostic>      diagnostics; // syntax + include + semantic, this file only

    const text::TextDocument&     doc()        const { return parsed->doc; }
    const std::vector<Statement>& statements() const { return parsed->statements; }
    lsp::Range range(Span sp) const {
        return {doc().offsetToPosition(sp.start), doc().offsetToPosition(sp.end)};
    }
};

// Position of a statement in the include-expanded stream.
struct StmtRef {
    FileId   file  = 0;
    uint32_t index = 0;
};

// A compilation unit: a root file plus everything it includes (recursively,
// each file once), analysed as a single program exactly like the assembler
// sees it. Rebuilt whenever one of its files changes.
struct Compilation {
    std::vector<SourceFile> files; // files[0] is the root
    std::vector<StmtRef>    order; // statements in textual-insertion order
    SymbolTable             symbols;

    const SourceFile& root() const { return files.front(); }
    const SourceFile* findByKey(const std::string& key) const;
    const SourceFile* findByUri(const std::string& uri) const;
    const Statement&  stmt(StmtRef r) const { return files[r.file].statements()[r.index]; }

    // Single-file build from raw text, without any file-system access. Includes
    // are reported as unresolvable. Never throws — errors become diagnostics.
    static Compilation build(const std::string& uri, const std::string& text);

    using ParseCache = std::unordered_map<std::string /*key*/, std::shared_ptr<const ParsedFile>>;

    struct UnitOptions {
        const fs::SourceProvider* sources = nullptr;
        fs::PathConfig            paths;
        ParseCache*               cache = nullptr;   // reused parses, keyed by file key
        bool                      checkEmbeddedFiles = true; // warn when an emb file is missing
        // URI to report for a file key (e.g. the exact URI of an open editor);
        // defaults to fs::pathToUri(path).
        std::function<std::optional<std::string>(const std::string& key)> uriForKey;
    };

    // Multi-file build rooted at `rootPath`, reading every file through
    // `opts.sources`. `rootUri` overrides the URI reported for the root.
    static Compilation buildUnit(const std::string& rootPath, const UnitOptions& opts,
                                 const std::string& rootUri = {});

    // Multi-file build whose root is given as text (e.g. an unsaved document).
    // Relative includes resolve against the URI's directory when it is a file URI.
    static Compilation buildText(const std::string& uri, const std::string& text,
                                 const UnitOptions& opts);

    // Hard limit on the number of files in a unit.
    static constexpr size_t MAX_FILES = 512;
};

} // namespace misa::lang
