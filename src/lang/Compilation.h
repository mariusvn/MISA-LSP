#pragma once
#include "lang/Ast.h"
#include "lang/SymbolTable.h"
#include "protocol/LspTypes.h"
#include "text/TextDocument.h"
#include <string>
#include <vector>

namespace misa::lang {

// A fully analysed document: tokens, AST, symbols, diagnostics.
// Rebuilt from scratch on every didOpen / didChange (Full sync).
struct Compilation {
    std::string                  uri;
    text::TextDocument           doc;
    std::vector<Statement>       statements;
    SymbolTable                  symbols;
    std::vector<lsp::Diagnostic> diagnostics;

    // Build from raw text. Never throws — errors become diagnostics.
    static Compilation build(const std::string& uri, const std::string& text);
};

} // namespace misa::lang
