#include "command_handler.hpp"
#include <iostream>
#include <string>

using namespace std;

map<string, void(*)(int,const string&)> commandHandlers;

void initializeCommandHandlers() {
    commandHandlers["ADD"] = handleAddCommand;
    commandHandlers["EXIT"] = handleExitCommand;
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

void handleAddCommand(int clientFd, const string& args) {
    cout << "[ADD] Cliente " << clientFd << " añadió: " << args << endl;
    // TODO: submitDownload(args);
}

void handleExitCommand(int clientFd, const string& args) {
    cout << "[EXIT] Cliente " << clientFd << " solicitó cerrar el servidor" << endl;
    extern bool serverRunning;
    serverRunning = false;
}