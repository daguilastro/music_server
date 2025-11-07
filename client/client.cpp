#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>

using namespace std;

// ===== ESTRUCTURA DE CALLBACK =====
struct EpollCallbackData {
    int fd;
    void (*handler)(int fd, void* data);
    void* data;
};

// ===== ESTADOS DEL CLIENTE =====
enum ClientState {
    STATE_IDLE,                 // Esperando comando
    STATE_ADD_WAITING_URL,      // Pidiendo URL del usuario
    STATE_ADD_WAITING_SERVER,   // Esperando respuesta OK_ADD del servidor
    STATE_ADD_WAITING_TITLE,    // Pidiendo título
    STATE_ADD_WAITING_ARTIST,   // Pidiendo artista
    STATE_ADD_WAITING_FILENAME, // Pidiendo filename
    STATE_ADD_WAITING_DURATION, // Pidiendo duración
    STATE_ADD_SUBMITTING,       // Esperando respuesta INDEX del servidor
    STATE_GET_WAITING_ID,       // Pidiendo ID para GET
    STATE_GET_WAITING_RESPONSE  // Esperando respuesta GET del servidor
};

// ===== VARIABLES GLOBALES =====
int clientSocket = -1;
bool clientRunning = true;
string receivedResponse = "";
ClientState clientState = STATE_IDLE;

// Datos temporales para ADD
string tempUrl = "";
string tempTitle = "";
string tempArtist = "";
string tempFilename = "";
string tempDuration = "";

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

// ===== HELPER PARA MOSTRAR PROMPT =====
void showPrompt() {
    cout << "> ";
    cout.flush();
}

// ===== HANDLER DEL SERVIDOR =====
void handleServerEvent(int fd, void* data) {
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    
    ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes <= 0) {
        cerr << "\n[ERROR] Desconectado del servidor\n";
        clientRunning = false;
        return;
    }
    
    buffer[bytes] = '\0';
    receivedResponse += string(buffer);
    
    // Procesar todas las líneas completas
    while (true) {
        size_t pos = receivedResponse.find('\n');
        if (pos == string::npos) break;
        
        string line = receivedResponse.substr(0, pos);
        receivedResponse = receivedResponse.substr(pos + 1);
        
        // Limpiar espacios finales
        line.erase(line.find_last_not_of("\r\n\t ") + 1);
        
        if (line.empty()) continue;
        
        // ===== MENSAJES ASINCRONICOS =====
        if (line.find("Descarga completada") != string::npos) {
            cout << "\n[SERVER] " << line << endl;
            if (clientState == STATE_IDLE) {
                showPrompt();
            }
            continue;
        }
        
        // ===== ESTADO: Esperando respuesta OK_ADD del servidor =====
        if (clientState == STATE_ADD_WAITING_SERVER) {
            if (line.find("OK_ADD") != string::npos) {
                cout << "\n✅ URL válida, ingrese los campos:\n" << endl;
                
                cout << "Título: ";
                cout.flush();
                clientState = STATE_ADD_WAITING_TITLE;
                
            } else if (line.find("DUPLICATE") != string::npos) {
                cout << "\n❌ Esta canción ya existe en la base de datos\n";
                clientState = STATE_IDLE;
                showPrompt();
                
            } else if (line.find("ERROR") != string::npos) {
                cout << "\n❌ Error del servidor: " << line << endl;
                clientState = STATE_IDLE;
                showPrompt();
            }
            continue;
        }
        
        // ===== ESTADO: Esperando respuesta INDEX (SUBMIT) =====
        if (clientState == STATE_ADD_SUBMITTING) {
            if (line.find("INDEXED") != string::npos) {
                cout << "\n✅ Canción añadida exitosamente!" << endl;
                cout << "[SERVER] " << line << endl;
                clientState = STATE_IDLE;
                showPrompt();
                
            } else if (line.find("ERROR") != string::npos) {
                cout << "\n❌ Error al añadir canción: " << line << endl;
                clientState = STATE_IDLE;
                showPrompt();
            }
            continue;
        }
        
        // ===== ESTADO: Esperando respuesta GET =====
        if (clientState == STATE_GET_WAITING_RESPONSE) {
            if (line.find("SONG ") == 0) {
                // Parsear y mostrar canción
                string data = line.substr(5);
                vector<string> fields;
                stringstream ss(data);
                string field;
                
                while (getline(ss, field, '|')) {
                    fields.push_back(field);
                }
                
                if (fields.size() == 7) {
                    cout << "\n╔════════════════════════════════════════════════════════════╗" << endl;
                    cout << "║                    INFORMACIÓN DE CANCIÓN                  ║" << endl;
                    cout << "╠════════════════════════════════════════════════════════════╣" << endl;
                    cout << "║ ID:         " << fields[0] << endl;
                    cout << "║ Título:     " << fields[1] << endl;
                    cout << "║ Artista:    " << fields[2] << endl;
                    cout << "║ Archivo:    " << fields[3] << endl;
                    cout << "║ URL:        " << fields[4] << endl;
                    cout << "║ Duración:   " << fields[5] << " segundos" << endl;
                    cout << "║ Offset:     " << fields[6] << " bytes" << endl;
                    cout << "╚════════════════════════════════════════════════════════════╝" << endl;
                    clientState = STATE_IDLE;
                    showPrompt();
                }
            } else if (line.find("ERROR") != string::npos) {
                cout << "\n❌ " << line << endl;
                clientState = STATE_IDLE;
                showPrompt();
            }
            continue;
        }
        
        // ===== ESTADO: IDLE (otros comandos) =====
        if (clientState == STATE_IDLE) {
            if (line.find("ERROR") != string::npos) {
                cout << "\n❌ Error: " << line << endl;
            } else {
                cout << "\n[SERVER] " << line << endl;
            }
            showPrompt();
        }
    }
}

// ===== HANDLER DEL STDIN =====
void handleStdinEvent(int fd, void* data) {
    string input;
    if (!getline(cin, input)) {
        clientRunning = false;
        return;
    }
    
    // Ignorar comandos vacíos en IDLE
    if (input.empty() && clientState == STATE_IDLE) {
        showPrompt();
        return;
    }
    
    // ===== PARSEAR COMANDO Y PARÁMETROS =====
    string command = input;
    string param = "";
    
    size_t spacePos = input.find(' ');
    if (spacePos != string::npos) {
        command = input.substr(0, spacePos);
        param = input.substr(spacePos + 1);
        
        // Limpiar espacios iniciales del parámetro
        param.erase(0, param.find_first_not_of(" \t"));
    }
    
    // ===== COMANDO: ADD =====
    if ((command == "ADD" || command == "add") && clientState == STATE_IDLE) {
        cout << "\nURL de la canción: ";
        cout.flush();
        
        tempUrl = "";
        tempTitle = "";
        tempArtist = "";
        tempFilename = "";
        tempDuration = "";
        
        clientState = STATE_ADD_WAITING_URL;
        return;
    }
    
    // ===== ESTADO: Esperando URL =====
    if (clientState == STATE_ADD_WAITING_URL) {
        tempUrl = input;
        
        if (tempUrl.empty()) {
            cout << "[ERROR] URL no puede estar vacía\n";
            cout << "URL de la canción: ";
            cout.flush();
            return;
        }
        
        cout << "[CLIENT] Verificando URL con servidor..." << endl;
        string cmd = "ADD " + tempUrl + "\n";
        send(clientSocket, cmd.c_str(), cmd.size(), 0);
        
        clientState = STATE_ADD_WAITING_SERVER;
        return;
    }
    
    // ===== ESTADO: Esperando Título =====
    if (clientState == STATE_ADD_WAITING_TITLE) {
        tempTitle = input;
        
        if (tempTitle.empty()) {
            cout << "[ERROR] Título no puede estar vacío\n";
            cout << "Título: ";
            cout.flush();
            return;
        }
        
        cout << "Artista (opcional, Enter para omitir): ";
        cout.flush();
        clientState = STATE_ADD_WAITING_ARTIST;
        return;
    }
    
    // ===== ESTADO: Esperando Artista =====
    if (clientState == STATE_ADD_WAITING_ARTIST) {
        tempArtist = input;  // Puede estar vacío
        
        cout << "Nombre de archivo: ";
        cout.flush();
        clientState = STATE_ADD_WAITING_FILENAME;
        return;
    }
    
    // ===== ESTADO: Esperando Filename =====
    if (clientState == STATE_ADD_WAITING_FILENAME) {
        tempFilename = input;
        
        if (tempFilename.empty()) {
            cout << "[ERROR] Nombre de archivo no puede estar vacío\n";
            cout << "Nombre de archivo: ";
            cout.flush();
            return;
        }
        
        cout << "Duración (segundos): ";
        cout.flush();
        clientState = STATE_ADD_WAITING_DURATION;
        return;
    }
    
    // ===== ESTADO: Esperando Duración =====
    if (clientState == STATE_ADD_WAITING_DURATION) {
        tempDuration = input;
        
        if (tempDuration.empty()) {
            tempDuration = "0";
        }
        
        // ===== ENVIAR INDEX AL SERVIDOR =====
        stringstream indexCmd;
        indexCmd << "INDEX " << tempUrl << "|" << tempTitle << "|" << tempArtist << "|" 
                 << tempFilename << "|" << tempDuration;
        
        cout << "\n[CLIENT] Enviando datos al servidor..." << endl;
        string cmd = indexCmd.str() + "\n";
        send(clientSocket, cmd.c_str(), cmd.size(), 0);
        
        clientState = STATE_ADD_SUBMITTING;
        return;
    }
    
    // ===== COMANDO: GET con parámetro directo =====
    if ((command == "GET" || command == "get") && !param.empty() && clientState == STATE_IDLE) {
        string songId = param;
        
        cout << "[CLIENT] Consultando canción ID: " << songId << "..." << endl;
        string cmd = "GET " + songId + "\n";
        send(clientSocket, cmd.c_str(), cmd.size(), 0);
        
        clientState = STATE_GET_WAITING_RESPONSE;
        return;
    }
    
    // ===== COMANDO: GET sin parámetro =====
    if ((command == "GET" || command == "get") && param.empty() && clientState == STATE_IDLE) {
        cout << "\nID de la canción: ";
        cout.flush();
        clientState = STATE_GET_WAITING_ID;
        return;
    }
    
    // ===== ESTADO: Esperando ID para GET =====
    if (clientState == STATE_GET_WAITING_ID) {
        string songId = input;
        
        if (songId.empty()) {
            cout << "[ERROR] ID no puede estar vacío\n";
            cout << "ID de la canción: ";
            cout.flush();
            return;
        }
        
        cout << "[CLIENT] Consultando canción ID: " << songId << "..." << endl;
        string cmd = "GET " + songId + "\n";
        send(clientSocket, cmd.c_str(), cmd.size(), 0);
        
        clientState = STATE_GET_WAITING_RESPONSE;
        return;
    }
    
    // ===== COMANDO: QUIT =====
    if ((command == "quit" || command == "QUIT") && clientState == STATE_IDLE) {
        clientRunning = false;
        return;
    }
    
    // ===== OTROS COMANDOS (solo en IDLE) =====
    if (clientState == STATE_IDLE) {
        string cmd = input + "\n";
        send(clientSocket, cmd.c_str(), cmd.size(), 0);
    }
}

// ===== FUNCIONES AUXILIARES =====
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
        cerr << "[ERROR] No se pudo conectar al servidor " << strerror(errno) << "\n";
        return false;
    }
    
    // Configurar como no bloqueante
    int flags = fcntl(clientSocket, F_GETFL, 0);
    fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);
    
    cout << "[CLIENT] Conectado al servidor " << host << ":" << port << endl;
    return true;
}

// ===== MAIN =====
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
    
    // Crear epoll
    int epollFd = createEpoll();
    if (epollFd < 0) {
        return 1;
    }
    
    // Añadir servidor y stdin a epoll
    addToEpoll(epollFd, clientSocket, handleServerEvent, nullptr);
    addToEpoll(epollFd, STDIN_FILENO, handleStdinEvent, nullptr);
    
    cout << "\n========================================\n";
    cout << "Comandos disponibles:\n";
    cout << "  ADD      - Añadir canción\n";
    cout << "  GET      - Obtener canción por ID\n";
    cout << "  EXIT     - Cerrar servidor\n";
    cout << "  quit     - Desconectar cliente\n";
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