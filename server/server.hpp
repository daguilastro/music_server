#pragma once
#include "epoll_handler.hpp"
#include "client_handler.hpp"
#include "../worker/worker_manager.hpp"
#include "../commands/command_handler.hpp"
#include "../network/upnp.hpp"
#include <iostream>
#include "../indexation/database.hpp"

using namespace std;

// Variables globales compartidas
extern bool serverRunning;
extern SongDatabase* globalDB;

// Funciones principales
void runServer(int& serverSocket);
int mainloop(int& serverSocket);