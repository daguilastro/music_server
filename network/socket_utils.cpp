#include "socket_utils.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

int createTcpServerSocket() {
    int fd = socket(IPv4, STREAM | NO_BLOQUEANTE, 0);
    if (fd < 0) {
        cerr << "error creando el socket: " << strerror(errno) << "\n";
        return -1;
    }
    
    int yes = 1;
    if (setsockopt(fd, SOCKET_LEVEL, REUSE_ADDR, &yes, sizeof(yes)) != 0 || 
        setsockopt(fd, SOCKET_LEVEL, REUSE_PORT, &yes, sizeof(yes)) != 0) {
        cerr << "error cambiando las opciones del socket: " << strerror(errno) << "\n";
        return -1;
    }

    sockaddr_in socketAddress{};
    socketAddress.sin_family = IPv4;
    socketAddress.sin_port = htons(0);
    socketAddress.sin_addr.s_addr = htonl(RECIEVE_EVERYWHERE);

    if (bind(fd, (sockaddr*)&socketAddress, sizeof(socketAddress)) < 0) {
        cerr << "error en el bind del socket: " << strerror(errno) << "\n";
        return -1;
    }

    if (listen(fd, 128) < 0) {
        cerr << "error en el listen del socket: " << strerror(errno) << "\n";
        return -1;
    }

    cout << "Socket creado exitosamente!!\n";
    return fd;
}

int getPort(int socketFd) {
    sockaddr_in serverAddress{};
    socklen_t len = sizeof(serverAddress);
    if (getsockname(socketFd, (sockaddr*)&serverAddress, &len) < 0) {
        cerr << "[ERROR] No se pudo obtener el puerto del servidor: " << strerror(errno) << "\n";
        return -1;
    }
    return ntohs(serverAddress.sin_port);
}

string getLocalIPAddress() {
    cout << "[DEBUG] Obteniendo IP local...\n";
    
    int sock = socket(IPv4, SOCK_DGRAM, 0);
    if (sock < 0) {
        cerr << "[ERROR] No se pudo crear socket para obtener IP local\n";
        return "";
    }
    
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("8.8.8.8");
    server.sin_port = htons(53);
    
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        cerr << "[ERROR] No se pudo obtener IP local: " << strerror(errno) << "\n";
        close(sock);
        return "";
    }
    
    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    if (getsockname(sock, (struct sockaddr*)&name, &namelen) < 0) {
        cerr << "[ERROR] Error en getsockname: " << strerror(errno) << "\n";
        close(sock);
        return "";
    }
    
    char buffer[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &name.sin_addr, buffer, INET_ADDRSTRLEN);
    close(sock);
    
    string localIP(buffer);
    cout << "[DEBUG] IP local detectada: " << localIP << "\n";
    return localIP;
}