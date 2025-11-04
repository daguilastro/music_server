#pragma once

#include <sys/types.h>
#include <string>
#include <cstdint>
#include <unistd.h>
#include <iostream>

using namespace std;

// Worker states
#define WORKER_IDLE 0
#define WORKER_BUSY 1

// Message types for pipe communication
#define MSG_REQUEST  1
#define MSG_FINISHED 2
#define MSG_SHUTDOWN 3

// Message structure for pipe communication
struct WorkerMessage {
    uint8_t type;
    uint32_t request_id;
    uint32_t data_length;
    char data[512];
    
    WorkerMessage() : type(MSG_FINISHED), request_id(0), data_length(0) {
        data[0] = '\0';
    }
};

// Worker information structure (for server use)
struct WorkerInfo {
    pid_t pid;
    int pipe_read_fd;
    int pipe_write_fd;
    int state;
    uint32_t current_request_id;
    
    WorkerInfo() : pid(-1), pipe_read_fd(-1), pipe_write_fd(-1), 
                   state(WORKER_IDLE), current_request_id(0) {}
};

// Funciones
void workerProcess(int read_fd, int write_fd, int worker_id);
bool writeWorkerMessage(int fd, const WorkerMessage& msg);
bool readWorkerMessage(int fd, WorkerMessage& msg);