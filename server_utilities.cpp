#include "server_utilities.hpp"
#include "socket_utilities.hpp"
#include <netinet/in.h>

int createTcpServerSocket(int port){
    int fd = socket(IPv4, STREAM | NO_BLOQUEANTE, 0);
    if (fd < 0){
        cerr << "error creando el socket: " << strerror(errno) << "\n";
    }
    int yes = 1;
    if(
        setsockopt(fd, SOCKET_LEVEL, REUSE_ADDR, &yes, sizeof(yes)) != 0 ||
        setsockopt(fd, SOCKET_LEVEL, REUSE_PORT, &yes, sizeof(yes)) != 0
    ){
        cerr << "error cambiando las opciones del socket: " << strerror(errno) << "\n";
    }

    sockaddr_in socketAdress{};
    socketAdress.sin_family = IPv4;
    socketAdress.sin_port = htons(port);
    socketAdress.sin_addr .s_addr = htonl("192.127.0.0");

    return fd;
}