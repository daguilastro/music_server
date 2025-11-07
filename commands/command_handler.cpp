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
	commandHandlers["ADD"] = handleAddCommand;
	commandHandlers["EXIT"] = handleExitCommand;
	commandHandlers["INDEX"] = handleIndexCommand;
	commandHandlers["GET"] = handleGetCommand;
	commandHandlers["SEARCH"] = handleSearchCommand;
	commandHandlers["BUILD"] = handleBuildCommand;
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

void handleBuildCommand(int clientFd, const string &args) {
	cout << "[BUILD] Iniciando construcción de canciones hardcodeadas..." << endl;

	// Canciones hardcodeadas
	struct SongData {
		string url;
		string artist;
		string title;
	};

	vector<SongData> songs = {
		{ "https://music.youtube.com/watch?v=3T9hxB5_dn4&si=9YhWtIXUvKyS8NBp", "nanahira", "fly away!" },
		{ "https://music.youtube.com/watch?v=lKmIhmFhjUg&si=wvJYfXCOwyxlPC4D", "nanahira", "unya pa pa" },
		{ "https://music.youtube.com/watch?v=2JJtVae2jl8&si=oCs2luZq5HXTdHOj", "nanahira", "sang it" },
		{ "https://music.youtube.com/watch?v=11NDsq0UK7c&si=P_TFUGcThU8RdD2U", "miku", "triple baka" }
	};

	int successCount = 0;
	int skipCount = 0;

	for (const auto &song : songs) {
		cout << "[BUILD] Procesando: " << song.title << " - " << song.artist << endl;

		// Verificar duplicado
		if (isDuplicateURL(globalDB, song.url.c_str())) {
			cout << "[BUILD] Ya existe: " << song.title << endl;
			skipCount++;
			continue;
		}

		// Generar filename
		string filename = song.title;
		for (char &c : filename) {
			if (!isalnum(c) && c != '_' && c != '-') {
				c = '_';
			}
		}
		filename += ".mp3";

		// Añadir canción
		int songId = addSong(globalDB,
			song.title.c_str(),
			filename.c_str(),
			song.url.c_str(),
			song.artist.c_str(),
			0);

		if (songId < 0) {
			cerr << "[BUILD] ERROR al añadir: " << song.title << endl;
			continue;
		}

		cout << "[BUILD] Añadida: [" << songId << "] " << song.title << " - " << song.artist << endl;

		// Iniciar descarga
		submitDownload(song.url, clientFd);

		successCount++;
	}

	// Enviar respuesta
	stringstream response;
	response << "BUILD_COMPLETE total=" << songs.size()
			 << " added=" << successCount
			 << " skipped=" << skipCount << "\n";

	string resp = response.str();
	send(clientFd, resp.c_str(), resp.size(), 0);

	cout << "[BUILD] Completado: " << successCount << " añadidas, "
		 << skipCount << " omitidas" << endl;
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

	// URL válida, dar luz verde al cliente
	string response = "OK_ADD\n";
	send(clientFd, response.c_str(), response.size(), 0);
	cout << "[ADD] URL aceptada, esperando comando INDEX..." << endl;
}

void handleIndexCommand(int clientFd, const string &args) {
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

	// Añadir canción a la database (SOLO EN MEMORIA)
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

	// Enviar confirmación
	stringstream response;
	response << "INDEXED id=" << songId << "\n";
	string resp = response.str();
	send(clientFd, resp.c_str(), resp.size(), 0);

	cout << "[INDEX] Canción añadida e indexada en memoria: [" << songId << "] "
		 << title << " - " << artist << endl;

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
	SearchResult result = searchSongs(globalDB, query.c_str(), true, true);

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