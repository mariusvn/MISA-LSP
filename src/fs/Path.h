#pragma once
#include <optional>
#include <string>
#include <string_view>

// Pure string helpers for file paths and file:// URIs.
// Paths are always handled in a normalised form using '/' separators and a
// lowercase drive letter ("c:/Users/x.asm"), so results are identical on every
// platform and in tests. Nothing here touches the file system.
namespace misa::fs {

// '\' → '/', collapses "//" and "." segments, resolves "..", lowercases a drive
// letter. A leading "//" (UNC) is preserved. ".." never climbs above the root.
std::string normalize(std::string_view path);

bool        isAbsolute(std::string_view path);   // "/x", "c:/x", "c:\x", "//server/x"
std::string dirname(std::string_view path);      // "a/b/c.asm" → "a/b"
std::string filename(std::string_view path);     // "a/b/c.asm" → "c.asm"
std::string extensionLower(std::string_view path); // "a/B.ASM" → ".asm"
std::string join(std::string_view dir, std::string_view rel);

// file:// URI → normalised path. Returns nullopt for other schemes (untitled:).
//   "file:///c%3A/Users/a%20b/x.asm" → "c:/Users/a b/x.asm"
//   "file:///home/x.asm"            → "/home/x.asm"
//   "file://server/share/x.asm"     → "//server/share/x.asm"
std::optional<std::string> uriToPath(std::string_view uri);

// Normalised path → file:// URI in the form VS Code uses ("file:///c%3A/x%20y").
std::string pathToUri(std::string_view path);

// Directories behind the virtual folders of the Mnemonimov console.
struct PathConfig {
    std::string userProjectsDir;   // "@u/"
    std::string sampleProjectsDir; // "@s/"
};

enum class ResolveError { None, EmptyPath, UserFolderUnset, SampleFolderUnset, RelativeWithoutBase };

struct ResolvedPath {
    std::string  path;  // normalised; empty on error
    ResolveError error = ResolveError::None;
};

// Resolves an include / emb file path:
//   "@u/…" and "@s/…" → the configured virtual folder,
//   absolute paths    → as is,
//   anything else     → relative to `baseDir` (the including file's directory).
ResolvedPath resolveIncludePath(std::string_view raw, std::string_view baseDir,
                                const PathConfig& cfg);

// Human-readable explanation of a ResolveError.
std::string describe(ResolveError e);

// Best-effort location of the console's user and sample project folders
// (%APPDATA%/Mnemonimov/user_projects, the Steam install, …). Only returns
// directories that exist.
PathConfig detectDefaultPathConfig();

} // namespace misa::fs
