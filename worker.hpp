#pragma once

#include <sys/types.h>
#include "socket_utilities.hpp"
#include <string>
#include <cstdint>

using namespace std;

// Worker states
#define WORKER_IDLE 0
#define WORKER_BUSY 1

// Message types for pipe communication
#define MSG_REQUEST  1    // Server -> Worker: new job
#define MSG_FINISHED 2    // Worker -> Server: job finished (success or failure)
#define MSG_SHUTDOWN 3    // Server -> Worker: shutdown worker

// Message structure for pipe communication
struct WorkerMessage {
    uint8_t type;
    uint32_t request_id;
    uint32_t data_length;
    char data[512];  // URL or result message
    
    WorkerMessage() : type(MSG_FINISHED), request_id(0), data_length(0) {
        data[0] = '\0';
    }
};

// Worker information structure (for server use)
struct WorkerInfo {
    pid_t pid;
    int pipe_read_fd;   // Server reads from this
    int pipe_write_fd;  // Server writes to this
    int state;          // WORKER_IDLE or WORKER_BUSY
    uint32_t current_request_id;
    
    WorkerInfo() : pid(-1), pipe_read_fd(-1), pipe_write_fd(-1), 
                   state(WORKER_IDLE), current_request_id(0) {}
};

void submitDownloadRequest(const string& url);
void workerProcess(int read_fd, int write_fd, int worker_id);

// Utility functions for message I/O
bool writeWorkerMessage(int fd, const WorkerMessage& msg);
bool readWorkerMessage(int fd, WorkerMessage& msg);