#pragma once
#include <map>
#include <string>

using namespace std;

// Variables globales
extern map<string, void(*)(int, const string&)> commandHandlers;

// Funciones
void initializeCommandHandlers();
void handleCommand(int clientFd, const string& request);
void handleAddCommand(int clientFd, const string& args);
void handleExitCommand(int clientFd, const string& args);