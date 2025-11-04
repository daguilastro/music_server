#include "server_utilities.hpp"
#include "socket_utilities.hpp"
#include "worker.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

map<int, string> clientBuffers;
bool serverRunning = true;
map<string, void (*)(int, string)> commandHandlers;
vector<WorkerInfo> workers;

void initializeCommandHandlers() {
	commandHandlers["ADD"] = handleAddCommand;
	commandHandlers["EXIT"] = handleExitCommand;

	cout << "[Server] Command handlers initialized" << endl;
}

bool initializeWorkers(int epollFd) {
	cout << "[Server] Inicializando 4 workers..." << endl;

	for (int i = 0; i < 4; i++) {
		int pipeToWorker[2];
		int pipeFromWorker[2];

		if (pipe(pipeToWorker) == -1 || pipe(pipeFromWorker) == -1) {
			perror("pipe");
			return false;
		}

		pid_t pid = fork();

		if (pid == -1) {
			perror("fork");
			return false;
		}

		if (pid == 0) {
			// ===== PROCESO HIJO (WORKER) =====
			close(pipeToWorker[1]);
			close(pipeFromWorker[0]);
			workerProcess(pipeToWorker[0], pipeFromWorker[1], i);
			exit(0);
		}

		// ===== PROCESO PADRE (SERVIDOR) =====
		close(pipeToWorker[0]);
		close(pipeFromWorker[1]);

		// Configurar como no bloqueante
		int flags = fcntl(pipeFromWorker[0], F_GETFL, 0);
		fcntl(pipeFromWorker[0], F_SETFL, flags | O_NONBLOCK);

		// Crear WorkerInfo
		WorkerInfo worker;
		worker.pid = pid;
		worker.pipe_read_fd = pipeFromWorker[0];
		worker.pipe_write_fd = pipeToWorker[1];
		worker.state = WORKER_IDLE;
		worker.current_request_id = 0;
		workers.push_back(worker);

		// ===== AÑADIR WORKER A EPOLL CON CALLBACK =====
		if (addToEpoll(epollFd, pipeFromWorker[0], handleWorkerEvent, &workers.back()) < 0) {
			cerr << "[ERROR] No se pudo añadir worker a epoll\n";
			return false;
		}

		cout << "[Server] Worker " << i << " creado (PID: " << pid << ")" << endl;
	}

	cout << "[Server] Todos los workers inicializados" << endl;
	return true;
}

void handleClientEvent(int fd, void *data) {
	int epollFd = *(int *)data;

	int isMessage = receiveFromClient(fd, epollFd);
	if (isMessage == 1) {
		processCommands(fd);
	} else if (isMessage < 0) {
		epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr);
		close(fd);
		clientBuffers.erase(fd);
		cout << "[INFO] Cliente " << fd << " desconectado\n";
	}
}

void handleWorkerEvent(int fd, void *data) {
	WorkerInfo *worker = static_cast<WorkerInfo *>(data);

	WorkerMessage response;
	if (!readWorkerMessage(fd, response)) {
		cerr << "[Server] Error leyendo de worker " << worker->pid << endl;
		return;
	}

	// Procesar mensaje FINISHED
	if (response.type == MSG_FINISHED) {
		cout << "[Server] Worker " << worker->pid << " terminó request "
			 << response.request_id << ": " << response.data << endl;

		// Marcar worker como IDLE
		worker->state = WORKER_IDLE;
		worker->current_request_id = 0;

		// TODO: Intentar asignar más trabajo
	} else {
		cerr << "[Server] Tipo de mensaje inesperado de worker" << endl;
	}
}

void handleAddCommand(int clientFd, string args) {
}

void handleExitCommand(int clientFd, string args) {
	serverRunning = false;
}

void handleServerEvent(int fd, void *data) {
	int epollFd = *(int *)data;

	cout << "[DEBUG] Nueva conexión entrante en FD " << fd << endl;

	// Aceptar todas las conexiones pendientes
	while (acceptNewClient(fd, epollFd) >= 1) {
		// Continúa aceptando mientras haya conexiones
	}
}

void handleCommand(int clientFd, string request) {
	cout << "[REQUEST] Cliente " << clientFd << ": " << request << endl;

	// Separar el comando de sus argumentos
	string command;
	string arguments;

	size_t space_pos = request.find(' ');
	if (space_pos != string::npos) {
		command = request.substr(0, space_pos); // "ADD"
		arguments = request.substr(space_pos + 1); // "https://..."
	} else {
		// No hay argumentos: "SKIP"
		command = request;
		arguments = "";
	}
	if (commandHandlers.count(command)) {
		commandHandlers[command](clientFd, arguments);
	} else {
		cerr << "[ERROR] Comando desconocido: " << command << endl;
	}
}

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

int addToEpoll(int epollFd, int fd, void (*handler)(int, void *), void *data) {
	// Crear el callback
	EpollCallbackData *callback = new EpollCallbackData;
	callback->fd = fd;
	callback->handler = handler;
	callback->data = data;

	// Configurar evento de epoll
	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.ptr = callback;

	cout << "[DEBUG] añadiendo FD " << fd << " con callback a epoll (epollFd=" << epollFd << ")\n";

	// Añadir a epoll
	if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event) != 0) {
		cerr << "[ERROR] No se pudo agregar FD " << fd << " a epoll: "
			 << strerror(errno) << "\n";
		delete callback; // Limpiar si falla
		return -1;
	}

	cout << "[DEBUG] FD " << fd << " agregado exitosamente a epoll\n";
	return 0;
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

int mainloop(int &serverSocket) {
	cout << "[DEBUG] Creando epoll\n";
	int epollFd = crearEpoll();
	cout << "[DEBUG] Epoll creado! " << epollFd << "\n";

	if (epollFd < 0) {
		cerr << "[ERROR] No se pudo crear epoll\n";
		return -1;
	}

	cout << "[DEBUG] añadiendo el server a epoll\n";

	// ===== AÑADIR SERVER SOCKET CON CALLBACK =====
	int *epollFd_ptr = new int(epollFd); // ← Crear en heap
	if (addToEpoll(epollFd, serverSocket, handleServerEvent, epollFd_ptr) < 0) {
		cerr << "[ERROR] No se pudo añadir server a epoll\n";
		delete epollFd_ptr;
		close(epollFd);
		return -1;
	}
	// inicializar los comandos jajajaj
	initializeCommandHandlers();
	// ===== INICIALIZAR WORKERS =====
	if (!initializeWorkers(epollFd)) {
		cerr << "[ERROR] No se pudieron inicializar workers\n";
		close(epollFd);
		return -1;
	}

	struct epoll_event events[200];
	serverRunning = true;

	cout << "[SERVER] SERVIDOR CREADO Y ESPERANDO...\n";

	// ===== MAINLOOP SIMPLIFICADO =====
	while (serverRunning) {
		int nfds = epoll_wait(epollFd, events, 200, -1);

		if (nfds < 0) {
			cerr << "[ERROR] Error en epoll_wait\n";
			break;
		}

		// Procesar todos los eventos
		for (int i = 0; i < nfds; i++) {
			EpollCallbackData *callback = (EpollCallbackData *)events[i].data.ptr;

			// Ejecutar el handler correspondiente
			callback->handler(callback->fd, callback->data);
		}
	}

	// ===== CLEANUP =====
	cout << "[Server] Cerrando servidor...\n";

	// Cerrar workers
	for (auto &worker : workers) {
		WorkerMessage shutdown;
		shutdown.type = MSG_SHUTDOWN;
		writeWorkerMessage(worker.pipe_write_fd, shutdown);
		close(worker.pipe_write_fd);
		close(worker.pipe_read_fd);
	}

	// Cerrar clientes
	for (auto &pair : clientBuffers) {
		close(pair.first);
	}
	clientBuffers.clear();

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