#include "server/Workspace.h"

namespace misa::server {

using lang::Compilation;

// How far up the directory tree to look for project.mnemonimov.
static constexpr int MAX_PROJECT_DEPTH = 16;

Workspace::Workspace(std::unique_ptr<fs::SourceProvider> disk)
    : m_src(std::move(disk)) {}

std::string Workspace::keyFor(const std::string& uri, const std::string& path) const {
    return path.empty() ? uri : m_src.key(path);
}

Compilation::UnitOptions Workspace::unitOptions() {
    Compilation::UnitOptions o;
    o.sources  = &m_src;
    o.paths    = m_cfg;
    o.cache    = &m_parseCache;
    o.uriForKey = [this](const std::string& key) -> std::optional<std::string> {
        if (auto it = m_open.find(key); it != m_open.end()) return it->second.uri;
        return std::nullopt;
    };
    return o;
}

void Workspace::setPathConfig(fs::PathConfig cfg) {
    m_cfg = std::move(cfg);
    reconcile({}, /*rebuildAll*/ true);
}

// ── Document events ───────────────────────────────────────────────────────────

void Workspace::open(const std::string& uri, std::string text) {
    OpenDoc doc;
    doc.uri  = uri;
    doc.path = fs::uriToPath(uri).value_or("");
    doc.key  = keyFor(uri, doc.path);
    if (doc.path.empty()) doc.text = text;
    else                  m_src.setBuffer(doc.path, std::move(text));

    std::string key = doc.key;
    m_uriToKey[uri] = key;
    m_open[key] = std::move(doc);
    reconcile({key});
}

void Workspace::change(const std::string& uri, std::string text) {
    auto it = m_uriToKey.find(uri);
    if (it == m_uriToKey.end()) { open(uri, std::move(text)); return; }
    OpenDoc& doc = m_open[it->second];
    if (doc.path.empty()) doc.text = std::move(text);
    else                  m_src.setBuffer(doc.path, std::move(text));
    reconcile({doc.key});
}

void Workspace::close(const std::string& uri) {
    auto it = m_uriToKey.find(uri);
    if (it == m_uriToKey.end()) return;
    std::string key = it->second;
    const OpenDoc& doc = m_open[key];
    if (!doc.path.empty()) m_src.clearBuffer(doc.path);
    m_open.erase(key);
    m_uriToKey.erase(it);
    m_parseCache.erase(key); // the file on disk may differ from the closed buffer

    m_pending.push_back({uri, {}});
    m_published.erase(uri);
    reconcile({key});
}

void Workspace::fileChanged(const std::string& uri, FileChange change) {
    auto path = fs::uriToPath(uri);
    if (!path) return;
    std::string key = m_src.key(*path);
    if (m_open.count(key)) return; // the editor buffer is authoritative

    m_parseCache.erase(key);
    std::string name = fs::filename(*path);
    if (change != FileChange::Changed || name == "project.mnemonimov" || name == "main.asm")
        m_projectMain.clear();
    if (change == FileChange::Changed) reconcile({key});
    else                                reconcile({}, /*rebuildAll*/ true);
}

std::optional<Workspace::Target> Workspace::lookup(const std::string& uri) const {
    auto k = m_uriToKey.find(uri);
    if (k == m_uriToKey.end()) return std::nullopt;
    const OpenDoc& doc = m_open.at(k->second);
    auto u = m_units.find(doc.owner);
    if (u == m_units.end()) return std::nullopt;
    const lang::SourceFile* file = u->second.findByKey(doc.key);
    if (!file) return std::nullopt;
    return Target{&u->second, file};
}

std::vector<Workspace::Publish> Workspace::takePendingDiagnostics() {
    std::vector<Publish> out;
    out.swap(m_pending);
    return out;
}

// ── Units ─────────────────────────────────────────────────────────────────────

std::optional<std::string> Workspace::projectMainFor(const std::string& path) {
    std::string dir = fs::dirname(path);
    std::vector<std::string> visited;
    std::optional<std::string> found;
    for (int depth = 0; depth < MAX_PROJECT_DEPTH && !dir.empty(); ++depth) {
        if (auto it = m_projectMain.find(dir); it != m_projectMain.end()) {
            found = it->second;
            break;
        }
        visited.push_back(dir);
        if (m_src.base().exists(dir + "/project.mnemonimov")) {
            std::string main = dir + "/main.asm";
            if (m_src.exists(main)) found = main;
            break;
        }
        std::string parent = fs::dirname(dir);
        if (parent == dir) break;
        dir = parent;
    }
    for (const auto& d : visited) m_projectMain[d] = found;
    return found;
}

const Compilation& Workspace::ensureUnit(const std::string& rootKey, const std::string& rootPath,
                                         const OpenDoc* textRoot) {
    auto it = m_units.find(rootKey);
    if (it != m_units.end()) return it->second;
    Compilation c = textRoot && textRoot->path.empty()
        ? Compilation::buildText(textRoot->uri, textRoot->text, unitOptions())
        : Compilation::buildUnit(rootPath, unitOptions());
    return m_units.emplace(rootKey, std::move(c)).first->second;
}

void Workspace::reconcile(const std::unordered_set<std::string>& dirtyKeys, bool rebuildAll) {
    // 1. Drop units containing a changed file.
    for (auto it = m_units.begin(); it != m_units.end();) {
        bool stale = rebuildAll;
        for (const auto& f : it->second.files) {
            if (stale) break;
            stale = dirtyKeys.count(f.key) != 0;
        }
        it = stale ? m_units.erase(it) : std::next(it);
    }

    // 2. Pick the unit analysing each open document, building what is missing.
    std::unordered_set<std::string> live;
    for (auto& [key, doc] : m_open) {
        doc.owner = key;
        if (!doc.path.empty()) {
            if (auto main = projectMainFor(doc.path)) {
                std::string mainKey = m_src.key(*main);
                live.insert(mainKey); // keep it for the membership check next time
                if (mainKey != key && ensureUnit(mainKey, *main, nullptr).findByKey(key))
                    doc.owner = mainKey;
            }
        }
        if (doc.owner == key) ensureUnit(key, doc.path, &doc);
        live.insert(doc.owner);
    }

    // 3. Forget units nobody needs any more.
    for (auto it = m_units.begin(); it != m_units.end();)
        it = live.count(it->first) ? std::next(it) : m_units.erase(it);

    // 4. Queue diagnostics that changed.
    for (const auto& [key, doc] : m_open) {
        const lang::SourceFile* file = m_units.at(doc.owner).findByKey(key);
        std::vector<lsp::Diagnostic> diags = file ? file->diagnostics : std::vector<lsp::Diagnostic>{};
        auto prev = m_published.find(doc.uri);
        if (prev != m_published.end() && prev->second == diags) continue;
        m_published[doc.uri] = diags;
        m_pending.push_back({doc.uri, std::move(diags)});
    }
}

} // namespace misa::server
