#include "command_handler.hpp"
#include <cstdlib>
#include <iostream>
#include <regex>
#include <string>
#include <sys/socket.h>
#include <vector>

using namespace std;

map<string, void(*)(int,const string&)> commandHandlers;

void initializeCommandHandlers() {
    commandHandlers["ADD"] = handleAddCommand;
    commandHandlers["EXIT"] = handleExitCommand;
    commandHandlers["INDEX"] = handleIndexCommand;
    cout << "[Server] Command handlers initialized" << endl;
}

void handleCommand(int clientFd, const string& request) {
    cout << "[REQUEST] Cliente " << clientFd << ": " << request << endl;

    // Separar el comando de sus argumentos
    string command;
    string arguments;

    size_t space_pos = request.find(' ');
    if (space_pos != string::npos) {
        command = request.substr(0, space_pos);      // "ADD"
        arguments = request.substr(space_pos + 1);   // "https://..."
    } else {
        // No hay argumentos: "EXIT"
        command = request;
        arguments = "";
    }
    
    // Limpiar espacios y \r\n del comando
    command.erase(command.find_last_not_of(" \t\r\n") + 1);
    command.erase(0, command.find_first_not_of(" \t\r\n"));
    
    if (commandHandlers.count(command)) {
        commandHandlers[command](clientFd, arguments);
    } else {
        cerr << "[ERROR] Comando desconocido: " << command << endl;
    }
}

void handleAddCommand(int clientFd, const string& url) {
    if (url.empty()) {
        string error = "ERROR missing_url\n";
        send(clientFd, error.c_str(), error.size(), 0);
        return;
    }
    
    cout << "[ADD] Cliente " << clientFd << " verificando URL: " << url << endl;
    
    // Verificar si URL ya existe
    if (isDuplicateURL(globalDB, url.c_str())) {
        string response = "DUPLICATE\n";
        send(clientFd, response.c_str(), response.size(), 0);
        cout << "[ADD] URL duplicada rechazada: " << url << endl;
        return;
    }
    
    // URL válida, dar luz verde al cliente
    string response = "OK_ADD\n";
    send(clientFd, response.c_str(), response.size(), 0);
    cout << "[ADD] URL aceptada, esperando comando INDEX..." << endl;
}

void handleIndexCommand(int clientFd, const string& args) {
    // Formato esperado: "url|title|artist|filename|duration"
    
    if (args.empty()) {
        string error = "ERROR missing_data\n";
        send(clientFd, error.c_str(), error.size(), 0);
        return;
    }
    
    // Parsear campos separados por |
    vector<string> fields;
    stringstream ss(args);
    string field;
    
    while (getline(ss, field, '|')) {
        fields.push_back(field);
    }
    
    if (fields.size() != 5) {
        string error = "ERROR invalid_format\n";
        send(clientFd, error.c_str(), error.size(), 0);
        cerr << "[INDEX] Formato inválido, se esperaban 5 campos, recibidos: " << fields.size() << endl;
        return;
    }
    
    string url = fields[0];
    string title = fields[1];
    string artist = fields[2];
    string filename = fields[3];
    uint32_t duration = atoi(fields[4].c_str());
    
    cout << "[INDEX] Procesando:" << endl;
    cout << "  URL: " << url << endl;
    cout << "  Título: " << title << endl;
    cout << "  Artista: " << artist << endl;
    cout << "  Archivo: " << filename << endl;
    cout << "  Duración: " << duration << " seg" << endl;
    
    // Verificar NUEVAMENTE que no exista (por seguridad)
    if (isDuplicateURL(globalDB, url.c_str())) {
        string response = "ERROR duplicate_url\n";
        send(clientFd, response.c_str(), response.size(), 0);
        cout << "[INDEX] URL duplicada (doble verificación)" << endl;
        return;
    }
    
    // Validar campos obligatorios
    if (title.empty()) {
        string error = "ERROR missing_title\n";
        send(clientFd, error.c_str(), error.size(), 0);
        return;
    }
    
    if (filename.empty()) {
        string error = "ERROR missing_filename\n";
        send(clientFd, error.c_str(), error.size(), 0);
        return;
    }
    
    // Añadir canción a la database
    int songId = addSong(globalDB, 
                        title.c_str(),
                        filename.c_str(),
                        url.c_str(),
                        artist.empty() ? "Unknown" : artist.c_str(),
                        duration);
    
    if (songId < 0) {
        string error = "ERROR could_not_add_song\n";
        send(clientFd, error.c_str(), error.size(), 0);
        return;
    }
    
    // Guardar database
    cout << "[INDEX] Guardando base de datos..." << endl;
    if (!saveDatabase(globalDB, "music_database.bin")) {
        cerr << "[WARNING] No se pudo guardar la base de datos" << endl;
    }
    
    // Enviar confirmación
    stringstream response;
    response << "INDEXED id=" << songId << "\n";
    string resp = response.str();
    send(clientFd, resp.c_str(), resp.size(), 0);
    
    cout << "[INDEX] Canción añadida e indexada: [" << songId << "] " 
         << title << " - " << artist << endl;
    
    // TODO: Ahora iniciar descarga
    submitDownload(url, clientFd);
}

void handleExitCommand(int clientFd, const string& args) {
    cout << "[EXIT] Cliente " << clientFd << " solicitó cerrar el servidor" << endl;
    extern bool serverRunning;
    serverRunning = false;
}