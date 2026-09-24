#pragma once
#include "fs/Path.h"
#include "fs/SourceProvider.h"
#include "lang/Compilation.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace misa::server {

// Keeps the open documents and the compilation units that analyse them.
//
// Each open document is analysed inside a unit rooted either at itself or, when
// it lives in a Mnemonimov project (a folder with `project.mnemonimov`) whose
// main.asm includes it, at that main.asm — exactly what the assembler sees.
// Units are rebuilt when any of their files changes (open buffer or disk).
class Workspace {
public:
    explicit Workspace(std::unique_ptr<fs::SourceProvider> disk);

    void setPathConfig(fs::PathConfig cfg);
    const fs::PathConfig& pathConfig() const { return m_cfg; }

    void open  (const std::string& uri, std::string text);
    void change(const std::string& uri, std::string text);
    void close (const std::string& uri);

    enum class FileChange { Created = 1, Changed = 2, Deleted = 3 };
    // A file changed on disk (workspace/didChangeWatchedFiles).
    void fileChanged(const std::string& uri, FileChange change);

    struct Target {
        const lang::Compilation* unit;
        const lang::SourceFile*  file;
    };
    // The unit analysing an open document, and that document inside it.
    std::optional<Target> lookup(const std::string& uri) const;

    struct Publish {
        std::string                  uri;
        std::vector<lsp::Diagnostic> diagnostics;
    };
    // Diagnostics that changed since the last call (empty lists clear a file).
    std::vector<Publish> takePendingDiagnostics();

private:
    struct OpenDoc {
        std::string uri;
        std::string path;  // empty for non-file URIs
        std::string key;
        std::string text;  // kept for non-file URIs, which have no path to read
        std::string owner; // key of the unit root analysing this document
    };

    fs::OverlaySourceProvider                            m_src; // open buffers over disk
    fs::PathConfig                                       m_cfg;
    lang::Compilation::ParseCache                        m_parseCache;
    std::unordered_map<std::string, OpenDoc>             m_open;     // key → document
    std::unordered_map<std::string, std::string>         m_uriToKey;
    std::unordered_map<std::string, lang::Compilation>   m_units;    // root key → unit
    std::unordered_map<std::string, std::optional<std::string>> m_projectMain; // dir → main.asm
    std::unordered_map<std::string, std::vector<lsp::Diagnostic>> m_published; // uri → last sent
    std::vector<Publish>                                 m_pending;

    std::string keyFor(const std::string& uri, const std::string& path) const;
    lang::Compilation::UnitOptions unitOptions();
    std::optional<std::string> projectMainFor(const std::string& path);
    const lang::Compilation& ensureUnit(const std::string& rootKey, const std::string& rootPath,
                                        const OpenDoc* textRoot);
    void reconcile(const std::unordered_set<std::string>& dirtyKeys, bool rebuildAll = false);
};

} // namespace misa::server
