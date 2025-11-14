#include "command_handler.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <regex>
#include <string>
#include <sys/socket.h>
#include <vector>

using namespace std;

map<string, void (*)(int, const string &)> commandHandlers;

void initializeCommandHandlers() {
	commandHandlers["ADD"] = handleAddCommand;	//	no está verificando que la url ya esté
	commandHandlers["EXIT"] = handleExitCommand;
	commandHandlers["GET"] = handleGetCommand;	//
	commandHandlers["SEARCH"] = handleSearchCommand;	// hay que implementar un search bueno.
	cout << "[Server] Command handlers initialized" << endl;
}

void handleCommand(int clientFd, const string &request) {
	cout << "[REQUEST] Cliente " << clientFd << ": " << request << endl;

	// Separar el comando de sus argumentos
	string command;
	string arguments;

	size_t space_pos = request.find(' ');
	if (space_pos != string::npos) {
		command = request.substr(0, space_pos); // "ADD"
		arguments = request.substr(space_pos + 1); // "https://..."
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

void handleGetCommand(int clientFd, const string &args) {
	if (args.empty()) {
		string error = "ERROR missing_id\n";
		send(clientFd, error.c_str(), error.size(), 0);
		return;
	}

	int songId = atoi(args.c_str());

	if (songId <= 0) {
		string error = "ERROR invalid_id\n";
		send(clientFd, error.c_str(), error.size(), 0);
		return;
	}

	cout << "[GET] Cliente " << clientFd << " solicitó canción ID: " << songId << endl;

	// Buscar canción
	Song *song = getSongById(globalDB, (uint32_t)songId);

	if (!song) {
		string error = "ERROR song_not_found\n";
		send(clientFd, error.c_str(), error.size(), 0);
		cout << "[GET] Canción no encontrada: ID " << songId << endl;
		return;
	}

	// Obtener offset en el archivo
	long offset = getSongOffsetInFile(globalDB, (uint32_t)songId);

	// Construir respuesta
	// Formato: SONG id|title|artist|filename|url|duration|offset
	stringstream response;
	response << "SONG "
			 << song->id << "|"
			 << song->title << "|"
			 << song->artist << "|"
			 << song->filename << "|"
			 << song->url << "|"
			 << song->duration << "|"
			 << offset << "\n";

	string resp = response.str();
	send(clientFd, resp.c_str(), resp.size(), 0);

	cout << "[GET] Enviada canción: [" << song->id << "] "
		 << song->title << " - " << song->artist
		 << " (offset: " << offset << " bytes)" << endl;
}

void handleAddCommand(int clientFd, const string &url) {
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
	
	// Iniciar descarga
	submitDownload(url, clientFd);
}

void handleSearchCommand(int clientFd, const string &args) {
	if (args.empty()) {
		string error = "ERROR missing_query\n";
		send(clientFd, error.c_str(), error.size(), 0);
		return;
	}

	// Limpiar query
	string query = args;
	query.erase(0, query.find_first_not_of(" \t"));
	query.erase(query.find_last_not_of(" \t\r\n") + 1);

	if (query.empty()) {
		string error = "ERROR empty_query\n";
		send(clientFd, error.c_str(), error.size(), 0);
		return;
	}

	cout << "[SEARCH] Cliente " << clientFd << " busca: \"" << query << "\"" << endl;

	// Siempre buscar en AMBOS índices
	SearchResult result = searchSongs(globalDB, query.c_str());

	if (result.count == 0) {
		string response = "SEARCH_RESULTS 0\n";
		send(clientFd, response.c_str(), response.size(), 0);
		cout << "[SEARCH] Sin resultados" << endl;
		freeSearchResult(&result);
		return;
	}

	// Construir respuesta
	stringstream response;
	response << "SEARCH_RESULTS " << result.count << "\n";

	for (int i = 0; i < result.count; i++) {
		Song *song = getSongById(globalDB, result.songIds[i]);
		if (song) {
			response << song->id << "|"
					 << song->title << "|"
					 << song->artist << "|"
					 << song->duration << "\n";
		}
	}

	string resp = response.str();
	send(clientFd, resp.c_str(), resp.size(), 0);

	cout << "[SEARCH] Enviados " << result.count << " resultados" << endl;

	freeSearchResult(&result);
}

void handleExitCommand(int clientFd, const string &args) {
	cout << "[EXIT] Cliente " << clientFd << " solicitó cerrar el servidor" << endl;
	extern bool serverRunning;
	serverRunning = false;
}