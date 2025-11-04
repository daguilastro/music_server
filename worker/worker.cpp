#include "worker.hpp"
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <iostream>

using namespace std;

bool writeWorkerMessage(int fd, const WorkerMessage& msg) {
    size_t total = 0;
    size_t msg_size = sizeof(WorkerMessage);
    char* buf = (char*)&msg;
    
    while (total < msg_size) {
        ssize_t written = write(fd, buf + total, msg_size - total);
        if (written < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        total += written;
    }
    return true;
}

bool readWorkerMessage(int fd, WorkerMessage& msg) {
    size_t total = 0;
    size_t messageSize = sizeof(WorkerMessage);
    char* buf = (char*)&msg;
    
    while (total < messageSize) {
        ssize_t bytes_read = read(fd, buf + total, messageSize - total);
        if (bytes_read < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (bytes_read == 0) {
            return false;
        }
        total += bytes_read;
    }
    return true;
}

void workerProcess(int read_fd, int write_fd, int worker_id) {
    cout << "[Worker " << worker_id << "] Iniciado con PID " << getpid() << endl;
    
    while (true) {
        WorkerMessage request;
        
        if (!readWorkerMessage(read_fd, request)) {
            cerr << "[Worker " << worker_id << "] Error leyendo, saliendo" << endl;
            break;
        }
        
        if (request.type == MSG_SHUTDOWN) {
            cout << "[Worker " << worker_id << "] SHUTDOWN recibido" << endl;
            break;
        }
        
        if (request.type != MSG_REQUEST) {
            continue;
        }
        
        string url(request.data, request.data_length);
        cout << "[Worker " << worker_id << "] Procesando: " << url << endl;
        
        // Simular trabajo
        sleep(2);
        
        WorkerMessage response;
        response.type = MSG_FINISHED;
        response.request_id = request.request_id;
        string result = "Download completado";
        response.data_length = result.length();
        memcpy(response.data, result.c_str(), response.data_length);
        response.data[response.data_length] = '\0';
        
        if (!writeWorkerMessage(write_fd, response)) {
            cerr << "[Worker " << worker_id << "] Error enviando respuesta" << endl;
            break;
        }
    }
    
    close(read_fd);
    close(write_fd);
    exit(0);
}