#pragma once
#include "server/DocumentStore.h"
#include "transport/JsonRpcStream.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <functional>

namespace misa::server {

class Server {
public:
    // Run the main request loop until shutdown+exit or EOF. Returns exit code.
    int run();

private:
    DocumentStore m_store;
    bool          m_initialized = false;
    bool          m_shutdown    = false;

    using Handler = std::function<nlohmann::json(const nlohmann::json&)>;
    std::unordered_map<std::string, Handler> m_handlers;

    void registerHandlers();

    void dispatch(const nlohmann::json& msg);

    // LSP lifecycle
    nlohmann::json onInitialize(const nlohmann::json& params);
    void           onInitialized(const nlohmann::json& params);
    nlohmann::json onShutdown(const nlohmann::json& params);
    void           onExit(const nlohmann::json& params);

    // Document sync
    void onDidOpen  (const nlohmann::json& params);
    void onDidChange(const nlohmann::json& params);
    void onDidClose (const nlohmann::json& params);

    // Feature providers
    nlohmann::json onHover         (const nlohmann::json& params);
    nlohmann::json onCompletion    (const nlohmann::json& params);
    nlohmann::json onDefinition    (const nlohmann::json& params);
    nlohmann::json onReferences    (const nlohmann::json& params);
    nlohmann::json onDocumentSymbol(const nlohmann::json& params);
    nlohmann::json onSignatureHelp (const nlohmann::json& params);
    nlohmann::json onFoldingRange  (const nlohmann::json& params);

    // Push diagnostics after every document change
    void publishDiagnostics(const std::string& uri,
                            const std::vector<lsp::Diagnostic>& diags);

    // Helper: extract position from a request
    static lsp::Position getPosition(const nlohmann::json& params);
    static std::string   getUri     (const nlohmann::json& params);
};

} // namespace misa::server
