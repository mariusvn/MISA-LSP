#include "server/DocumentStore.h"

namespace misa::server {

void DocumentStore::update(const std::string& uri, const std::string& text) {
    m_docs[uri] = lang::Compilation::build(uri, text);
}

void DocumentStore::remove(const std::string& uri) {
    m_docs.erase(uri);
}

const lang::Compilation* DocumentStore::get(const std::string& uri) const {
    auto it = m_docs.find(uri);
    return it != m_docs.end() ? &it->second : nullptr;
}

} // namespace misa::server
