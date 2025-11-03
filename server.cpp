#include "server_utilities.hpp"
#include "socket_utilities.hpp"

int main(){
    UPnPRouter router;
    int serverSocket = createTcpServerSocket();
    connectUPnP(serverSocket, 8080, router);
    closeRouterPort(router, 8080);
    close(serverSocket);
    return 0;
}