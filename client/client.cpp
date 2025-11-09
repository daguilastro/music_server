#include "client_state.hpp"
#include "client_handlers.hpp"
#include "client_display.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>

using namespace std;

// ===== DEFINICIÓN DE VARIABLES GLOBALES =====
int clientSocket = -1;
bool clientRunning = true;
string receivedResponse = "";
ClientState clientState = STATE_IDLE;

string tempUrl = "";
string tempQuery = "";

// ===== ESTRUCTURA DE CALLBACK =====
struct EpollCallbackData {
    int fd;
    void (*handler)(int fd, void* data);
    void* data;
};

// ===== FUNCIONES DE EPOLL =====
int createEpoll() {
    int epollFd = epoll_create1(0);
    if (epollFd < 0) {
        cerr << "[ERROR] No se pudo crear epoll\n";
        return -1;
    }
    return epollFd;
}

int addToEpoll(int epollFd, int fd, void (*handler)(int, void*), void* data) {
    EpollCallbackData* callback = new EpollCallbackData;
    callback->fd = fd;
    callback->handler = handler;
    callback->data = data;

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.ptr = callback;

    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event) != 0) {
        cerr << "[ERROR] No se pudo agregar FD " << fd << " a epoll\n";
        delete callback;
        return -1;
    }

    return 0;
}

bool connectToServer(const string& host, int port) {
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "[ERROR] No se pudo crear socket\n";
        return false;
    }
    
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        cerr << "[ERROR] Dirección IP inválida\n";
        return false;
    }
    
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "[ERROR] No se pudo conectar al servidor: " << strerror(errno) << "\n";
        return false;
    }
    
    int flags = fcntl(clientSocket, F_GETFL, 0);
    fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);
    
    cout << "[CLIENT] Conectado al servidor " << host << ":" << port << endl;
    return true;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cerr << "Uso: " << argv[0] << " <host> <puerto>\n";
        return 1;
    }
    
    string host = argv[1];
    int port = atoi(argv[2]);
    
    if (!connectToServer(host, port)) {
        return 1;
    }
    
    int epollFd = createEpoll();
    if (epollFd < 0) {
        return 1;
    }
    
    addToEpoll(epollFd, clientSocket, handleServerEvent, nullptr);
    addToEpoll(epollFd, STDIN_FILENO, handleStdinEvent, nullptr);
    
    cout << "\n========================================\n";
    cout << "Comandos disponibles:\n";
    cout << "  ADD       - Añadir canción\n";
    cout << "  GET       - Obtener canción por ID\n";
    cout << "  SEARCH    - Buscar canciones\n";
    cout << "  EXIT      - Cerrar servidor\n";
    cout << "  quit      - Desconectar cliente\n";
    cout << "========================================\n\n";
    showPrompt();
    
    struct epoll_event events[10];
    
    while (clientRunning) {
        int nfds = epoll_wait(epollFd, events, 10, -1);
        
        if (nfds < 0) {
            cerr << "[ERROR] Error en epoll_wait\n";
            break;
        }
        
        for (int i = 0; i < nfds; i++) {
            EpollCallbackData* callback = (EpollCallbackData*)events[i].data.ptr;
            callback->handler(callback->fd, callback->data);
        }
    }
    
    close(epollFd);
    close(clientSocket);
    cout << "\n[CLIENT] Desconectado\n";
    
    return 0;
}