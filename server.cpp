#include "server_utilities.hpp"
#include "socket_utilities.hpp"

int main(){
    int serverSocket = createTcpServerSocket();
    connectUPnP(serverSocket, 8080);
    return 0;
}