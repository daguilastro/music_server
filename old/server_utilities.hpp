#pragma once

#include "socket_utilities.hpp"
#include <string>
#include <sys/epoll.h>
#include <fcntl.h>
#include <map>
#include <queue>
#include "worker.hpp"

using namespace std;

extern std::map<int, std::string> clientBuffers;
extern bool serverRunning;
extern queue<string> downloadQueue;
extern map<string, void(*)(int, string)> commandHandlers;
extern vector<WorkerInfo> workers;


int createTcpServerSocket();
int crearEpoll();

// Handlers
void handleServerEvent(int fd, void* data);
void handleClientEvent(int fd, void* data);
void handleWorkerEvent(int fd, void* data);
void handleCommand(int clientFd, string command);
void handleAddCommand(int clientFd, string args);
void handleExitCommand(int clientFd, string args);

int addToEpoll(int epollFd, int fd, void (*handler)(int, void*), void* data);
int acceptNewClient(int serverSocket, int epollFd);
int mainloop(int& serverSocket, sockaddr_in ipBind);
void runServer(int& serverSocket);
void processCommands(int clientFd);
bool initializeWorkers(int epollFd);

int receiveFromClient(int clientFd, int epollFd);
bool readWorkerMessage(int fd, WorkerMessage& msg);
void closeServer();