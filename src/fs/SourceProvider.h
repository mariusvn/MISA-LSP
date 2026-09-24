#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

// Read access to source files. Paths are normalised (see fs/Path.h).
// The server reads from disk with open editor buffers layered on top; tests use
// an in-memory implementation.
namespace misa::fs {

class SourceProvider {
public:
    virtual ~SourceProvider() = default;

    virtual std::optional<std::string> read(const std::string& path) const = 0;
    virtual bool exists(const std::string& path) const = 0;

    // Identity of a file, used for include-once: two paths naming the same file
    // map to the same key (case-folded on case-insensitive file systems).
    virtual std::string key(const std::string& path) const = 0;
};

class DiskSourceProvider : public SourceProvider {
public:
    std::optional<std::string> read(const std::string& path) const override;
    bool exists(const std::string& path) const override;
    std::string key(const std::string& path) const override;

private:
    struct Cached { int64_t mtime; uintmax_t size; std::string text; };
    mutable std::unordered_map<std::string, Cached> m_cache; // key → last read
};

class InMemorySourceProvider : public SourceProvider {
public:
    explicit InMemorySourceProvider(bool caseInsensitive = false)
        : m_caseInsensitive(caseInsensitive) {}

    void set(const std::string& path, std::string text);
    void remove(const std::string& path);

    std::optional<std::string> read(const std::string& path) const override;
    bool exists(const std::string& path) const override;
    std::string key(const std::string& path) const override;

private:
    bool m_caseInsensitive;
    std::unordered_map<std::string, std::string> m_files; // key → text
};

// Mnemonimov projects are folders holding a `project.mnemonimov` file, with
// main.asm as their entry. Returns the main.asm of the project containing
// `path` (searching up to `maxDepth` parent folders), if any.
std::optional<std::string> findProjectMain(const std::string& path, const SourceProvider& sources,
                                           int maxDepth = 16);

// Open editor buffers take precedence over the underlying provider.
class OverlaySourceProvider : public SourceProvider {
public:
    explicit OverlaySourceProvider(std::unique_ptr<SourceProvider> base)
        : m_base(std::move(base)) {}

    void setBuffer(const std::string& path, std::string text);
    void clearBuffer(const std::string& path);
    bool hasBuffer(const std::string& path) const;

    std::optional<std::string> read(const std::string& path) const override;
    bool exists(const std::string& path) const override;
    std::string key(const std::string& path) const override { return m_base->key(path); }

    const SourceProvider& base() const { return *m_base; }

private:
    std::unique_ptr<SourceProvider>              m_base;
    std::unordered_map<std::string, std::string> m_buffers; // key → text
};

} // namespace misa::fs
