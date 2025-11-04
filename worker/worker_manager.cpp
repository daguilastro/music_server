#include "worker_manager.hpp"
#include "worker.hpp"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <cstring>

using namespace std;

// Variables globales
vector<WorkerInfo> workers;
queue<string> downloadQueue;
uint32_t nextRequestId = 1;

bool initializeWorkers(int epollFd, int numWorkers) {
    cout << "[Server] Inicializando " << numWorkers << " workers..." << endl;

    for (int i = 0; i < numWorkers; i++) {
        int pipeToWorker[2];
        int pipeFromWorker[2];

        if (pipe(pipeToWorker) == -1 || pipe(pipeFromWorker) == -1) {
            perror("pipe");
            return false;
        }

        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            return false;
        }

        if (pid == 0) {
            // ===== PROCESO HIJO (WORKER) =====
            close(pipeToWorker[1]);
            close(pipeFromWorker[0]);
            workerProcess(pipeToWorker[0], pipeFromWorker[1], i);
            exit(0);
        }

        // ===== PROCESO PADRE (SERVIDOR) =====
        close(pipeToWorker[0]);
        close(pipeFromWorker[1]);

        // Configurar como no bloqueante
        int flags = fcntl(pipeFromWorker[0], F_GETFL, 0);
        fcntl(pipeFromWorker[0], F_SETFL, flags | O_NONBLOCK);

        // Crear WorkerInfo
        WorkerInfo worker;
        worker.pid = pid;
        worker.pipe_read_fd = pipeFromWorker[0];
        worker.pipe_write_fd = pipeToWorker[1];
        worker.state = WORKER_IDLE;
        worker.current_request_id = 0;
        workers.push_back(worker);

        // ===== AÑADIR WORKER A EPOLL CON CALLBACK =====
        // Necesitamos declarar addToEpoll (está en epoll_handler)
        extern int addToEpoll(int epollFd, int fd, void (*handler)(int, void*), void* data);
        
        if (addToEpoll(epollFd, pipeFromWorker[0], handleWorkerEvent, &workers.back()) < 0) {
            cerr << "[ERROR] No se pudo añadir worker a epoll\n";
            return false;
        }

        cout << "[Server] Worker " << i << " creado (PID: " << pid << ")" << endl;
    }

    cout << "[Server] Todos los workers inicializados" << endl;
    return true;
}

void handleWorkerEvent(int fd, void* data) {
    WorkerInfo* worker = static_cast<WorkerInfo*>(data);

    WorkerMessage response;
    if (!readWorkerMessage(fd, response)) {
        cerr << "[Server] Error leyendo de worker " << worker->pid << endl;
        return;
    }

    // Procesar mensaje FINISHED
    if (response.type == MSG_FINISHED) {
        cout << "[Server] Worker " << worker->pid << " terminó request "
             << response.request_id << ": " << response.data << endl;

        // Marcar worker como IDLE
        worker->state = WORKER_IDLE;
        worker->current_request_id = 0;

        // Intentar asignar más trabajo
        assignPendingDownloads();
    } else {
        cerr << "[Server] Tipo de mensaje inesperado de worker" << endl;
    }
}

void submitDownload(const string& url) {
    cout << "[Server] Añadiendo a cola de descargas: " << url << endl;
    downloadQueue.push(url);
    assignPendingDownloads();
}

void assignPendingDownloads() {
    // Mientras haya trabajo pendiente y workers libres
    while (!downloadQueue.empty()) {
        // Buscar un worker IDLE
        WorkerInfo* idleWorker = nullptr;
        for (auto& worker : workers) {
            if (worker.state == WORKER_IDLE) {
                idleWorker = &worker;
                break;
            }
        }

        // Si no hay workers libres, salir
        if (idleWorker == nullptr) {
            cout << "[Server] No hay workers libres, " << downloadQueue.size() 
                 << " descargas en cola" << endl;
            break;
        }

        // Obtener la siguiente URL de la cola
        string url = downloadQueue.front();
        downloadQueue.pop();

        // Crear mensaje para el worker
        WorkerMessage request;
        request.type = MSG_REQUEST;
        request.request_id = nextRequestId++;
        request.data_length = url.length();
        memcpy(request.data, url.c_str(), request.data_length);
        request.data[request.data_length] = '\0';

        // Enviar al worker
        if (writeWorkerMessage(idleWorker->pipe_write_fd, request)) {
            idleWorker->state = WORKER_BUSY;
            idleWorker->current_request_id = request.request_id;
            cout << "[Server] Asignado request " << request.request_id 
                 << " al worker " << idleWorker->pid << ": " << url << endl;
        } else {
            cerr << "[ERROR] No se pudo enviar request al worker " 
                 << idleWorker->pid << endl;
            // Volver a poner en la cola
            downloadQueue.push(url);
            break;
        }
    }
}

void shutdownWorkers() {
    cout << "[Server] Cerrando workers..." << endl;
    
    for (auto& worker : workers) {
        WorkerMessage shutdown;
        shutdown.type = MSG_SHUTDOWN;
        writeWorkerMessage(worker.pipe_write_fd, shutdown);
        close(worker.pipe_write_fd);
        close(worker.pipe_read_fd);
        cout << "[Server] Worker " << worker.pid << " cerrado" << endl;
    }
    
    workers.clear();
}