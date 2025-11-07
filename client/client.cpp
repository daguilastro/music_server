#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

string receiveResponse(int sockfd) {
    char buf[512];
    memset(buf, 0, sizeof(buf));
    ssize_t bytes = recv(sockfd, buf, sizeof(buf) - 1, 0);
    
    if (bytes <= 0) {
        return "";
    }
    
    return string(buf);
}

void sendCommand(int sockfd, const string& command) {
    string cmd = command + "\n";
    send(sockfd, cmd.c_str(), cmd.size(), 0);
}

void handleAddCommand(int sockfd) {
    string url, title, artist, filename, duration;
    
    // ===== PASO 1: Pedir URL y verificar con servidor =====
    cout << "\nURL de la canción: ";
    getline(cin, url);
    
    if (url.empty()) {
        cout << "[ERROR] URL no puede estar vacía\n";
        return;
    }
    
    cout << "[CLIENT] Verificando URL con servidor..." << endl;
    sendCommand(sockfd, "ADD " + url);
    
    string response = receiveResponse(sockfd);
    
    if (response.find("DUPLICATE") != string::npos) {
        cout << "❌ Esta canción ya existe en la base de datos\n";
        return;
    }
    
    if (response.find("OK_ADD") == string::npos) {
        cout << "❌ Error del servidor: " << response;
        return;
    }
    
    cout << "✅ URL válida, ingrese los campos:\n" << endl;
    
    // ===== PASO 2: Pedir campos localmente (en el cliente) =====
    cout << "Título: ";
    getline(cin, title);
    
    if (title.empty()) {
        cout << "[ERROR] Título no puede estar vacío\n";
        return;
    }
    
    cout << "Artista (opcional, Enter para omitir): ";
    getline(cin, artist);
    
    cout << "Nombre de archivo: ";
    getline(cin, filename);
    
    if (filename.empty()) {
        cout << "[ERROR] Nombre de archivo no puede estar vacío\n";
        return;
    }
    
    cout << "Duración (segundos): ";
    getline(cin, duration);
    
    if (duration.empty()) {
        duration = "0";
    }
    
    // ===== PASO 3: Construir comando INDEX y enviar =====
    stringstream indexCmd;
    indexCmd << "INDEX " << url << "|" << title << "|" << artist << "|" 
             << filename << "|" << duration;
    
    cout << "\n[CLIENT] Enviando datos al servidor..." << endl;
    sendCommand(sockfd, indexCmd.str());
    
    response = receiveResponse(sockfd);
    
    if (response.find("INDEXED") != string::npos) {
        cout << "✅ Canción añadida exitosamente!" << endl;
        cout << "[SERVER] " << response;
    } else if (response.find("ERROR") != string::npos) {
        cout << "❌ Error al añadir canción: " << response;
    } else {
        cout << "[SERVER] " << response;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cerr << "Uso: " << argv[0] << " <IP_SERVIDOR> <PUERTO>\n";
        cerr << "Ejemplo: " << argv[0] << " 192.168.1.120 46505\n";
        return 1;
    }

    string serverIP = argv[1];
    int serverPort = atoi(argv[2]);

    cout << "[CLIENT] Conectando a " << serverIP << ":" << serverPort << "...\n";

    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "[ERROR] No se pudo crear socket: " << strerror(errno) << "\n";
        return 1;
    }

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "[ERROR] Dirección IP inválida: " << serverIP << "\n";
        close(clientSocket);
        return 1;
    }

    if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        cerr << "[ERROR] No se pudo conectar al servidor: " << strerror(errno) << "\n";
        close(clientSocket);
        return 1;
    }

    cout << "[SUCCESS] Conectado al servidor!\n";
    cout << "\n========================================\n";
    cout << "Comandos disponibles:\n";
    cout << "  ADD      - Añadir canción\n";
    cout << "  EXIT     - Cerrar servidor\n";
    cout << "  quit     - Desconectar cliente\n";
    cout << "========================================\n\n";

    string command;

    while (true) {
        cout << "> ";
        getline(cin, command);

        if (command == "quit" || command == "q") {
            cout << "[CLIENT] Desconectando...\n";
            break;
        }

        if (command.empty()) {
            continue;
        }

        // Comando especial ADD (interactivo)
        if (command == "ADD" || command == "add") {
            handleAddCommand(clientSocket);
            continue;
        }

        // Otros comandos simples
        sendCommand(clientSocket, command);
        string response = receiveResponse(clientSocket);
        
        if (!response.empty()) {
            cout << "[SERVER] " << response;
        }

        if (command == "EXIT") {
            cout << "[CLIENT] Servidor cerrándose...\n";
            sleep(1);
            break;
        }
    }

    close(clientSocket);
    cout << "[CLIENT] Desconectado\n";

    return 0;
}