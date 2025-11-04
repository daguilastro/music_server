#pragma once
#include "epoll_handler.hpp"
#include "client_handler.hpp"
#include "../worker/worker_manager.hpp"
#include "../commands/command_handler.hpp"
#include "../network/upnp.hpp"
#include <iostream>

using namespace std;

// Variables globales compartidas
extern bool serverRunning;

// Funciones principales
void runServer(int& serverSocket);
int mainloop(int& serverSocket);