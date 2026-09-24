#include "fs/SourceProvider.h"
#include "fs/Path.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace misa::fs {

namespace {

std::filesystem::path toFsPath(const std::string& path) {
    return std::filesystem::path(std::u8string(path.begin(), path.end()));
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

// ── Disk ──────────────────────────────────────────────────────────────────────

std::optional<std::string> DiskSourceProvider::read(const std::string& path) const {
    std::error_code ec;
    auto fsPath = toFsPath(path);
    if (!std::filesystem::is_regular_file(fsPath, ec)) return std::nullopt;

    auto size  = std::filesystem::file_size(fsPath, ec);
    auto mtime = std::filesystem::last_write_time(fsPath, ec).time_since_epoch().count();
    std::string k = key(path);
    if (auto it = m_cache.find(k); it != m_cache.end() && !ec &&
        it->second.mtime == mtime && it->second.size == size)
        return it->second.text;

    std::ifstream f(fsPath, std::ios::binary);
    if (!f) return std::nullopt;
    std::stringstream ss;
    ss << f.rdbuf();
    std::string text = ss.str();
    if (!ec) m_cache[k] = Cached{static_cast<int64_t>(mtime), size, text};
    return text;
}

bool DiskSourceProvider::exists(const std::string& path) const {
    std::error_code ec;
    return std::filesystem::is_regular_file(toFsPath(path), ec);
}

std::string DiskSourceProvider::key(const std::string& path) const {
    std::error_code ec;
    auto canon = std::filesystem::weakly_canonical(toFsPath(path), ec);
    std::string k;
    if (ec) {
        k = normalize(path);
    } else {
        auto u8 = canon.generic_u8string();
        k = normalize(std::string(u8.begin(), u8.end()));
    }
#ifdef _WIN32
    k = lower(std::move(k)); // NTFS is case-insensitive
#endif
    return k;
}

// ── In-memory ─────────────────────────────────────────────────────────────────

void InMemorySourceProvider::set(const std::string& path, std::string text) {
    m_files[key(path)] = std::move(text);
}

void InMemorySourceProvider::remove(const std::string& path) {
    m_files.erase(key(path));
}

std::optional<std::string> InMemorySourceProvider::read(const std::string& path) const {
    auto it = m_files.find(key(path));
    if (it == m_files.end()) return std::nullopt;
    return it->second;
}

bool InMemorySourceProvider::exists(const std::string& path) const {
    return m_files.count(key(path)) != 0;
}

std::string InMemorySourceProvider::key(const std::string& path) const {
    std::string k = normalize(path);
    return m_caseInsensitive ? lower(std::move(k)) : k;
}

// ── Projects ──────────────────────────────────────────────────────────────────

std::optional<std::string> findProjectMain(const std::string& path, const SourceProvider& sources,
                                           int maxDepth) {
    std::string dir = dirname(path);
    for (int depth = 0; depth < maxDepth && !dir.empty(); ++depth) {
        if (sources.exists(dir + "/project.mnemonimov")) {
            std::string main = dir + "/main.asm";
            if (sources.exists(main)) return main;
            return std::nullopt;
        }
        std::string parent = dirname(dir);
        if (parent == dir) break;
        dir = parent;
    }
    return std::nullopt;
}

// ── Overlay ───────────────────────────────────────────────────────────────────

void OverlaySourceProvider::setBuffer(const std::string& path, std::string text) {
    m_buffers[key(path)] = std::move(text);
}

void OverlaySourceProvider::clearBuffer(const std::string& path) {
    m_buffers.erase(key(path));
}

bool OverlaySourceProvider::hasBuffer(const std::string& path) const {
    return m_buffers.count(key(path)) != 0;
}

std::optional<std::string> OverlaySourceProvider::read(const std::string& path) const {
    if (auto it = m_buffers.find(key(path)); it != m_buffers.end()) return it->second;
    return m_base->read(path);
}

bool OverlaySourceProvider::exists(const std::string& path) const {
    return hasBuffer(path) || m_base->exists(path);
}

} // namespace misa::fs
