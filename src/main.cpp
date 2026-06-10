#include "transport/JsonRpcStream.h"
#include "server/Server.h"
#include <cstdlib>

int main() {
    misa::transport::JsonRpcStream::init();
    misa::server::Server server;
    return server.run();
}
