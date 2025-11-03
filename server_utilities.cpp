#include "server_utilities.hpp"
#include "socket_utilities.hpp"


int createTcpServerSocket(){
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
    socketAdress.sin_port = htons(0);
    socketAdress.sin_addr .s_addr = htonl(RECIEVE_EVERYWHERE); // Toca pasarlo en bytes así se ve mejor

    if (bind(fd, (sockaddr*) &socketAdress, sizeof(socketAdress)) < 0){
        cerr << "error en el bind del socket: " << strerror(errno) << "\n";
    }

    if (listen(fd, 128 < 0)){
        cerr << "error en el listen del socket: " << strerror(errno) << "\n";
    }
     
    cout << "Socket creado exitosamente!!\n";

    return fd;
}