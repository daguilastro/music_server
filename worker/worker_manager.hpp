#pragma once

#include "worker.hpp"
#include <vector>
#include <queue>
#include <string>

using namespace std;

// Variables globales
extern vector<WorkerInfo> workers;
extern queue<string> downloadQueue;
extern uint32_t nextRequestId;

// Funciones
bool initializeWorkers(int epollFd, int numWorkers = 4);
void submitDownload(const string& url);
void assignPendingDownloads();
void shutdownWorkers();

// Handler para epoll
void handleWorkerEvent(int fd, void* data);