#include "features/DocumentLinks.h"

namespace misa::features {

using namespace misa::lang;
using namespace misa::lsp;

// Range of a string literal without its quotes.
static Range innerRange(const SourceFile& f, Span sp) {
    Span inner = sp;
    const std::string& text = f.doc().text();
    if (inner.length() >= 1 && text[inner.start] == '"') ++inner.start;
    if (inner.end > inner.start && text[inner.end - 1] == '"') --inner.end;
    return f.range(inner);
}

std::vector<DocumentLink> provideDocumentLinks(const Compilation& c, const SourceFile& f) {
    std::vector<DocumentLink> links;
    for (const auto& inc : f.includes) {
        if (!inc.target) continue; // not found / unresolved
        DocumentLink link;
        link.range   = innerRange(f, inc.pathSpan);
        link.target  = c.files[*inc.target].uri;
        link.tooltip = inc.resolvedPath;
        links.push_back(std::move(link));
    }
    for (const auto& emb : f.embeddedFiles) {
        if (!emb.exists) continue;
        DocumentLink link;
        link.range   = innerRange(f, emb.pathSpan);
        link.target  = fs::pathToUri(emb.resolvedPath);
        link.tooltip = emb.resolvedPath;
        links.push_back(std::move(link));
    }
    return links;
}

} // namespace misa::features
