#include "fs/Path.h"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace misa::fs {

namespace {

bool isDriveAt(std::string_view p) {
    return p.size() >= 2 && std::isalpha(static_cast<unsigned char>(p[0])) && p[1] == ':';
}

bool isSep(char c) { return c == '/' || c == '\\'; }

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string percentDecode(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int hi = hexValue(s[i + 1]), lo = hexValue(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += static_cast<char>(hi * 16 + lo);
                i += 2;
                continue;
            }
        }
        out += s[i];
    }
    return out;
}

bool isUnreserved(unsigned char c) {
    return std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~';
}

} // namespace

std::string normalize(std::string_view in) {
    std::string p(in);
    for (char& c : p) if (c == '\\') c = '/';

    std::string prefix;
    size_t i = 0;
    bool absolute = false;
    if (p.size() >= 2 && p[0] == '/' && p[1] == '/') {          // UNC
        prefix = "//";
        i = 2;
        absolute = true;
    } else if (isDriveAt(p)) {                                   // c: or c:/
        prefix = std::string(1, static_cast<char>(std::tolower(static_cast<unsigned char>(p[0])))) + ":";
        i = 2;
        if (i < p.size() && p[i] == '/') { prefix += '/'; ++i; absolute = true; }
    } else if (!p.empty() && p[0] == '/') {
        prefix = "/";
        i = 1;
        absolute = true;
    }

    std::vector<std::string> parts;
    while (i <= p.size()) {
        size_t j = p.find('/', i);
        if (j == std::string::npos) j = p.size();
        std::string seg = p.substr(i, j - i);
        if (seg.empty() || seg == ".") {
            // skip
        } else if (seg == "..") {
            if (!parts.empty() && parts.back() != "..") parts.pop_back();
            else if (!absolute) parts.push_back("..");
        } else {
            parts.push_back(std::move(seg));
        }
        i = j + 1;
    }

    std::string out = prefix;
    for (size_t k = 0; k < parts.size(); ++k) {
        if (k) out += '/';
        out += parts[k];
    }
    if (out.empty()) out = ".";
    return out;
}

bool isAbsolute(std::string_view p) {
    if (!p.empty() && isSep(p[0])) return true;
    return p.size() >= 3 && isDriveAt(p) && isSep(p[2]);
}

std::string dirname(std::string_view path) {
    std::string p = normalize(path);
    size_t slash = p.rfind('/');
    if (slash == std::string::npos) return isDriveAt(p) ? p.substr(0, 2) : std::string();
    if (slash == 0) return "/";
    if (slash == 1 && p[0] == '/') return "//";
    if (slash == 2 && isDriveAt(p)) return p.substr(0, 3);
    return p.substr(0, slash);
}

std::string filename(std::string_view path) {
    size_t slash = path.find_last_of("/\\");
    return std::string(slash == std::string_view::npos ? path : path.substr(slash + 1));
}

std::string extensionLower(std::string_view path) {
    std::string name = filename(path);
    size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0) return {};
    std::string ext = name.substr(dot);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

std::string join(std::string_view dir, std::string_view rel) {
    if (dir.empty()) return normalize(rel);
    std::string s(dir);
    s += '/';
    s += rel;
    return normalize(s);
}

std::optional<std::string> uriToPath(std::string_view uri) {
    constexpr std::string_view scheme = "file://";
    if (uri.size() < scheme.size()) return std::nullopt;
    for (size_t i = 0; i < scheme.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(uri[i])) != scheme[i]) return std::nullopt;

    std::string_view rest = uri.substr(scheme.size());
    size_t slash = rest.find('/');
    std::string authority(rest.substr(0, slash));
    std::string path = percentDecode(slash == std::string_view::npos ? "" : rest.substr(slash));

    if (!authority.empty() && authority != "localhost")
        return normalize("//" + percentDecode(authority) + path);
    // "/c:/x" → "c:/x"
    if (path.size() >= 3 && path[0] == '/' && isDriveAt(std::string_view(path).substr(1)))
        path.erase(0, 1);
    return normalize(path);
}

std::string pathToUri(std::string_view path) {
    std::string p = normalize(path);
    std::string authority;
    if (p.size() >= 2 && p[0] == '/' && p[1] == '/') {           // UNC
        size_t slash = p.find('/', 2);
        authority = p.substr(2, slash == std::string::npos ? std::string::npos : slash - 2);
        p = slash == std::string::npos ? "/" : p.substr(slash);
    } else if (!p.empty() && p[0] != '/') {
        p.insert(p.begin(), '/');
    }

    static const char* hex = "0123456789ABCDEF";
    std::string out = "file://" + authority;
    for (unsigned char c : p) {
        if (isUnreserved(c) || c == '/') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

ResolvedPath resolveIncludePath(std::string_view raw, std::string_view baseDir,
                                const PathConfig& cfg) {
    if (raw.empty()) return {{}, ResolveError::EmptyPath};

    if (raw.size() >= 3 && raw[0] == '@' && isSep(raw[2])) {
        char folder = static_cast<char>(std::tolower(static_cast<unsigned char>(raw[1])));
        if (folder == 'u' || folder == 's') {
            const std::string& dir = folder == 'u' ? cfg.userProjectsDir : cfg.sampleProjectsDir;
            if (dir.empty())
                return {{}, folder == 'u' ? ResolveError::UserFolderUnset
                                          : ResolveError::SampleFolderUnset};
            return {join(dir, raw.substr(3)), ResolveError::None};
        }
    }

    if (isAbsolute(raw)) return {normalize(raw), ResolveError::None};
    if (baseDir.empty()) return {{}, ResolveError::RelativeWithoutBase};
    return {join(baseDir, raw), ResolveError::None};
}

std::string describe(ResolveError e) {
    switch (e) {
        case ResolveError::None:                return {};
        case ResolveError::EmptyPath:           return "The path is empty.";
        case ResolveError::UserFolderUnset:
            return "The user projects folder (@u/) could not be found. "
                   "Set 'mnemonimov.userProjectsPath' in the settings.";
        case ResolveError::SampleFolderUnset:
            return "The sample projects folder (@s/) could not be found. "
                   "Set 'mnemonimov.sampleProjectsPath' in the settings.";
        case ResolveError::RelativeWithoutBase:
            return "A relative path cannot be resolved in an unsaved document.";
    }
    return {};
}

// ── Default virtual folder detection ─────────────────────────────────────────

namespace {

std::string envVar(const char* name) {
#ifdef _WIN32
    std::wstring wname(name, name + std::strlen(name));
    wchar_t* v = nullptr;
    size_t len = 0;
    if (_wdupenv_s(&v, &len, wname.c_str()) != 0 || !v) return {};
    std::wstring value(v);
    std::free(v);
    if (value.empty()) return {};
    auto u8 = std::filesystem::path(value).generic_u8string();
    return std::string(u8.begin(), u8.end());
#else
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
#endif
}

bool dirExists(const std::string& p) {
    std::error_code ec;
    std::u8string u8(p.begin(), p.end());
    return !p.empty() && std::filesystem::is_directory(std::filesystem::path(u8), ec);
}

std::string readText(const std::string& p) {
    std::u8string u8(p.begin(), p.end());
    std::ifstream f(std::filesystem::path(u8), std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Extra Steam library folders listed in <steam>/steamapps/libraryfolders.vdf.
std::vector<std::string> steamLibraries(const std::string& steamRoot) {
    std::vector<std::string> libs{steamRoot};
    std::string vdf = readText(steamRoot + "/steamapps/libraryfolders.vdf");
    size_t pos = 0;
    while ((pos = vdf.find("\"path\"", pos)) != std::string::npos) {
        pos += 6;
        size_t open = vdf.find('"', pos);
        if (open == std::string::npos) break;
        std::string value;
        size_t i = open + 1;
        for (; i < vdf.size() && vdf[i] != '"'; ++i) {
            if (vdf[i] == '\\' && i + 1 < vdf.size()) ++i; // "D:\\SteamLibrary"
            value += vdf[i];
        }
        pos = i + 1;
        if (!value.empty()) libs.push_back(normalize(value));
    }
    return libs;
}

} // namespace

PathConfig detectDefaultPathConfig() {
    std::vector<std::string> userCandidates, steamRoots;

#ifdef _WIN32
    if (auto appData = envVar("APPDATA"); !appData.empty())
        userCandidates.push_back(appData + "/Mnemonimov/user_projects");
    for (const char* var : {"ProgramFiles(x86)", "ProgramFiles"})
        if (auto pf = envVar(var); !pf.empty()) steamRoots.push_back(pf + "/Steam");
#elif defined(__APPLE__)
    std::string home = envVar("HOME");
    userCandidates.push_back(home + "/Library/Application Support/Mnemonimov/user_projects");
    steamRoots.push_back(home + "/Library/Application Support/Steam");
#else
    std::string home = envVar("HOME");
    std::string xdg  = envVar("XDG_DATA_HOME");
    if (xdg.empty()) xdg = home + "/.local/share";
    userCandidates.push_back(xdg + "/Mnemonimov/user_projects");
    userCandidates.push_back(xdg + "/godot/app_userdata/Mnemonimov/user_projects");
    steamRoots.push_back(home + "/.steam/steam");
    steamRoots.push_back(xdg + "/Steam");
#endif

    PathConfig cfg;
    for (const auto& c : userCandidates)
        if (dirExists(c)) { cfg.userProjectsDir = normalize(c); break; }

    for (const auto& root : steamRoots) {
        if (!dirExists(root)) continue;
        for (const auto& lib : steamLibraries(normalize(root))) {
            std::string samples = lib + "/steamapps/common/Mnemonimov/sample_projects";
            if (dirExists(samples)) { cfg.sampleProjectsDir = normalize(samples); return cfg; }
        }
    }
    return cfg;
}

} // namespace misa::fs
