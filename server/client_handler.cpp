#include "client_handler.hpp"
#include "epoll_handler.hpp"
#include "../commands/command_handler.hpp"
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

map<int, string> clientBuffers;

void handleServerEvent(int fd, void* data) {
    int epollFd = *(int*)data;
    while (acceptNewClient(fd, epollFd) >= 1) {}
}

void handleClientEvent(int fd, void* data) {
    int epollFd = *(int*)data;
    
    int isMessage = receiveFromClient(fd, epollFd);
    if (isMessage == 1) {
        processCommands(fd);
    } else if (isMessage < 0) {
        disconnectClient(fd, epollFd);
    }
}

int acceptNewClient(int serverSocket, int epollFd) {
	struct sockaddr_in clientAddress;
	socklen_t client_len = sizeof(clientAddress);

	int clientFd = accept(serverSocket, (struct sockaddr *)&clientAddress, &client_len);

	if (clientFd < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return 0;
		}
		cerr << "[ERROR] Error en accept\n";
		return -1;
	}

	// Configurar como no bloqueante
	int flags = fcntl(clientFd, F_GETFL, 0);
	if (flags < 0) {
		cerr << "[ERROR] Error obteniendo flags del socket cliente\n";
		close(clientFd);
		return -1;
	}

	if (fcntl(clientFd, F_SETFL, flags | O_NONBLOCK) < 0) {
		cerr << "[ERROR] Error configurando socket cliente como no bloqueante\n";
		close(clientFd);
		return -1;
	}

	// ===== CREAR CALLBACK PARA EL CLIENTE =====
	int *epollFd_ptr = new int(epollFd); // ← Crear en heap
	if (addToEpoll(epollFd, clientFd, handleClientEvent, epollFd_ptr) < 0) {
		cerr << "[ERROR] No se pudo agregar cliente a epoll\n";
		delete epollFd_ptr;
		close(clientFd);
		return -1;
	}

	cout << "[SERVER] Cliente conectado (FD " << clientFd << ")\n";
	return clientFd;
}

int receiveFromClient(int clientFd, int epollFd) {
	char buffer[1024];
	while (true) {
		ssize_t bytes_recv = recv(clientFd, buffer, sizeof(buffer), 0);

		if (bytes_recv > 0) { // hay datos
			// Protección contra buffer overflow
			if (clientBuffers[clientFd].size() + bytes_recv > 16 * 1024) {
				cerr << "[ERROR] Buffer overflow para cliente " << clientFd << "\n";
				epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
				close(clientFd);
				clientBuffers.erase(clientFd);
				return -1;
			}

			clientBuffers[clientFd].append(buffer, bytes_recv);

			// Verificar si tenemos comando completo
			if (clientBuffers[clientFd].find('\n') != string::npos) {
				return 1; // Comando completo recibido
			}
			continue;

		} else if (bytes_recv == 0) { // conexión cerrada (informar pero no manejar)
			cout << "[WARNING] Cliente " << clientFd << " cerró la conexión inesperadamente\n";
			return -1;

		} else if (errno == EAGAIN || errno == EWOULDBLOCK) { // no hay más datos disponibles
			// Verificar si ya tenemos un comando completo en el buffer
			return clientBuffers[clientFd].find('\n') != string::npos ? 1 : 0;

		} else { // error real
			cerr << "[ERROR] Error en recv para cliente " << clientFd << ": " << strerror(errno) << "\n";
			return -1;
		}
	}
}

void processCommands(int clientFd) {
	string &buffer = clientBuffers[clientFd];
	size_t pos;

	// Procesar TODOS los comandos que tengan \n
	while ((pos = buffer.find('\n')) != string::npos) {
		string command = buffer.substr(0, pos);
		buffer.erase(0, pos + 1); // Remover comando procesado (incluye \n)
		// Ignorar comandos vacíos
		if (command.empty()) {
			continue;
		}

		cout << "[CLIENT " << clientFd << "] Comando: " << command << "\n";
		handleCommand(clientFd, command);
	}
}

void disconnectClient(int clientFd, int epollFd) {
    removeFromEpoll(epollFd, clientFd);
    close(clientFd);
    clientBuffers.erase(clientFd);
    cout << "[INFO] Cliente " << clientFd << " desconectado\n";
}

void closeAllClients() {
    for (auto& pair : clientBuffers) {
        close(pair.first);
    }
    clientBuffers.clear();
}