#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

int main(int argc, char *argv[]) {
	// Verificar argumentos
	if (argc != 3) {
		cerr << "Uso: " << argv[0] << " <IP_SERVIDOR> <PUERTO>\n";
		cerr << "Ejemplo: " << argv[0] << " 192.168.1.120 46505\n";
		return 1;
	}

	string serverIP = argv[1];
	int serverPort = atoi(argv[2]);

	cout << "[CLIENT] Conectando a " << serverIP << ":" << serverPort << "...\n";

	// Crear socket
	int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket < 0) {
		cerr << "[ERROR] No se pudo crear socket: " << strerror(errno) << "\n";
		return 1;
	}

	// Configurar dirección del servidor
	struct sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(serverPort);

	if (inet_pton(AF_INET, serverIP.c_str(), &serverAddress.sin_addr) <= 0) {
		cerr << "[ERROR] Dirección IP inválida: " << serverIP << "\n";
		close(clientSocket);
		return 1;
	}

	// Conectar al servidor
	if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
		cerr << "[ERROR] No se pudo conectar al servidor: " << strerror(errno) << "\n";
		close(clientSocket);
		return 1;
	}

	cout << "[SUCCESS] Conectado al servidor!\n";
	cout << "\n========================================\n";
	cout << "Comandos disponibles:\n";
	cout << "  EXIT     - Cerrar servidor\n";
	cout << "========================================\n\n";

	string command;

	while (true) {
		cout << "> ";
		getline(cin, command);

		// Comando local para salir
		if (command == "quit" || command == "exit" || command == "q") {
			cout << "[CLIENT] Desconectando...\n";
			break;
		}

		// Ignorar líneas vacías
		if (command.empty()) {
			continue;
		}

		// Añadir \n al final del comando
		command += "\n";

		// Enviar comando al servidor
		ssize_t bytes_sent = send(clientSocket, command.c_str(), command.size(), 0);

		if (bytes_sent < 0) {
			cerr << "[ERROR] Error enviando comando: " << strerror(errno) << "\n";
			break;
		} else if (bytes_sent == 0) {
			cout << "[WARNING] Servidor cerró la conexión\n";
			break;
		}

		cout << "[CLIENT] Comando enviado (" << bytes_sent << " bytes)\n";

		char buf[256]; // ← Array de 256 caracteres
		memset(buf, 0, sizeof(buf)); // Limpiar el buffer
		ssize_t bytes = recv(clientSocket, buf, sizeof(buf) - 1, 0);

		cout << buf << endl;
		// Si enviamos EXIT, cerramos después de un momento
		if (command == "EXIT\n") {
			cout << "[CLIENT] Comando EXIT enviado, el servidor se cerrará...\n";
			sleep(1);
			break;
		}
	}

	// Cerrar socket
	close(clientSocket);
	cout << "[CLIENT] Desconectado\n";

	return 0;
}