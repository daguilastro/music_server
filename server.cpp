#include "server_utilities.hpp"
#include "socket_utilities.hpp"

int main(){
    int serverSocket = createTcpServerSocket();
    connectLocal(serverSocket);
    close(serverSocket);
    return 0;
}