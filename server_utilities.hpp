#pragma once

#include "socket_utilities.hpp"
#include <sys/epoll.h>
#include <fcntl.h>
#include <map>

using namespace std;

extern std::map<int, std::string> clientBuffers;

int createTcpServerSocket();
int crearEpoll();
int addSocketToEpoll(int epollFd, int listenSocket);
int acceptNewClient(int serverSocket, int epollFd);
int mainloop(int& serverSocket, sockaddr_in ipBind);
void runServer(int& serverSocket);
void processCommands(int clientFd);
void handleCommand(int clientFd, string command);
int receiveFromClient(int clientFd, int epollFd);