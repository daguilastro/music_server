#include "server/server.hpp"
#include "network/socket_utils.hpp"

int main() {
    int serverSocket = createTcpServerSocket();
    if (serverSocket < 0) {
        return 1;
    }
    
    runServer(serverSocket);
    return 0;
}