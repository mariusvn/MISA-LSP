#include "transport/JsonRpcStream.h"
#include <iostream>
#include <string>

#ifdef _WIN32
#  include <io.h>
#  include <fcntl.h>
#endif

namespace misa::transport {

void JsonRpcStream::init() {
#ifdef _WIN32
    _setmode(_fileno(stdin),  _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
}

std::optional<nlohmann::json> JsonRpcStream::readMessage() {
    size_t contentLength = 0;

    // Read headers line by line until blank line
    while (true) {
        std::string line;
        if (!std::getline(std::cin, line)) return std::nullopt;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;

        const std::string prefix = "Content-Length: ";
        if (line.size() > prefix.size() && line.substr(0, prefix.size()) == prefix)
            contentLength = std::stoul(line.substr(prefix.size()));
        // Ignore other headers (e.g. Content-Type)
    }

    if (contentLength == 0) return std::nullopt;

    std::string body(contentLength, '\0');
    if (!std::cin.read(body.data(), static_cast<std::streamsize>(contentLength)))
        return std::nullopt;

    try {
        return nlohmann::json::parse(body);
    } catch (...) {
        return std::nullopt;
    }
}

void JsonRpcStream::writeMessage(const nlohmann::json& msg) {
    const std::string body = msg.dump();
    std::cout
        << "Content-Length: " << body.size() << "\r\n"
        << "\r\n"
        << body
        << std::flush;
}

void JsonRpcStream::writeResponse(const nlohmann::json& id, const nlohmann::json& result) {
    writeMessage({{"jsonrpc", "2.0"}, {"id", id}, {"result", result}});
}

void JsonRpcStream::writeError(const nlohmann::json& id, int code, const std::string& message) {
    writeMessage({
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {{"code", code}, {"message", message}}}
    });
}

void JsonRpcStream::writeNotification(const std::string& method, const nlohmann::json& params) {
    writeMessage({{"jsonrpc", "2.0"}, {"method", method}, {"params", params}});
}

} // namespace misa::transport
