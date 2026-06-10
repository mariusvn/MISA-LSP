#include "server/Server.h"
#include "protocol/Serialization.h"
#include "features/Diagnostics.h"
#include "features/Hover.h"
#include "features/Completion.h"
#include "features/Definition.h"
#include "features/References.h"
#include "features/DocumentSymbols.h"
#include "features/SignatureHelp.h"
#include "features/Folding.h"
#include <nlohmann/json.hpp>

namespace misa::server {

// ── Helpers ───────────────────────────────────────────────────────────────────

lsp::Position Server::getPosition(const nlohmann::json& params) {
    const auto& pos = params["position"];
    return lsp::Position{
        pos.value("line",      0u),
        pos.value("character", 0u)
    };
}

std::string Server::getUri(const nlohmann::json& params) {
    return params["textDocument"].value("uri", std::string{});
}

void Server::publishDiagnostics(const std::string& uri,
                                const std::vector<lsp::Diagnostic>& diags) {
    nlohmann::json items = nlohmann::json::array();
    for (const auto& d : diags) {
        nlohmann::json j;
        lsp::to_json(j, d);
        items.push_back(j);
    }
    transport::JsonRpcStream::writeNotification(
        "textDocument/publishDiagnostics",
        {{"uri", uri}, {"diagnostics", items}});
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

nlohmann::json Server::onInitialize(const nlohmann::json& /*params*/) {
    m_initialized = true;
    return {
        {"capabilities", lsp::makeServerCapabilities()},
        {"serverInfo", {{"name", "misa-lsp"}, {"version", "0.1.0"}}}
    };
}

void Server::onInitialized(const nlohmann::json& /*params*/) {}

nlohmann::json Server::onShutdown(const nlohmann::json& /*params*/) {
    m_shutdown = true;
    return nullptr;
}

void Server::onExit(const nlohmann::json& /*params*/) {}

// ── Document sync ─────────────────────────────────────────────────────────────

void Server::onDidOpen(const nlohmann::json& params) {
    const auto& td = params["textDocument"];
    std::string uri  = td.value("uri", std::string{});
    std::string text = td.value("text", std::string{});
    m_store.update(uri, text);
    if (const auto* c = m_store.get(uri))
        publishDiagnostics(uri, c->diagnostics);
}

void Server::onDidChange(const nlohmann::json& params) {
    std::string uri = getUri(params);
    const auto& changes = params["contentChanges"];
    if (!changes.is_array() || changes.empty()) return;
    // Full sync: take the last change's text
    std::string text = changes.back().value("text", std::string{});
    m_store.update(uri, text);
    if (const auto* c = m_store.get(uri))
        publishDiagnostics(uri, c->diagnostics);
}

void Server::onDidClose(const nlohmann::json& params) {
    m_store.remove(getUri(params));
}

// ── Feature providers ─────────────────────────────────────────────────────────

nlohmann::json Server::onHover(const nlohmann::json& params) {
    std::string uri = getUri(params);
    const auto* c = m_store.get(uri);
    if (!c) return nullptr;
    auto hover = features::provideHover(*c, getPosition(params));
    if (!hover) return nullptr;
    nlohmann::json j;
    lsp::to_json(j, *hover);
    return j;
}

nlohmann::json Server::onCompletion(const nlohmann::json& params) {
    std::string uri = getUri(params);
    const auto* c = m_store.get(uri);
    if (!c) return nlohmann::json::array();
    auto list = features::provideCompletion(*c, getPosition(params));
    nlohmann::json j;
    lsp::to_json(j, list);
    return j;
}

nlohmann::json Server::onDefinition(const nlohmann::json& params) {
    std::string uri = getUri(params);
    const auto* c = m_store.get(uri);
    if (!c) return nullptr;
    auto locs = features::provideDefinition(*c, getPosition(params), uri);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& l : locs) {
        nlohmann::json j;
        lsp::to_json(j, l);
        arr.push_back(j);
    }
    return arr;
}

nlohmann::json Server::onReferences(const nlohmann::json& params) {
    std::string uri = getUri(params);
    const auto* c = m_store.get(uri);
    if (!c) return nlohmann::json::array();
    bool includeDecl = params.value("context", nlohmann::json{})
                             .value("includeDeclaration", false);
    auto locs = features::provideReferences(*c, getPosition(params), uri, includeDecl);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& l : locs) {
        nlohmann::json j;
        lsp::to_json(j, l);
        arr.push_back(j);
    }
    return arr;
}

nlohmann::json Server::onDocumentSymbol(const nlohmann::json& params) {
    std::string uri = getUri(params);
    const auto* c = m_store.get(uri);
    if (!c) return nlohmann::json::array();
    auto syms = features::provideDocumentSymbols(*c);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& s : syms) {
        nlohmann::json j;
        lsp::to_json(j, s);
        arr.push_back(j);
    }
    return arr;
}

nlohmann::json Server::onSignatureHelp(const nlohmann::json& params) {
    std::string uri = getUri(params);
    const auto* c = m_store.get(uri);
    if (!c) return nullptr;
    auto help = features::provideSignatureHelp(*c, getPosition(params));
    if (!help) return nullptr;
    nlohmann::json j;
    lsp::to_json(j, *help);
    return j;
}

nlohmann::json Server::onFoldingRange(const nlohmann::json& params) {
    std::string uri = getUri(params);
    const auto* c = m_store.get(uri);
    if (!c) return nlohmann::json::array();
    auto ranges = features::provideFoldingRanges(*c);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& fr : ranges) {
        nlohmann::json j;
        lsp::to_json(j, fr);
        arr.push_back(j);
    }
    return arr;
}

// ── Dispatch ──────────────────────────────────────────────────────────────────

void Server::registerHandlers() {
    // Requests (have an id, need a response)
    m_handlers["initialize"]                = [this](const auto& p){ return onInitialize(p); };
    m_handlers["shutdown"]                  = [this](const auto& p){ return onShutdown(p); };
    m_handlers["textDocument/hover"]        = [this](const auto& p){ return onHover(p); };
    m_handlers["textDocument/completion"]   = [this](const auto& p){ return onCompletion(p); };
    m_handlers["textDocument/definition"]   = [this](const auto& p){ return onDefinition(p); };
    m_handlers["textDocument/references"]   = [this](const auto& p){ return onReferences(p); };
    m_handlers["textDocument/documentSymbol"]=[this](const auto& p){ return onDocumentSymbol(p); };
    m_handlers["textDocument/signatureHelp"]= [this](const auto& p){ return onSignatureHelp(p); };
    m_handlers["textDocument/foldingRange"] = [this](const auto& p){ return onFoldingRange(p); };
}

void Server::dispatch(const nlohmann::json& msg) {
    const std::string method = msg.value("method", std::string{});
    const bool hasId = msg.contains("id") && !msg["id"].is_null();
    const nlohmann::json params = msg.value("params", nlohmann::json::object());
    const nlohmann::json id     = msg.value("id", nlohmann::json(nullptr));

    // Notifications (no id)
    if (!hasId) {
        if (method == "initialized")              onInitialized(params);
        else if (method == "textDocument/didOpen")   onDidOpen(params);
        else if (method == "textDocument/didChange") onDidChange(params);
        else if (method == "textDocument/didClose")  onDidClose(params);
        else if (method == "exit")                   onExit(params);
        return;
    }

    // Requests
    auto it = m_handlers.find(method);
    if (it == m_handlers.end()) {
        // Method not found: -32601
        transport::JsonRpcStream::writeError(id, -32601,
            "Method not found: " + method);
        return;
    }

    if (!m_initialized && method != "initialize") {
        transport::JsonRpcStream::writeError(id, -32002, "Server not initialized.");
        return;
    }

    nlohmann::json result;
    try {
        result = it->second(params);
    } catch (const std::exception& e) {
        transport::JsonRpcStream::writeError(id, -32603,
            std::string("Internal error: ") + e.what());
        return;
    }

    transport::JsonRpcStream::writeResponse(id, result);
}

int Server::run() {
    registerHandlers();

    while (true) {
        auto msg = transport::JsonRpcStream::readMessage();
        if (!msg) break; // EOF

        dispatch(*msg);

        // Honour shutdown+exit sequence
        if (m_shutdown) {
            // Wait for exit notification
            while (true) {
                auto m2 = transport::JsonRpcStream::readMessage();
                if (!m2) return 1;
                if (m2->value("method", std::string{}) == "exit") return 0;
            }
        }
    }
    return 0;
}

} // namespace misa::server
