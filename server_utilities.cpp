#include "server_utilities.hpp"
#include "socket_utilities.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

map<int, string> clientBuffers;
bool serverRunning = true;

int createTcpServerSocket() {
	int fd = socket(IPv4, STREAM | NO_BLOQUEANTE, 0);
	if (fd < 0) {
		cerr << "error creando el socket: " << strerror(errno) << "\n";
	}
	int yes = 1;
	if (
		setsockopt(fd, SOCKET_LEVEL, REUSE_ADDR, &yes, sizeof(yes)) != 0 || setsockopt(fd, SOCKET_LEVEL, REUSE_PORT, &yes, sizeof(yes)) != 0) {
		cerr << "error cambiando las opciones del socket: " << strerror(errno) << "\n";
	}

	sockaddr_in socketAdress {};
	socketAdress.sin_family = IPv4;
	socketAdress.sin_port = htons(0);
	socketAdress.sin_addr.s_addr = htonl(RECIEVE_EVERYWHERE);

	if (bind(fd, (sockaddr *)&socketAdress, sizeof(socketAdress)) < 0) {
		cerr << "error en el bind del socket: " << strerror(errno) << "\n";
	}

	if (listen(fd, 128) < 0) {
		cerr << "error en el listen del socket: " << strerror(errno) << "\n";
	}

	cout << "Socket creado exitosamente!!\n";

	return fd;
}

int crearEpoll() {
	int epollFd = epoll_create1(0);
	if (epollFd < 0) {
		cerr << "[ERROR] No se pudo crear epoll\n";
		return -1;
	}
	return epollFd;
}

int addSocketToEpoll(int epollFd, int listenSocket) {
	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = listenSocket;

	cout << "[DEBUG] añadiendo FD " << listenSocket << " a epoll (epollFd=" << epollFd << ")\n";

	if (epoll_ctl(epollFd, EPOLL_CTL_ADD, listenSocket, &event) != 0) {
		cerr << "[ERROR] No se pudo agregar FD " << listenSocket << " a epoll: " << strerror(errno) << "\n";
		return -1;
	}

	cout << "[DEBUG] FD " << listenSocket << " agregado exitosamente a epoll\n";
	return 0;
}

int acceptNewClient(int serverSocket, int epollFd) {
	struct sockaddr_in clientAddres;
	socklen_t client_len = sizeof(clientAddres);

	int clientFd = accept(serverSocket, (struct sockaddr *)&clientAddres, &client_len);

	if (clientFd < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			// No hay mas conexiones pendientes
			return 0;
		}
		cerr << "[ERROR] Error en accept\n";
		return -1;
	}
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

	if (addSocketToEpoll(epollFd, clientFd) < 0) {
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

void handleCommand(int clientFd, string command) {
	cout << "[COMMAND] Cliente " << clientFd << ": " << command << "\n";

	// TODO: Implementar lógica de comandos
	if (command == "EXIT") {
		cout << "[SERVER] CERRANDO SERVIDOR.\n";
		closeServer();
	} else if (command == "ADD") {
		cout << "[SERVER] LOGICA DE URL AÑADIR MUSICA DE YOUTUBE...\n";
	} else if (command == "SKIP") {
		cout << "[ACTION] Saltando canción...\n";
	} else if (command.find("VOLUME") == 0) {
		cout << "[ACTION] Cambiando volumen...\n";
	} else {
	}
}

void closeServer() {
	serverRunning = false;
}

int mainloop(int &serverSocket) {
	cout << "[DEBUG] Creando epoll\n";
	int epollFd = crearEpoll();
	cout << "[DEBUG] Epoll creado! " << epollFd << "\n";
	if (epollFd < 0) {
		cerr << "[ERROR] No se pudo crear epoll\n";
		return -1;
	}
	cout << "[DEBUG] añadiendo el server a epoll\n";
	if (addSocketToEpoll(epollFd, serverSocket) < 0) {
		cerr << "[ERROR] No se pudo añadir server a epoll\n";
		close(epollFd);
		return -1;
	}
	struct epoll_event events[200];
	serverRunning = true;
	cout << "[SERVER] SERVIDOR CREADO Y ESPERANDO...\n";
	while (serverRunning) {
		int nfds = epoll_wait(epollFd, events, 200, -1);
		if (nfds < 0) {
			cerr << "[ERROR] Error en epoll_wait\n";
			break;
		}
		// Procesar todos los eventos
		for (int i = 0; i < nfds; i++) {
			int connectionFd = events[i].data.fd;
			uint32_t event = events[i].events;
			// Evento en el servidor = nueva conexión
			if (connectionFd == serverSocket) {
				// Aceptar todas las conexiones pendientes
				while (true) {
					if (acceptNewClient(serverSocket, epollFd) < 1) {
						break;
					}
				}
				continue;
			}
			if (event & EPOLLIN) {
				int isMessage = receiveFromClient(connectionFd, epollFd);
				if (isMessage == 1) {
					processCommands(connectionFd);
				} else if (isMessage == 0) {
					continue;
				} else {
					epoll_ctl(epollFd, EPOLL_CTL_DEL, connectionFd, nullptr);
					close(connectionFd);
					clientBuffers.erase(connectionFd);
					cout << "[INFO] Cliente " << connectionFd << " desconectado\n";
				}
			}
			if (event & EPOLLOUT) {
				continue;
			}
		}
		if (!serverRunning) break;
	}
	for (auto &pair : clientBuffers) {
		int clientFd = pair.first;
		cout << "[SERVER] Cerrando cliente " << clientFd << "\n";
		epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
		close(clientFd);
	}
	clientBuffers.clear();

	// Cerrar epoll
	close(epollFd);
	return 0;
}
void runServer(int &serverSocket) {
	cout << "1. TCP UPnP public server\n2. TCP Local server\n[CLIENT]: ";
	int type;
	cin >> type;
    UPnPRouter router;
	if (type == 1) {
		int internalPort;
		connectUPnP(serverSocket, 15069, router);
	} else {
		connectLocal(serverSocket);
	}
	mainloop(serverSocket);
    closeRouterPort(router, 15069);
    close(serverSocket);
	return;
}