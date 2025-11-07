#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

string receiveResponse(int sockfd) {
    char buf[4096];
    memset(buf, 0, sizeof(buf));  // Limpia todo
    ssize_t bytes = recv(sockfd, buf, sizeof(buf) - 1, 0);
    
    if (bytes <= 0) {
        return "";
    }
    
    buf[bytes] = '\0';  // Asegura terminación
    string response(buf);
    
    // Limpia espacios y saltos finales
    response.erase(response.find_last_not_of("\n\r\t ") + 1);
    
    return response;
}

void sendCommand(int sockfd, const string& command) {
    string cmd = command + "\n";
    send(sockfd, cmd.c_str(), cmd.size(), 0);
}

void displaySongInfo(const string& response) {
    // Formato: SONG id|title|artist|filename|url|duration|offset
    
    if (response.find("SONG ") != 0) {
        cout << "❌ Respuesta inválida del servidor" << endl;
        return;
    }
    
    // Quitar "SONG "
    string data = response.substr(5);
    
    // Parsear campos separados por |
    vector<string> fields;
    stringstream ss(data);
    string field;
    
    while (getline(ss, field, '|')) {
        fields.push_back(field);
    }
    
    if (fields.size() != 7) {
        cout << "❌ Formato de respuesta incorrecto" << endl;
        return;
    }
    
    // Mostrar información
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
}

void handleGetCommand(int sockfd) {
    string songId;
    
    cout << "\nID de la canción: ";
    getline(cin, songId);
    
    if (songId.empty()) {
        cout << "[ERROR] ID no puede estar vacío\n";
        return;
    }
    
    cout << "[CLIENT] Consultando canción ID: " << songId << "..." << endl;
    
    sendCommand(sockfd, "GET " + songId);
    
    string response = receiveResponse(sockfd);
    
    if (response.find("ERROR") != string::npos) {
        if (response.find("song_not_found") != string::npos) {
            cout << "❌ Canción no encontrada con ID: " << songId << endl;
        } else if (response.find("invalid_id") != string::npos) {
            cout << "❌ ID inválido" << endl;
        } else {
            cout << "❌ Error: " << response;
        }
        return;
    }
    
    if (response.find("SONG ") == 0) {
        displaySongInfo(response);
    } else {
        cout << "[SERVER] " << response;
    }
}

void handleAddCommand(int sockfd) {
    string url, title, artist, filename, duration;
    
    cout << "\nURL de la canción: ";
    getline(cin, url);
    
    if (url.empty()) {
        cout << "[ERROR] URL no puede estar vacía\n";
        return;
    }
    
    cout << "[CLIENT] Verificando URL con servidor..." << endl;
    sendCommand(sockfd, "ADD " + url);
    
    string response = receiveResponse(sockfd);
    cout << "[SERVER] " << response;
    
    if (response.find("DUPLICATE") != string::npos) {
        cout << "❌ Esta canción ya existe en la base de datos\n";
        return;
    }
    
    if (response.find("OK_ADD") == string::npos) {
        cout << "❌ Error del servidor\n";
        return;
    }
    
    cout << "✅ URL válida, ingrese los campos:\n" << endl;
    
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
    cout << "  GET      - Obtener canción por ID\n";
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

        // ===== COMANDO ADD =====
        if (command == "ADD" || command == "add") {
            handleAddCommand(clientSocket);
            continue;
        }
        
        // ===== COMANDO GET =====
        if (command == "GET" || command == "get") {
            handleGetCommand(clientSocket);
            continue;
        }
        
        // Detectar si es ADD con URL
        if (command.substr(0, 4) == "ADD " || command.substr(0, 4) == "add ") {
            string url = command.substr(4);
            url.erase(0, url.find_first_not_of(" \t"));
            url.erase(url.find_last_not_of(" \t") + 1);
            
            if (url.empty()) {
                cout << "[ERROR] Debe proporcionar una URL\n";
                continue;
            }
            
            cout << "[CLIENT] Verificando URL con servidor..." << endl;
            sendCommand(clientSocket, "ADD " + url);
            
            string response = receiveResponse(clientSocket);
            cout << "[SERVER] " << response;
            
            if (response.find("DUPLICATE") != string::npos) {
                cout << "❌ Esta canción ya existe en la base de datos\n";
                continue;
            }
            
            if (response.find("OK_ADD") == string::npos) {
                cout << "❌ Error del servidor\n";
                continue;
            }
            
            cout << "✅ URL válida, ingrese los campos:\n" << endl;
            
            string title, artist, filename, duration;
            
            cout << "Título: ";
            getline(cin, title);
            
            if (title.empty()) {
                cout << "[ERROR] Título no puede estar vacío\n";
                continue;
            }
            
            cout << "Artista (opcional, Enter para omitir): ";
            getline(cin, artist);
            
            cout << "Nombre de archivo: ";
            getline(cin, filename);
            
            if (filename.empty()) {
                cout << "[ERROR] Nombre de archivo no puede estar vacío\n";
                continue;
            }
            
            cout << "Duración (segundos): ";
            getline(cin, duration);
            
            if (duration.empty()) {
                duration = "0";
            }
            
            stringstream indexCmd;
            indexCmd << "INDEX " << url << "|" << title << "|" << artist << "|" 
                     << filename << "|" << duration;
            
            cout << "\n[CLIENT] Enviando datos al servidor..." << endl;
            sendCommand(clientSocket, indexCmd.str());
            
            response = receiveResponse(clientSocket);
            
            if (response.find("INDEXED") != string::npos) {
                cout << "✅ Canción añadida exitosamente!" << endl;
                cout << "[SERVER] " << response << endl;
            } else if (response.find("ERROR") != string::npos) {
                cout << "❌ Error al añadir canción: " << response;
            } else {
                cout << "[SERVER] " << response;
            }
            
            continue;
        }
        
        // Detectar si es GET con ID
        if (command.substr(0, 4) == "GET " || command.substr(0, 4) == "get ") {
            string songId = command.substr(4);
            songId.erase(0, songId.find_first_not_of(" \t"));
            songId.erase(songId.find_last_not_of(" \t\r\n") + 1);
            
            if (songId.empty()) {
                cout << "[ERROR] Debe proporcionar un ID\n";
                continue;
            }
            
            cout << "[CLIENT] Consultando canción ID: " << songId << "..." << endl;
            
            sendCommand(clientSocket, "GET " + songId);
            
            string response = receiveResponse(clientSocket);
            
            if (response.find("ERROR") != string::npos) {
                if (response.find("song_not_found") != string::npos) {
                    cout << "❌ Canción no encontrada con ID: " << songId << endl;
                } else if (response.find("invalid_id") != string::npos) {
                    cout << "❌ ID inválido" << endl;
                } else {
                    cout << "❌ Error: " << response;
                }
                continue;
            }
            
            if (response.find("SONG ") == 0) {
                displaySongInfo(response);
            } else {
                cout << "[SERVER] " << response;
            }
            
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