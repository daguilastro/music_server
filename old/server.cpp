#include "server_utilities.hpp"
#include "socket_utilities.hpp"

int main(){
    int serverSocket = createTcpServerSocket();
    runServer(serverSocket);
    return 0;
}