# Changelog

All notable changes to **MISA-LSP** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.1.1] — 2026-09-25

### Changed

- `.misa` files are accepted as MISA sources: including one no longer warns that included
  files should be `.asm` ([vscode-menmonimov-asm#1](https://github.com/mariusvn/vscode-menmonimov-asm/issues/1)).
- `serverInfo.version` is now `1.1.1`.

## [1.1.0] — 2026-09-25

Brings the server up to date with **Mnemonimov manual v0.1.6** and makes it understand
multi-file programs.

### Added

- **`include` directive and multi-file units**
  - `include "path"` is expanded in place, recursively, and each file only once (include-once
    by resolved path), so cycles stop on their own. This matches the assembler.
  - Relative paths resolve from the including file. Absolute paths are supported, and so are
    the virtual folders `@u/` (user projects) and `@s/` (sample projects).
  - A document belongs to a **compilation unit**: when a library sits in a project (a folder
    with `project.mnemonimov`) whose `main.asm` includes it, it is analysed through that
    `main.asm`. Otherwise it is analysed on its own, with its own includes.
  - Open editor buffers take precedence over the files on disk. Editing any file of a unit
    rebuilds that unit and republishes diagnostics for every open file that changed.
  - Go to definition, find references, hover and completion work across files. Go to
    definition on an include path opens the file.
  - **Document links** (`textDocument/documentLink`) for `include` and `emb file` paths.
  - Diagnostics:
    - unresolvable or missing files;
    - duplicate and circular includes (hints, tagged *unnecessary*);
    - a summary on the include line when an included file has errors;
    - missing or wrongly typed `emb file` assets.
  - Settings `userProjectsPath` / `sampleProjectsPath`, read from `initializationOptions` and
    `workspace/didChangeConfiguration`, and auto-detected when empty (`%APPDATA%`, XDG data
    dir, Steam libraries).
  - `workspace/didChangeWatchedFiles` support, so included files that are closed are re-read
    when they change on disk.
- **Character literals** `'a'` … `'abcd'`, packed big-endian (`'ab'` = `0x6162`). The escape
  sequences `\0 \t \n \' \" \\` work in character and string literals. Hovering a character
  literal shows its value.
- **Knowledge base**
  - Instructions: `cala`, `jmpa`, `jtra`, `jfsa`, `fma`, `mlhu`, `divu`, `remu`, `minu`, `maxu`,
    `clpu` (121 in total).
  - Condition: `gtu`.
  - Syscalls: `SYS_PRINT_LINE_INT`, `SYS_PRINT_LINE_FLOAT`, `SYS_PRINT_LINE_STRING`,
    `SYS_GET_MOUSE_POSITION`, `SYS_GET_MOUSE_BUTTON_INPUT`, `SYS_GET_KEYBOARD_INPUT`,
    `SYS_GET_TERMINAL_INPUT_SIZE`, `SYS_READ_TERMINAL_INPUT`, `SYS_ALLOW_UNSAFE_JUMP`.
  - Built-ins: `MAX_TERMINAL_INPUT_SIZE`, `MOUSE_BTN_*`, `KBE_*`, `KEY_*`.
  - Entry points: `_mouse_button_input`, `_keyboard_input`, `_terminal_input`.
- **Syntax diagnostics** from the lexer and the parser:
  - unexpected characters;
  - unterminated strings and character literals;
  - unknown escapes;
  - empty or overlong character literals;
  - a missing `)`;
  - tokens left over after a statement;
  - `include` without a path.
- **Semantic checks**
  - Undefined names inside compound expressions and in `emb`/`res` values.
  - Constants used before their `def` or after their `undef`.
  - Immediates or labels used as destinations.
  - Embed-only types in `lod`/`str`/`res`.
  - A mismatch between an `emb` type and its values.
  - Unresolved `@name-` / `@name+` references.
- **Constant evaluation.** Hover shows the value of a `def`, and a float constant used where an
  integer is expected (or the reverse) gets a warning.
- **Hover** shows the `##` doc comment above a label and the file a symbol is defined in.
- `misa-lint` follows includes and prints diagnostics for every file of the unit.
  - It checks a library through its project's `main.asm`.
  - New options `--user-dir` and `--sample-dir`.
- Example project `examples/include/`.
- Tests grew from 82 to 133: paths and URIs, include expansion, workspace, character literals,
  and the new checks.

### Changed

- `DocumentStore` is replaced by `Workspace`, which picks each document's root, tracks
  dependencies between files and only republishes diagnostics that changed.
- `Compilation` is now a multi-file unit (`files`, `order`, `symbols`). Every symbol, reference
  and diagnostic records which file it belongs to. Feature providers take `(unit, file, position)`,
  and root-only overloads are kept.
- Labels (global, local, reusable) are all collected in pass 1, so forward references such as
  `jmp DATA.end` resolve.
- Every reference is resolved to its definition, which makes definition, references and hover
  exact and scope-aware.
- `serverInfo.version` is now `1.1.0`.

### Fixed

- Reusable label references were never linked to a definition.
- `undef` did not remove the constant.
- `didClose` did not clear the file's diagnostics.
- Signature help ignored instructions that follow a `label:` or are indented with tabs.
- The outline dropped local labels whose global label lives in another file.

## [1.0] — 2026-06-10

### Added

- First release:
  - JSON-RPC 2.0 over stdio, with full document sync;
  - diagnostics, hover, context-aware completion, go to definition, references, document
    symbols, signature help and folding for single MISA files;
  - `misa-lint` CLI and Catch2 test suite.
