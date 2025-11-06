#include "command_handler.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

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
    cout << "[ADD] Cliente " << clientFd << " añadió: " << "args" << endl;
    vector<string> songs;
    songs.push_back("https://youtu.be/rEH3QdOogtY?si=vL-BZIPCNkzmde-3");
    songs.push_back("https://youtu.be/bmTedzd-Wj8?si=8k4sUvi8Fp1damxA");
    songs.push_back("https://youtu.be/gT2wY0DjYGo?si=B3mtO0tzi0gAtuPy");
    songs.push_back("https://youtu.be/AoxwwLEqHyc?si=7jUO4xzJwKEZw1rv");
    songs.push_back("https://youtu.be/dxE8Vlxs_rY?si=Y6TjRuGjyakpYctr");
    songs.push_back("https://youtu.be/mtbh8rZeMZY?si=4BqKz1b2RXwKwQ8v");
    songs.push_back("https://youtu.be/-qGbUNQqVNc?si=rRA3DGFh_GgJImGC");
    songs.push_back("https://youtu.be/n6B5gQXlB-0?si=4aiOcwrBdpjtXz1W");
    songs.push_back("https://youtu.be/ymRJNSUecV4?si=YzBOuWssm4WVLHnQ");
    songs.push_back("https://youtu.be/0EqHqPvXcMU?si=3B-NQzBLV3HQTi3x");

    for(const string& song : songs){
        submitDownload(song, clientFd);
    }
}

void handleExitCommand(int clientFd, const string& args) {
    cout << "[EXIT] Cliente " << clientFd << " solicitó cerrar el servidor" << endl;
    extern bool serverRunning;
    serverRunning = false;
}