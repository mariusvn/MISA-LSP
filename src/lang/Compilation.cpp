#include "lang/Compilation.h"
#include "lang/Lexer.h"
#include "lang/Parser.h"
#include "lang/SemanticAnalyzer.h"

namespace misa::lang {

Compilation Compilation::build(const std::string& uri, const std::string& text) {
    Compilation c;
    c.uri = uri;
    c.doc.update(text);

    Lexer  lexer(text);
    auto   tokens = lexer.tokenize();

    Parser parser(std::move(tokens));
    c.statements = parser.parse();

    SemanticAnalyzer sa(c.statements, c.doc);
    sa.analyze();
    c.symbols    = std::move(sa.symbolTable());
    c.diagnostics = std::move(sa.diagnostics());

    return c;
}

} // namespace misa::lang
