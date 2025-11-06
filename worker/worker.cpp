#include "worker.hpp"
#include <unistd.h>
#include <cstring>
#include <sys/wait.h>
#include <cerrno>
#include <iostream>

using namespace std;

bool writeWorkerMessage(int fd, const WorkerMessage &msg) {
	size_t total = 0;
	size_t msg_size = sizeof(WorkerMessage);
	char *buf = (char *)&msg;

	while (total < msg_size) {
		ssize_t written = write(fd, buf + total, msg_size - total);
		if (written < 0) {
			if (errno == EINTR) continue;
			return false;
		}
		total += written;
	}
	return true;
}

bool readWorkerMessage(int fd, WorkerMessage &msg) {
	size_t total = 0;
	size_t messageSize = sizeof(WorkerMessage);
	char *buf = (char *)&msg;

	while (total < messageSize) {
		ssize_t bytes_read = read(fd, buf + total, messageSize - total);
		if (bytes_read < 0) {
			if (errno == EINTR) continue;
			return false;
		}
		if (bytes_read == 0) {
			return false;
		}
		total += bytes_read;
	}
	return true;
}

void workerProcess(int read_fd, int write_fd, int worker_id) {
	cout << "[Worker " << worker_id << "] Iniciado con PID " << getpid() << endl;

	while (true) {
		WorkerMessage request;

		if (!readWorkerMessage(read_fd, request)) {
			cerr << "[Worker " << worker_id << "] Error leyendo, saliendo" << endl;
			break;
		}

		if (request.type == MSG_SHUTDOWN) {
			cout << "[Worker " << worker_id << "] SHUTDOWN recibido" << endl;
			break;
		}

		if (request.type != MSG_REQUEST) {
			continue;
		}

		string url(request.data, request.data_length);
		cout << "[Worker " << worker_id << "] Procesando: " << url << endl;

		// ===== EJECUTAR YT-DLP =====
		pid_t pid = fork();

		if (pid == 0) {
            
			// Si falla, intentar con la ruta alternativa
			execl("/usr/bin/yt-dlp",
				"yt-dlp",
				"-x", "--audio-format", "mp3",
				"--extractor-args", "youtube:player_client=android",
				"-o", "/tmp/downloads/%(title)s.%(ext)s",
				url.c_str(),
				(char *)NULL);

			// Si execl falla
			cerr << "[Worker " << worker_id << "] Error ejecutando yt-dlp: "
				 << strerror(errno) << endl;
			exit(1);
		} else if (pid > 0) {
			// Proceso padre: esperar a que termine yt-dlp
			int status;
			waitpid(pid, &status, 0);

			if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
				cout << "[Worker " << worker_id << "] Descarga completada: " << url << endl;
			} else {
				cerr << "[Worker " << worker_id << "] Error en descarga: " << url << endl;
			}
		} else {
			cerr << "[Worker " << worker_id << "] Error en fork" << endl;
		}

		WorkerMessage response;
		response.type = MSG_FINISHED;
		response.data_length = url.length();
		memcpy(response.data, url.c_str(), response.data_length);
		response.data[response.data_length] = '\0';

		if (!writeWorkerMessage(write_fd, response)) {
			cerr << "[Worker " << worker_id << "] Error enviando respuesta" << endl;
			break;
		}
	}

	close(read_fd);
	close(write_fd);
	exit(0);
}