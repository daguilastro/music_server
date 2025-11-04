#pragma once
#include <map>
#include <ratio>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>  

using namespace std;

// Variables globales
extern std::map<int, std::string> clientBuffers;

// Funciones
int acceptNewClient(int serverSocket, int epollFd);
int receiveFromClient(int clientFd, int epollFd);
void processCommands(int clientFd);
void disconnectClient(int clientFd, int epollFd);
void closeAllClients();

// Handlers para epoll
void handleServerEvent(int fd, void* data);
void handleClientEvent(int fd, void* data);