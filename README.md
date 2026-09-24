<div align="center">

<img src="assets/banner.svg" alt="MISA LSP — Language server for the Mnemonimov fantasy console" width="100%">

<br/>

**A fast, dependency-light Language Server for MISA / Mnemonimov assembly — written in modern C++.**

<br/>

[![CI](https://github.com/mariusvn/MISA-LSP/actions/workflows/ci.yml/badge.svg)](https://github.com/mariusvn/MISA-LSP/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-22c55e?style=flat-square)](LICENSE.md)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C?style=flat-square&logo=cmake&logoColor=white)
![LSP](https://img.shields.io/badge/LSP-3.17-7c3aed?style=flat-square)
![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20Linux%20%7C%20macOS-1f2937?style=flat-square)
![Tests](https://img.shields.io/badge/tests-133%20passing-22c55e?style=flat-square)

[**Features**](#-features) · [**Quick start**](#-quick-start) · [**Editor setup**](#-editor-integration) · [**Architecture**](#-architecture) · [**Language**](#-language-at-a-glance) · [**Changelog**](CHANGELOG.md)

</div>

---

## ✨ Features

Everything below works **today**, driven by a single static knowledge base of the full MISA instruction set
(up to date with the **Mnemonimov manual v0.1.6**).

| | Capability | What it does |
|:--:|:--|:--|
| 📚 | **`include` & multi-file projects** | Follows `include "…"` recursively (include-once, cycles ignored), resolves `@u/` / `@s/` virtual folders, and analyses a library through its project's `main.asm` — exactly what the assembler sees |
| 🩺 | **Diagnostics** | Syntax errors, unknown instructions, wrong arity, `int`/`float` mismatches, immediates used as destinations, writes to read-only registers, undefined labels and constants (define-before-use, `undef`), unresolved `@name-`/`@name+`, malformed character literals and escapes, missing include/`emb file` files, missing `exit` in entry points |
| 💡 | **Hover** | Rich docs for every instruction, register (with ABI role), syscall (args & returns), type, condition and built-in symbol — plus constant values, character-literal values, `##` doc comments and the file a symbol comes from |
| ⌨️ | **Completion** | **Context-aware** — proposes types after `lod`/`ste`, conditions after `cmp`, `SYS_*` after `syscall`, registers in operand slots, symbols from every included file |
| 🧭 | **Go to definition** | Jump to any label or constant — across files, qualified names included (`PRINTER.MAPPING`), reusable labels resolved to the right `@name` — or open an included file |
| 🔎 | **Find references** | Every use of a label or constant across all the files of the program |
| 🔗 | **Document links** | `include` and `emb file` paths are clickable |
| 🗂️ | **Document symbols** | Outline with entry-points highlighted and locals nested under their scope |
| ✍️ | **Signature help** | Operand slots per instruction, argument-by-argument syscall hints |
| 📐 | **Folding** | Label scopes, `bmk`/`sbmk` sections, doc-comment blocks |

> 🛰️ Communicates over **JSON-RPC 2.0 / stdio** — drop it into any LSP-capable editor.

---

## 🚀 Quick start

```bash
# Configure & build (first run fetches nlohmann/json + Catch2)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The server binary lands at `build/misa-lsp` (`build/Release/misa-lsp.exe` on Windows).

<details>
<summary>📦 <b>Prerequisites</b></summary>

<br/>

- **CMake** ≥ 3.20
- A **C++20** compiler — MSVC 2022, GCC 12+, or Clang 15+
- Internet access on the first build (dependencies are fetched automatically)

</details>

<details>
<summary>🧪 <b>Run the test suite</b></summary>

<br/>

```bash
cmake -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

</details>

<details>
<summary>🔍 <b>Lint a file from the command line</b></summary>

<br/>

A standalone linter ships alongside the server — handy for CI or a quick check. It follows
`include`s and reports diagnostics for every file of the program:

```bash
build/misa-lint path/to/main.asm
# main.asm:12:5: error: Expected a type (i8t, u8t, …) but got 'u17t'.
# -- 3 file(s), 1 diagnostic(s), 1 error(s)

# The @u/ and @s/ virtual folders are auto-detected; override them with:
build/misa-lint --user-dir ~/Mnemonimov/user_projects --sample-dir /games/Mnemonimov/sample_projects main.asm
```

</details>

---

## 🔌 Editor integration

<details open>
<summary><b>VS Code</b></summary>

<br/>

Register the language and launch the server from your extension:

```ts
import { LanguageClient, ServerOptions, TransportKind } from 'vscode-languageclient/node';

const serverOptions: ServerOptions = {
  command: '/path/to/misa-lsp',
  transport: TransportKind.stdio,
};

const client = new LanguageClient(
  'misa-lsp',
  'MISA Language Server',
  serverOptions,
  {
    documentSelector: [{ scheme: 'file', language: 'mnemonimov' }],
    // Optional: folders behind @u/ and @s/ (auto-detected when empty).
    initializationOptions: { mnemonimov: { userProjectsPath: '', sampleProjectsPath: '' } },
    // Lets the server notice when a closed, included file changes on disk.
    synchronize: { fileEvents: workspace.createFileSystemWatcher('**/{*.asm,*.mnemo,project.mnemonimov}') },
  },
);

client.start();
```

The same settings can be updated later through `workspace/didChangeConfiguration`.

</details>

<details>
<summary><b>Neovim</b> (built-in LSP)</summary>

<br/>

```lua
vim.api.nvim_create_autocmd('FileType', {
  pattern = 'mnemonimov',
  callback = function()
    vim.lsp.start({ name = 'misa-lsp', cmd = { '/path/to/misa-lsp' } })
  end,
})
```

</details>

---

## 🏗️ Architecture

Each file is lexed and parsed once (and cached); a **`Compilation`** is a whole *unit* — a root file plus
everything it includes, expanded in textual order — analysed as one program. The **`Workspace`** picks the
root of each open document (its project's `main.asm` when that includes it), overlays unsaved editor buffers
on the disk, and rebuilds the units a change affects. Every feature provider reads from the unit plus the
static knowledge base.

```
                 ┌─────────── transport ───────────┐
   editor  ⇄     │  JSON-RPC 2.0 over stdio          │
                 └──────────────┬──────────────────┘
                                │
   Workspace (open buffers ⊕ disk, project roots, invalidation)
                                │
   files ─► Lexer ─► ExprParser (Pratt) ─► Parser ─► AST ─► include expansion
                                                         │
                          SemanticAnalyzer (2 passes) ◄──┘     ┌──────────────┐
                                   │                            │ Knowledge    │
                                   ▼                            │ Base (static)│
                              Compilation  ◄───────────────────┤ instructions │
                          (AST · symbols · diagnostics)         │ regs · sys…  │
                                   │                            └──────────────┘
       ┌───────────────────────────┴───────────────────────────────┐
   Diagnostics · Hover · Completion · Definition · References ·
   DocumentSymbols · SignatureHelp · Folding · DocumentLinks
```

```text
src/
  transport/   JSON-RPC stdio framing
  protocol/    LSP types + serialization
  server/      lifecycle, dispatch, workspace (units, project roots)
  fs/          paths, file URIs, virtual folders, source providers (disk / memory / overlay)
  text/        TextDocument (UTF-16 position handling)
  kb/          Knowledge Base (instructions, registers, syscalls…)
  lang/        Lexer → ExprParser → Parser → AST → include expansion → SemanticAnalyzer → Compilation
  features/    one file per LSP feature provider
test/          Catch2 unit tests   ·   tools/  standalone linter
examples/      hello.mnemo · game_skeleton.mnemo · include/ (multi-file project)
```

---

## 📝 Language at a glance

```misa
include "lib/math.asm"        # relative to this file · "@u/lib/main.asm" · "@s/lander/main.asm"

## Move the player and bounce it off the screen edge.
def SPEED 2

player_x: emb i32t 160        # data label, inspectable in the debugger

_update:
    lod  i32t, t0, player_x   # load  (types: i8t/u8t/…/f32t)
    add  t0, SPEED            # compact form: t0 += SPEED
    cmp  gte, t0, SCREEN_WIDTH
    jtr  .wrap
    str  i32t, player_x, t0
    exit
.wrap:
    str  i32t, player_x, zr   # zr always reads 0
    exit
```

| | |
|:--|:--|
| **Files** | `.asm`, `.misa`, `.mnemo` · `include "path"` (recursive, each file once) |
| **Comments** | `#` line · `##` doc-comment |
| **Integers** | `42` · `0x2a` · `0b101010` · `0o52` · `10_000` |
| **Characters** | `'a'` · `'misa'` (up to 4, packed big-endian) · escapes `\0 \t \n \' \" \\` |
| **Floats** | `3.14` (no scientific notation) |
| **Strict typing** | `add 1.0` ❌ (wants int) · `fadd 1` ❌ (wants float) |
| **Labels** | global `foo:` · local `.bar:` · reusable `@loop:` + `@loop-` / `@end+` |

---

## 🤝 Contributing

Issues and pull requests are welcome! Before opening a PR, make sure the build is clean and the tests
pass locally (`ctest --test-dir build --output-on-failure`). New language behaviour should come with a
matching unit test under [`test/`](test).

## 🧠 Development note

This project was built with the assistance of AI coding tools (Claude code). The architecture, code, and tests were
directed, reviewed, built, and validated by a human — including against real-world MISA programs.

## 📄 License

Released under the [**MIT License**](LICENSE.md).

<div align="center"><sub>Made for the Mnemonimov community · happy hacking 🎮</sub></div>
