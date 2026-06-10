#pragma once
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

namespace misa::transport {

// JSON-RPC 2.0 framing over stdin/stdout using Content-Length headers.
class JsonRpcStream {
public:
    // Must be called once before any I/O on Windows to set binary mode.
    static void init();

    // Block until a complete message arrives. Returns nullopt on EOF or parse error.
    static std::optional<nlohmann::json> readMessage();

    static void writeMessage(const nlohmann::json& msg);
    static void writeResponse(const nlohmann::json& id, const nlohmann::json& result);
    static void writeError(const nlohmann::json& id, int code, const std::string& message);
    static void writeNotification(const std::string& method, const nlohmann::json& params);
};

} // namespace misa::transport
