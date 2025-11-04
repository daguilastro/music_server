#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>

using namespace std;

// ===== CONSTANTES DE SOCKET =====

// DOMAIN OF THE SOCKET
constexpr int IPv4 = AF_INET;
constexpr int IPv6 = AF_INET6;
constexpr int LOCAL = AF_LOCAL;
constexpr int VIRTUAL_MACHINE = AF_VSOCK;
constexpr int BLUETOOTH = AF_BLUETOOTH;
constexpr int NFC = AF_NFC;

// TYPE OF THE SOCKET
constexpr int STREAM = SOCK_STREAM;
constexpr int DGRAM = SOCK_DGRAM;
constexpr int RAW = SOCK_RAW;
constexpr int NO_BLOQUEANTE = SOCK_NONBLOCK;

// OPTIONS FOR THE SOCKET
constexpr int REUSE_ADDR = SO_REUSEADDR;
constexpr int REUSE_PORT = SO_REUSEPORT;
constexpr int SEND_BUFFER_SIZE = SO_SNDBUF;
constexpr int RECIEVE_BUFFER_SIZE = SO_RCVBUF;
constexpr int LOW_LATENCY_MODE = SO_INCOMING_CPU;
constexpr int RECIEVE_EVERYWHERE = INADDR_ANY;
constexpr int SOCKET_LEVEL = SOL_SOCKET;

// ===== FUNCIONES PÚBLICAS =====

int createTcpServerSocket();
int getPort(int socketFd);
string getLocalIPAddress();