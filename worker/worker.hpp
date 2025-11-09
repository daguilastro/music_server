#pragma once

#include <sys/types.h>
#include <string>
#include <cstdint>
#include <unistd.h>
#include <iostream>
#include "../indexation/database.hpp"
#include <cstring>
#include <sys/wait.h>
#include <cerrno>


using namespace std;

// Worker states
#define WORKER_IDLE 0
#define WORKER_BUSY 1

// Message types for pipe communication
#define MSG_REQUEST  1
#define MSG_FINISHED 2
#define MSG_SHUTDOWN 3
#define MSG_METADATA 4

// Message structure for pipe communication
struct WorkerMessage {
    uint8_t type;
    uint32_t data_length;
    char data[2048];
    
    WorkerMessage() : type(MSG_FINISHED), data_length(0) {
        data[0] = '\0';
    }
};


struct DownloadRequest {
    string url;
    int clientFd;
};

// Worker information structure (for server use)
struct WorkerInfo {
    pid_t pid;
    int pipe_read_fd;
    int pipe_write_fd;
    int state;
    DownloadRequest currentRequest;
    
    WorkerInfo() : pid(-1), pipe_read_fd(-1), pipe_write_fd(-1), 
                   state(WORKER_IDLE) {
        currentRequest.clientFd = -1;
    }
};

// Funciones
void workerProcess(int read_fd, int write_fd, int worker_id);
bool writeWorkerMessage(int fd, const WorkerMessage& msg);
bool readWorkerMessage(int fd, WorkerMessage& msg);