#include "server.hpp"

bool serverRunning = true;
SongDatabase *globalDB = nullptr;

void runServer(int &serverSocket) {
	cout << "1. TCP UPnP public server\n2. TCP Local server\n[CLIENT]: ";
	int type;
	cin >> type;
	UPnPRouter router;

	if (type == 1) {
		connectUPnP(serverSocket, 8085, router);
	} else {
		connectLocal(serverSocket);
	}

	mainloop(serverSocket);
	saveDatabase(globalDB, "music_database.bin");
	freeDatabase(globalDB);
	closeRouterPort(router, 8085);
	close(serverSocket);
}

int mainloop(int &serverSocket) {
	cout << "[SERVER] Cargando base de datos..." << endl;
	globalDB = loadDatabase("music_database.bin");
	if (!globalDB) {
		cerr << "[ERROR] No se pudo cargar la base de datos" << endl;
		return -1;
	}
	cout << "[SERVER] Base de datos lista (" << getSongCount(globalDB) << " canciones)" << endl;

	int epollFd = createEpoll();
	if (epollFd < 0) {
		return -1;
	}

	// Añadir server socket
	int *epollFd_ptr = new int(epollFd);
	addToEpoll(epollFd, serverSocket, handleServerEvent, epollFd_ptr);

	// Inicializar comandos y workers
	initializeCommandHandlers();
	initializeWorkers(epollFd);

	struct epoll_event events[200];
	serverRunning = true;

	cout << "[SERVER] SERVIDOR CREADO Y ESPERANDO...\n";

	while (serverRunning) {
		int nfds = epoll_wait(epollFd, events, 200, -1);

		for (int i = 0; i < nfds; i++) {
			EpollCallbackData *callback = (EpollCallbackData *)events[i].data.ptr;
			callback->handler(callback->fd, callback->data);
		}
	}

	// Cleanup
	shutdownWorkers();
	closeAllClients();
	close(epollFd);
	return 0;
}