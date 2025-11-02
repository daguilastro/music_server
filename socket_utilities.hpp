#pragma once

#include <asm-generic/socket.h>
#include <cstddef>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>


using namespace std;


struct UPnPURL{
    string ip;
    int port;
    string path;
};

// Rename constants for clarity //

// DOMAIN OF THE SOCKET: Los tipos de address que se van a usar y el protocolo de red que se usa para estos
constexpr int IPv4 = AF_INET;
constexpr int IPv6 = AF_INET6;
constexpr int LOCAL = AF_LOCAL;
constexpr int VIRTUAL_MACHINE = AF_VSOCK;
constexpr int BLUETOOTH = AF_BLUETOOTH;
constexpr int NFC = AF_NFC;

// TYPE OF THE SOCKET: How the socket will recieve information

constexpr int STREAM = SOCK_STREAM;
constexpr int DGRAM = SOCK_DGRAM;
constexpr int RAW = SOCK_RAW;

constexpr int NO_BLOQUEANTE = SOCK_NONBLOCK;

// OPTIONS FOR THE SOCKET

constexpr int REUSE_ADDR = SO_REUSEADDR;
constexpr int REUSE_PORT = SO_REUSEPORT; //  Sirve para que muchos sockets se puedan conectar a la misma ip para que así se puedan usar varios servidores a la vez
constexpr int SEND_BUFFER_SIZE = SO_SNDBUF; // Opción para cambiar el tamaño del buffer de send
constexpr int RECIEVE_BUFFER_SIZE = SO_RCVBUF; // Opción para cambiar el tamaño del buffer de send
constexpr int LOW_LATENCY_MODE = SO_INCOMING_CPU;
constexpr int RECIEVE_EVERYWHERE = INADDR_ANY;

constexpr int SOCKET_LEVEL = SOL_SOCKET;

int createSearchSocket();
void askRouterUPnPURL(int searchSocket);
int connectUPnP();
UPnPURL parseURL(string &stringURL);