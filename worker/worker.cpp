#include "worker.hpp"
#include <sys/types.h>

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

		int pipeMetadata[2];
		pipe(pipeMetadata);

		// ===== EJECUTAR YT-DLP =====
		pid_t pid = fork();

		if (pid == 0) {
			close(pipeMetadata[0]);
			dup2(pipeMetadata[1], STDOUT_FILENO);
			close(pipeMetadata[1]);

			execl("/usr/bin/yt-dlp",
				"yt-dlp",
				"--print", "before_dl:%(title)s\n%(artist,uploader)s\n%(duration)s",
				"-x", "--audio-format", "mp3",
				"--quiet",
				"--no-warnings",
				"--extractor-args", "youtube:player_client=android",
				"-o", "songs/%(title)s.%(ext)s",
				url.c_str(),
				(char *)NULL);

			// Si execl falla
			cerr << "[Worker " << worker_id << "] Error ejecutando yt-dlp: "
				 << strerror(errno) << endl;
			exit(1);
		} 
		else if (pid > 0) {
			// Proceso padre: esperar a que termine yt-dlp
			int status;
			close(pipeMetadata[1]);

			char metadata[2048];
			ssize_t bytesMetadata = 0;
			while (bytesMetadata < (ssize_t)sizeof(metadata)) {
				ssize_t bytesRead = read(pipeMetadata[0], metadata, sizeof(metadata));
				if (bytesRead < 0) {
					cerr << "error" << strerror(errno) << endl;
				}
				if (bytesRead == 0) {
					break;
				}
				bytesMetadata += bytesRead;
			}
			close(pipeMetadata[0]);

			WorkerMessage messageMetadata;
			
			cout << "[DEBUG WORKER] url en url.c_str() " << url.c_str() << endl;
			memcpy(metadata + bytesMetadata, url.c_str(), url.size());
			bytesMetadata += url.length();
			metadata[bytesMetadata] = '\0';
			cout << "[DEBUG WORKER] url copiada en metadata[2048]" << metadata << endl;
			messageMetadata.type = MSG_METADATA;
			memcpy(messageMetadata.data, metadata, bytesMetadata);
			messageMetadata.data[bytesMetadata] = '\0';
			messageMetadata.data_length = bytesMetadata;

			if (!writeWorkerMessage(write_fd, messageMetadata)) {
				cerr << "[Worker " << worker_id << "] Error enviando metadatos" << endl;
				break;
			}

			waitpid(pid, &status, 0);
			close(pipeMetadata[0]);

			if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
				cout << "[Worker " << worker_id << "] Descarga completada: " << url << endl;
			} else {
				cerr << "[Worker " << worker_id << "] Error en descarga: " << url << endl;
			}
		} 
		else {
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