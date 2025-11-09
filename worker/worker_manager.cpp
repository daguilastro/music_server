#include "worker_manager.hpp"
#include "worker.hpp"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <cstring>

using namespace std;

vector<WorkerInfo> workers;
queue<DownloadRequest> downloadQueue;

bool initializeWorkers(int epollFd, int numWorkers) {
	cout << "[Server] Inicializando " << numWorkers << " workers..." << endl;

	for (int i = 0; i < numWorkers; i++) {
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
			close(pipeToWorker[1]);
			close(pipeFromWorker[0]);
			workerProcess(pipeToWorker[0], pipeFromWorker[1], i);
			exit(0);
		}

		close(pipeToWorker[0]);
		close(pipeFromWorker[1]);

		int flags = fcntl(pipeFromWorker[0], F_GETFL, 0);
		fcntl(pipeFromWorker[0], F_SETFL, flags | O_NONBLOCK);

		WorkerInfo worker;
		worker.pid = pid;
		worker.pipe_read_fd = pipeFromWorker[0];
		worker.pipe_write_fd = pipeToWorker[1];
		worker.state = WORKER_IDLE;
		workers.push_back(worker);

		extern int addToEpoll(int epollFd, int fd, void (*handler)(int, void *), void *data);

		// ===== PASAR EL ÍNDICE EN LUGAR DEL PUNTERO =====
		int *workerIndex = new int(i); // Guardar el índice en el heap
		if (addToEpoll(epollFd, pipeFromWorker[0], handleWorkerEvent, workerIndex) < 0) {
			cerr << "[ERROR] No se pudo añadir worker a epoll\n";
			delete workerIndex;
			return false;
		}

		cout << "[Server] Worker " << i << " creado (PID: " << pid << ")" << endl;
	}

	cout << "[Server] Todos los workers inicializados" << endl;
	return true;
}

void handleWorkerEvent(int fd, void *data) {
	// ===== RECUPERAR EL WORKER POR ÍNDICE =====
	int workerIndex = *(int *)data;
	WorkerInfo *worker = &workers[workerIndex];

	cout << "[DEBUG] handleWorkerEvent llamado para worker " << workerIndex
		 << " (PID: " << worker->pid << ")" << endl;

	WorkerMessage response;
	if (!readWorkerMessage(fd, response)) {
		cerr << "[Server] Error leyendo de worker " << worker->pid << endl;
		return;
	}

	if (response.type == MSG_FINISHED) {
		string url(response.data, response.data_length);
		cout << "[Server] Worker " << worker->pid << " terminó: " << url << endl;

		cout << "[DEBUG] worker->currentRequest.url = " << worker->currentRequest.url << endl;
		cout << "[DEBUG] worker->currentRequest.clientFd = " << worker->currentRequest.clientFd << endl;

		int clientFd = worker->currentRequest.clientFd;

		if (clientFd > 0) {
			string msg = "Descarga completada: " + url + "\n";
			ssize_t sent = send(clientFd, msg.c_str(), msg.length(), 0);

			if (sent > 0) {
				cout << "[Server] Respuesta enviada a cliente " << clientFd
					 << " (" << sent << " bytes)" << endl;
			} else {
				cerr << "[Server] Error enviando a cliente " << clientFd
					 << ": " << strerror(errno) << endl;
			}
		} else {
			cerr << "[Server] clientFd inválido: " << clientFd << endl;
		}

		worker->state = WORKER_IDLE;
		worker->currentRequest.clientFd = -1;
		assignPendingDownloads();
	}

	else if (response.type == MSG_METADATA) {
		char metadata[2048];
		memcpy(metadata, response.data, response.data_length);

		char *title = strtok(metadata, "\n");
		char *artist = strtok(nullptr, "\n");
		char *duration = strtok(nullptr, "\n");
		char *url = strtok(nullptr, "\n");
		if (!title || !artist || !duration || !url) {
			cerr << "[ERROR] Metadatos incompletos" << endl;
			return;
		}

		Song songMetadata;

		strcpy(songMetadata.title, title);
		cout << "[DEBUG] title " << title << endl;
		strcpy(songMetadata.artist, artist);
		cout << "[DEBUG] artist " << artist << endl;
		strcpy(songMetadata.url, url);
		cout << "[DEBUG] url " << url << endl;
		songMetadata.duration = atoi(duration);
		cout << "[DEBUG] duration " << duration << endl;

		string filename = title;
		for (char &c : filename) {
			if (!isalnum(c) && c != '_' && c != '-' && c != ' ') {
				c = '_';
			}
		}
		filename += ".mp3";

		cout << "[DEBUG] filename " << filename << endl;
		strcpy(songMetadata.filename, filename.c_str());
		songMetadata.id = 0;

		indexSong(songMetadata);
	}
}

void submitDownload(const string &url, int clientFd) {
	cout << "[Server] Añadiendo a cola: " << url << " (cliente: " << clientFd << ")" << endl;

	DownloadRequest req;
	req.url = url;
	req.clientFd = clientFd;

	downloadQueue.push(req);
	assignPendingDownloads();
}

void assignPendingDownloads() {
	while (!downloadQueue.empty()) {
		WorkerInfo *idleWorker = nullptr;
		for (auto &worker : workers) {
			if (worker.state == WORKER_IDLE) {
				idleWorker = &worker;
				break;
			}
		}

		if (idleWorker == nullptr) {
			cout << "[Server] No hay workers libres, " << downloadQueue.size()
				 << " descargas en cola" << endl;
			break;
		}

		DownloadRequest req = downloadQueue.front();
		downloadQueue.pop();

		WorkerMessage request;
		request.type = MSG_REQUEST;
		request.data_length = req.url.length();
		memcpy(request.data, req.url.c_str(), request.data_length);
		request.data[request.data_length] = '\0';

		idleWorker->currentRequest = req;

		cout << "[DEBUG] Guardado en worker PID=" << idleWorker->pid
			 << " -> url=" << idleWorker->currentRequest.url
			 << ", clientFd=" << idleWorker->currentRequest.clientFd << endl;

		if (writeWorkerMessage(idleWorker->pipe_write_fd, request)) {
			idleWorker->state = WORKER_BUSY;
			cout << "[Server] Asignado al worker " << idleWorker->pid
				 << ": " << req.url << endl;
		} else {
			cerr << "[ERROR] No se pudo enviar request al worker\n";
			downloadQueue.push(req);
			idleWorker->currentRequest.clientFd = -1;
			break;
		}
	}
}

void shutdownWorkers() {
	cout << "[Server] Cerrando workers..." << endl;

	for (auto &worker : workers) {
		WorkerMessage shutdown;
		shutdown.type = MSG_SHUTDOWN;
		writeWorkerMessage(worker.pipe_write_fd, shutdown);
		close(worker.pipe_write_fd);
		close(worker.pipe_read_fd);
	}

	workers.clear();
}