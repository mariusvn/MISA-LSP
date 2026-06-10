#pragma once
#include "lang/Compilation.h"
#include <string>
#include <unordered_map>

namespace misa::server {

// Caches one Compilation per open document URI.
class DocumentStore {
public:
    // Called on textDocument/didOpen and textDocument/didChange (full sync).
    void update(const std::string& uri, const std::string& text);

    // Called on textDocument/didClose.
    void remove(const std::string& uri);

    // Returns nullptr if the document is not open.
    const lang::Compilation* get(const std::string& uri) const;

private:
    std::unordered_map<std::string, lang::Compilation> m_docs;
};

} // namespace misa::server
