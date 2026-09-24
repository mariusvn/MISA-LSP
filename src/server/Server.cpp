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
#include "features/DocumentLinks.h"
#include <nlohmann/json.hpp>

namespace misa::server {

Server::Server() : m_ws(std::make_unique<fs::DiskSourceProvider>()) {}

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

void Server::flushDiagnostics() {
    for (const auto& p : m_ws.takePendingDiagnostics())
        publishDiagnostics(p.uri, p.diagnostics);
}

template <typename T>
static nlohmann::json toJsonArray(const std::vector<T>& items) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& item : items) {
        nlohmann::json j;
        lsp::to_json(j, item);
        arr.push_back(j);
    }
    return arr;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

nlohmann::json Server::onInitialize(const nlohmann::json& params) {
    m_initialized = true;
    applySettings(params.value("initializationOptions", nlohmann::json::object()));
    return {
        {"capabilities", lsp::makeServerCapabilities()},
        {"serverInfo", {{"name", "misa-lsp"}, {"version", "1.1.1"}}}
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
    m_ws.open(td.value("uri", std::string{}), td.value("text", std::string{}));
}

void Server::onDidChange(const nlohmann::json& params) {
    const auto& changes = params["contentChanges"];
    if (!changes.is_array() || changes.empty()) return;
    // Full sync: take the last change's text
    m_ws.change(getUri(params), changes.back().value("text", std::string{}));
}

void Server::onDidClose(const nlohmann::json& params) {
    m_ws.close(getUri(params));
}

// ── Workspace ─────────────────────────────────────────────────────────────────

void Server::applySettings(const nlohmann::json& settings) {
    if (!settings.is_object()) return;
    // Accept both {"mnemonimov": {...}} and the bare section.
    const nlohmann::json& s = settings.contains("mnemonimov") ? settings["mnemonimov"] : settings;
    if (!s.is_object()) return;

    fs::PathConfig detected = fs::detectDefaultPathConfig();
    auto pick = [](const nlohmann::json& obj, const char* name, const std::string& fallback) {
        std::string v = obj.contains(name) && obj[name].is_string() ? obj[name].get<std::string>() : "";
        return v.empty() ? fallback : fs::normalize(v);
    };
    fs::PathConfig cfg;
    cfg.userProjectsDir   = pick(s, "userProjectsPath",   detected.userProjectsDir);
    cfg.sampleProjectsDir = pick(s, "sampleProjectsPath", detected.sampleProjectsDir);
    m_ws.setPathConfig(std::move(cfg));
}

void Server::onDidChangeConfiguration(const nlohmann::json& params) {
    applySettings(params.value("settings", nlohmann::json::object()));
}

void Server::onDidChangeWatchedFiles(const nlohmann::json& params) {
    const auto changes = params.value("changes", nlohmann::json::array());
    for (const auto& ch : changes) {
        int type = ch.value("type", 2);
        m_ws.fileChanged(ch.value("uri", std::string{}),
                         static_cast<Workspace::FileChange>(type >= 1 && type <= 3 ? type : 2));
    }
}

// ── Feature providers ─────────────────────────────────────────────────────────

nlohmann::json Server::onHover(const nlohmann::json& params) {
    auto t = m_ws.lookup(getUri(params));
    if (!t) return nullptr;
    auto hover = features::provideHover(*t->unit, *t->file, getPosition(params));
    if (!hover) return nullptr;
    nlohmann::json j;
    lsp::to_json(j, *hover);
    return j;
}

nlohmann::json Server::onCompletion(const nlohmann::json& params) {
    auto t = m_ws.lookup(getUri(params));
    if (!t) return nlohmann::json::array();
    auto list = features::provideCompletion(*t->unit, *t->file, getPosition(params));
    nlohmann::json j;
    lsp::to_json(j, list);
    return j;
}

nlohmann::json Server::onDefinition(const nlohmann::json& params) {
    auto t = m_ws.lookup(getUri(params));
    if (!t) return nullptr;
    return toJsonArray(features::provideDefinition(*t->unit, *t->file, getPosition(params)));
}

nlohmann::json Server::onReferences(const nlohmann::json& params) {
    auto t = m_ws.lookup(getUri(params));
    if (!t) return nlohmann::json::array();
    bool includeDecl = params.value("context", nlohmann::json{})
                             .value("includeDeclaration", false);
    return toJsonArray(features::provideReferences(*t->unit, *t->file, getPosition(params), includeDecl));
}

nlohmann::json Server::onDocumentSymbol(const nlohmann::json& params) {
    auto t = m_ws.lookup(getUri(params));
    if (!t) return nlohmann::json::array();
    return toJsonArray(features::provideDocumentSymbols(*t->unit, *t->file));
}

nlohmann::json Server::onSignatureHelp(const nlohmann::json& params) {
    auto t = m_ws.lookup(getUri(params));
    if (!t) return nullptr;
    auto help = features::provideSignatureHelp(*t->unit, *t->file, getPosition(params));
    if (!help) return nullptr;
    nlohmann::json j;
    lsp::to_json(j, *help);
    return j;
}

nlohmann::json Server::onFoldingRange(const nlohmann::json& params) {
    auto t = m_ws.lookup(getUri(params));
    if (!t) return nlohmann::json::array();
    return toJsonArray(features::provideFoldingRanges(*t->unit, *t->file));
}

nlohmann::json Server::onDocumentLink(const nlohmann::json& params) {
    auto t = m_ws.lookup(getUri(params));
    if (!t) return nlohmann::json::array();
    return toJsonArray(features::provideDocumentLinks(*t->unit, *t->file));
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
    m_handlers["textDocument/documentLink"] = [this](const auto& p){ return onDocumentLink(p); };
}

void Server::dispatch(const nlohmann::json& msg) {
    const std::string method = msg.value("method", std::string{});
    const bool hasId = msg.contains("id") && !msg["id"].is_null();
    const nlohmann::json params = msg.value("params", nlohmann::json::object());
    const nlohmann::json id     = msg.value("id", nlohmann::json(nullptr));

    // Notifications (no id)
    if (!hasId) {
        try {
            if (method == "initialized")              onInitialized(params);
            else if (method == "textDocument/didOpen")   onDidOpen(params);
            else if (method == "textDocument/didChange") onDidChange(params);
            else if (method == "textDocument/didClose")  onDidClose(params);
            else if (method == "workspace/didChangeConfiguration") onDidChangeConfiguration(params);
            else if (method == "workspace/didChangeWatchedFiles")  onDidChangeWatchedFiles(params);
            else if (method == "exit")                   onExit(params);
        } catch (const std::exception& e) {
            transport::JsonRpcStream::writeNotification("window/logMessage",
                {{"type", 1}, {"message", std::string("misa-lsp: ") + method + ": " + e.what()}});
        }
        flushDiagnostics();
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
    flushDiagnostics();
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
