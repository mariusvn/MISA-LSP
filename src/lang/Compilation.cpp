#include "lang/Compilation.h"
#include "lang/Lexer.h"
#include "lang/Parser.h"
#include "lang/SemanticAnalyzer.h"
#include <algorithm>
#include <functional>

namespace misa::lang {

using lsp::DiagnosticSeverity;

std::shared_ptr<const ParsedFile> ParsedFile::parse(std::string text) {
    auto pf = std::make_shared<ParsedFile>();
    Lexer lexer(text);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    pf->statements = parser.parse();
    pf->syntaxDiagnostics = lexer.diagnostics();
    pf->syntaxDiagnostics.insert(pf->syntaxDiagnostics.end(),
                                 parser.diagnostics().begin(), parser.diagnostics().end());
    pf->doc.update(std::move(text));
    return pf;
}

const SourceFile* Compilation::findByKey(const std::string& key) const {
    for (const auto& f : files) if (f.key == key) return &f;
    return nullptr;
}

const SourceFile* Compilation::findByUri(const std::string& uri) const {
    for (const auto& f : files) if (f.uri == uri) return &f;
    return nullptr;
}

// ── Unit builder ──────────────────────────────────────────────────────────────

namespace {

lsp::Diagnostic makeDiag(const SourceFile& f, Span sp, DiagnosticSeverity sev,
                         std::string msg, bool unnecessary = false) {
    lsp::Diagnostic d = lsp::Diagnostic::make(f.range(sp), sev, std::move(msg));
    if (unnecessary) d.tags.push_back(lsp::DiagnosticTag::Unnecessary);
    return d;
}

std::shared_ptr<const ParsedFile> parseCached(Compilation::ParseCache* cache,
                                              const std::string& key, std::string text) {
    if (cache) {
        auto it = cache->find(key);
        if (it != cache->end() && it->second->doc.text() == text) return it->second;
    }
    auto pf = ParsedFile::parse(std::move(text));
    if (cache) (*cache)[key] = pf;
    return pf;
}

class UnitBuilder {
public:
    UnitBuilder(Compilation& c, const Compilation::UnitOptions& o) : m_c(c), m_o(o) {}

    FileId addFile(std::string uri, std::string path, std::string key,
                   std::shared_ptr<const ParsedFile> parsed) {
        SourceFile f;
        f.id     = static_cast<FileId>(m_c.files.size());
        f.uri    = std::move(uri);
        f.path   = std::move(path);
        f.key    = std::move(key);
        f.parsed = std::move(parsed);
        for (const auto& d : f.parsed->syntaxDiagnostics)
            f.diagnostics.push_back(makeDiag(f, d.span, d.severity, d.message));
        m_seen[f.key] = f.id;
        m_c.files.push_back(std::move(f));
        return m_c.files.back().id;
    }

    void expand(FileId fid) {
        m_stack.push_back(fid);
        // Keep the parse alive locally: m_c.files may reallocate while recursing.
        std::shared_ptr<const ParsedFile> parsed = m_c.files[fid].parsed;
        const auto& stmts = parsed->statements;
        for (uint32_t i = 0; i < stmts.size(); ++i) {
            m_c.order.push_back(StmtRef{fid, i});
            if (const auto* inc = std::get_if<IncludeDirective>(&stmts[i]))
                handleInclude(fid, i, *inc);
            else if (const auto* emb = std::get_if<EmbDirective>(&stmts[i]))
                handleEmbFile(fid, *emb);
        }
        m_stack.pop_back();
    }

private:
    Compilation&                            m_c;
    const Compilation::UnitOptions&         m_o;
    std::unordered_map<std::string, FileId> m_seen;
    std::vector<FileId>                     m_stack;

    std::string baseDirOf(FileId fid) const {
        const auto& p = m_c.files[fid].path;
        return p.empty() ? std::string() : fs::dirname(p);
    }

    void diag(FileId fid, Span sp, DiagnosticSeverity sev, std::string msg, bool unnecessary = false) {
        auto& f = m_c.files[fid];
        f.diagnostics.push_back(makeDiag(f, sp, sev, std::move(msg), unnecessary));
    }

    std::string uriFor(const std::string& key, const std::string& path) const {
        if (m_o.uriForKey)
            if (auto u = m_o.uriForKey(key)) return *u;
        return fs::pathToUri(path);
    }

    void handleInclude(FileId fid, uint32_t stmtIndex, const IncludeDirective& inc) {
        if (!inc.hasPath) return; // reported by the parser

        IncludeRecord rec;
        rec.stmtIndex = stmtIndex;
        rec.pathSpan  = inc.pathSpan;

        auto resolved = fs::resolveIncludePath(inc.path, baseDirOf(fid), m_o.paths);
        if (resolved.error != fs::ResolveError::None) {
            diag(fid, inc.pathSpan, DiagnosticSeverity::Error,
                 "Cannot resolve '" + inc.path + "'. " + fs::describe(resolved.error));
            m_c.files[fid].includes.push_back(std::move(rec));
            return;
        }
        rec.resolvedPath = resolved.path;
        std::string name = fs::filename(resolved.path);
        std::string key  = m_o.sources->key(resolved.path);

        if (auto it = m_seen.find(key); it != m_seen.end()) {
            rec.target = it->second;
            bool cycle = std::find(m_stack.begin(), m_stack.end(), it->second) != m_stack.end();
            rec.status = cycle ? IncludeRecord::Status::Cycle
                               : IncludeRecord::Status::AlreadyIncluded;
            diag(fid, inc.pathSpan, DiagnosticSeverity::Hint,
                 cycle ? "Circular include of '" + name + "' is ignored: it is already being included."
                       : "'" + name + "' is already included; this include has no effect.",
                 /*unnecessary*/ true);
            m_c.files[fid].includes.push_back(std::move(rec));
            return;
        }

        auto text = m_o.sources->read(resolved.path);
        if (!text) {
            rec.status = IncludeRecord::Status::NotFound;
            diag(fid, inc.pathSpan, DiagnosticSeverity::Error,
                 "Cannot open included file '" + resolved.path + "'.");
            m_c.files[fid].includes.push_back(std::move(rec));
            return;
        }
        if (m_c.files.size() >= Compilation::MAX_FILES) {
            rec.status = IncludeRecord::Status::NotFound;
            diag(fid, inc.pathSpan, DiagnosticSeverity::Error,
                 "Too many included files (limit " + std::to_string(Compilation::MAX_FILES) + ").");
            m_c.files[fid].includes.push_back(std::move(rec));
            return;
        }
        std::string ext = fs::extensionLower(resolved.path);
        if (ext != ".asm" && ext != ".misa" && ext != ".mnemo")
            diag(fid, inc.pathSpan, DiagnosticSeverity::Warning,
                 "Included files are expected to be source files (.asm, .misa or .mnemo).");

        FileId target = addFile(uriFor(key, resolved.path), resolved.path, key,
                                parseCached(m_o.cache, key, std::move(*text)));
        rec.status = IncludeRecord::Status::Ok;
        rec.target = target;
        m_c.files[fid].includes.push_back(std::move(rec));
        expand(target);
    }

    void handleEmbFile(FileId fid, const EmbDirective& emb) {
        if (emb.typeName != "file" || emb.values.empty()) return;
        const auto* str = std::get_if<StringExpr>(&emb.values.front());
        if (!str || str->value.empty()) return;

        EmbeddedFileRecord rec;
        rec.pathSpan = str->span;
        auto resolved = fs::resolveIncludePath(str->value, baseDirOf(fid), m_o.paths);
        if (resolved.error == fs::ResolveError::None) {
            rec.resolvedPath = resolved.path;
            rec.exists = m_o.sources->exists(resolved.path);
            // Relative paths are documented as project-relative: also try the
            // directory of the root file when the file sits in an included library.
            if (!rec.exists && fid != 0 && !fs::isAbsolute(str->value) && str->value[0] != '@') {
                std::string alt = fs::join(baseDirOf(0), str->value);
                if (m_o.sources->exists(alt)) { rec.resolvedPath = alt; rec.exists = true; }
            }
        }

        std::string ext = fs::extensionLower(str->value);
        if (ext != ".png" && ext != ".bin")
            diag(fid, str->span, DiagnosticSeverity::Error,
                 "Only .png and .bin files can be embedded.");
        else if (resolved.error != fs::ResolveError::None &&
                 resolved.error != fs::ResolveError::RelativeWithoutBase)
            diag(fid, str->span, DiagnosticSeverity::Warning,
                 "Cannot resolve '" + str->value + "'. " + fs::describe(resolved.error));
        else if (resolved.error == fs::ResolveError::None && !rec.exists && m_o.checkEmbeddedFiles)
            diag(fid, str->span, DiagnosticSeverity::Warning,
                 "File not found: '" + rec.resolvedPath + "'.");

        m_c.files[fid].embeddedFiles.push_back(std::move(rec));
    }
};

// Warn on each include line when the included file (or anything it includes)
// has errors, so problems in closed files are visible from the includer.
void addIncludeErrorSummaries(Compilation& c) {
    std::vector<int> own(c.files.size(), 0), total(c.files.size(), -1);
    for (const auto& f : c.files)
        for (const auto& d : f.diagnostics)
            if (d.severity == DiagnosticSeverity::Error) ++own[f.id];

    // Ok-edges form a tree (each file is expanded once), so memoised recursion works.
    std::function<int(FileId)> sum = [&](FileId id) {
        if (total[id] >= 0) return total[id];
        int n = own[id];
        for (const auto& inc : c.files[id].includes)
            if (inc.status == IncludeRecord::Status::Ok && inc.target) n += sum(*inc.target);
        return total[id] = n;
    };

    for (auto& f : c.files) {
        for (const auto& inc : f.includes) {
            if (inc.status != IncludeRecord::Status::Ok || !inc.target) continue;
            int n = sum(*inc.target);
            if (n == 0) continue;
            std::string msg = "'" + fs::filename(inc.resolvedPath) + "' has " +
                              std::to_string(n) + " error(s)";
            // Point at the first error of the included file itself, if any.
            for (const auto& d : c.files[*inc.target].diagnostics) {
                if (d.severity != DiagnosticSeverity::Error) continue;
                msg += " (line " + std::to_string(d.range.start.line + 1) + ": " + d.message + ")";
                break;
            }
            if (msg.back() != '.') msg += '.';
            f.diagnostics.push_back(makeDiag(f, inc.pathSpan, DiagnosticSeverity::Warning, msg));
        }
    }
}

Compilation analyse(Compilation c) {
    SemanticAnalyzer sa(c);
    sa.analyze();
    addIncludeErrorSummaries(c);
    return c;
}

} // namespace

Compilation Compilation::build(const std::string& uri, const std::string& text) {
    fs::InMemorySourceProvider mem;
    UnitOptions opts;
    opts.sources = &mem;
    opts.checkEmbeddedFiles = false; // no file system: don't flag every emb file
    return buildText(uri, text, opts);
}

Compilation Compilation::buildText(const std::string& uri, const std::string& text,
                                   const UnitOptions& opts) {
    std::string path = fs::uriToPath(uri).value_or("");
    std::string key  = path.empty() ? uri : opts.sources->key(path);

    Compilation c;
    UnitBuilder b(c, opts);
    FileId root = b.addFile(uri, path, key, parseCached(opts.cache, key, text));
    b.expand(root);
    return analyse(std::move(c));
}

Compilation Compilation::buildUnit(const std::string& rootPath, const UnitOptions& opts,
                                   const std::string& rootUri) {
    std::string path = fs::normalize(rootPath);
    std::string key  = opts.sources->key(path);
    std::string text = opts.sources->read(path).value_or("");

    std::string uri = rootUri;
    if (uri.empty() && opts.uriForKey)
        if (auto u = opts.uriForKey(key)) uri = *u;
    if (uri.empty()) uri = fs::pathToUri(path);

    Compilation c;
    UnitBuilder b(c, opts);
    FileId root = b.addFile(std::move(uri), path, key, parseCached(opts.cache, key, std::move(text)));
    b.expand(root);
    return analyse(std::move(c));
}

} // namespace misa::lang
