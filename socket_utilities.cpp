#include "socket_utilities.hpp"
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <iterator>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

int createSearchSocket() {
	cout << "[DEBUG] Creando socket de búsqueda UDP...\n";
	int fd = socket(IPv4, DGRAM, 0);

	if (fd < 0) {
		cerr << "[ERROR] No se pudo crear socket: " << strerror(errno) << "\n";
		return -1;
	}
	cout << "[DEBUG] Socket creado (fd: " << fd << ")\n";

	// Timeout de 3 segundos para colectar todas las respuestas SSDP
	struct timeval tv;
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
	cout << "[DEBUG] Timeout de 3 segundos configurado\n";

	struct sockaddr_in localAddress;
	localAddress.sin_family = IPv4;
	localAddress.sin_addr.s_addr = htonl(RECIEVE_EVERYWHERE);
	localAddress.sin_port = htons(0);

	cout << "[DEBUG] Haciendo bind...\n";
	if (bind(fd, (struct sockaddr *)&localAddress, sizeof(localAddress)) < 0) {
		cerr << "[ERROR] Error en bind: " << strerror(errno) << "\n";
		close(fd);
		return -1;
	}
	cout << "[DEBUG] Bind exitoso\n";

	return fd;
}

void askRouterUPnPURL(int searchSocket) {
	cout << "[DEBUG] Preparando mensaje SSDP M-SEARCH...\n";
	const char *ssdpRequest = "M-SEARCH * HTTP/1.1\r\n"
							  "HOST: 239.255.255.250:1900\r\n"
							  "MAN: \"ssdp:discover\"\r\n"
							  "MX: 2\r\n"
							  "ST: upnp:rootdevice\r\n"
							  "\r\n";

	struct sockaddr_in multicastAddress;
	multicastAddress.sin_family = IPv4;
	multicastAddress.sin_addr.s_addr = inet_addr("239.255.255.250");
	multicastAddress.sin_port = htons(1900);

	cout << "[DEBUG] Enviando SSDP M-SEARCH a 239.255.255.250:1900...\n";
	if (sendto(searchSocket, ssdpRequest, strlen(ssdpRequest), 0, (sockaddr *)&multicastAddress, sizeof(multicastAddress)) < 0) {
		cerr << "[ERROR] Error enviando SSDP: " << strerror(errno) << "\n";
		close(searchSocket);
		return;
	}
	cout << "[DEBUG] SSDP M-SEARCH enviado (" << strlen(ssdpRequest) << " bytes)\n";
}

vector<string> collectAllSSDPResponses(int searchSocket) {
	cout << "[DEBUG] ===== FASE 1: Colectando todas las respuestas SSDP =====\n";
	cout << "[DEBUG] Esperando respuestas durante 3 segundos...\n";
	
	vector<string> responses;
	char responseBuffer[2048];
	struct sockaddr_in responseAddress;
	socklen_t addressLength = sizeof(responseAddress);
	
	int responseCount = 0;
	
	while (true) {
		int n = recvfrom(searchSocket, responseBuffer, sizeof(responseBuffer) - 1, 0, (struct sockaddr *)&responseAddress, &addressLength);
		
		if (n < 0) {
			cout << "[DEBUG] Timeout - No hay más respuestas SSDP\n";
			break;
		}
		
		responseCount++;
		responseBuffer[n] = '\0';
		string response(responseBuffer);
		
		string responderIP = inet_ntoa(responseAddress.sin_addr);
		
		cout << "\n[DEBUG] --- Respuesta SSDP #" << responseCount << " ---\n";
		cout << "[DEBUG] De: " << responderIP << " (" << n << " bytes)\n";
		cout << "[DEBUG] ========================================\n";
		cout << response << "\n";
		cout << "[DEBUG] ========================================\n";
		
		responses.push_back(response);
	}
	
	cout << "\n[DEBUG] ===== Total respuestas SSDP colectadas: " << responses.size() << " =====\n\n";
	
	return responses;
}

vector<UPnPRouter> findAllValidUPnPRouters(int searchSocket) {
	vector<UPnPRouter> validRouters;
	
	// Fase 1: Colectar todas las respuestas SSDP
	vector<string> ssdpResponses = collectAllSSDPResponses(searchSocket);
	
	if (ssdpResponses.empty()) {
		cerr << "[ERROR] No se recibió ninguna respuesta SSDP\n";
		return validRouters;  // Retorna vector vacío
	}
	
	// Fase 2: Procesar cada respuesta SSDP una por una
	cout << "[DEBUG] ===== FASE 2: Procesando respuestas SSDP una por una =====\n";
	
	for (size_t i = 0; i < ssdpResponses.size(); i++) {
		cout << "\n[DEBUG] ========================================\n";
		cout << "[DEBUG] Procesando respuesta SSDP #" << (i + 1) << " de " << ssdpResponses.size() << "\n";
		cout << "[DEBUG] ========================================\n";
		
		string response = ssdpResponses[i];
		
		cout << "[DEBUG] Verificando campo LOCATION...\n";
		if (response.find("LOCATION:") == string::npos) {
			cout << "[DEBUG] Sin LOCATION, saltando\n";
			continue;
		}
		cout << "[DEBUG] LOCATION encontrado\n";
		
		cout << "[DEBUG] Parseando URL...\n";
		UPnPURL urlParts = parseURL(response);
		if (urlParts.ip.empty() || urlParts.port == 0) {
			cout << "[DEBUG] Error parseando URL, saltando\n";
			continue;
		}
		cout << "[DEBUG] URL: " << urlParts.ip << ":" << urlParts.port << urlParts.path << "\n";
		
		cout << "[DEBUG] Obteniendo servicios UPnP...\n";
		string xmlContent = recieveUPnPServices(urlParts);

		if (xmlContent.empty()) {
			cout << "[DEBUG] No se pudo obtener XML, probando siguiente\n";
			continue;
		}
		cout << "[DEBUG] XML recibido (" << xmlContent.size() << " bytes)\n";

		cout << "[DEBUG] Extrayendo servicios WAN...\n";
		UPnPService service = extractUPnPService(xmlContent);

		if (service.serviceType.empty()) {
			cout << "[DEBUG] No tiene servicio WAN, probando siguiente\n";
			continue;
		}

		cout << "\n[DEBUG] ===== ¡Router UPnP válido encontrado! =====\n";
		UPnPRouter router;
		router.ip = urlParts.ip;
		router.port = urlParts.port;
		router.controlURL = service.controlURL;
		router.SCPDURL = service.SCPDURL;
		router.serviceType = service.serviceType;

		cout << "  IP: " << router.ip << "\n";
		cout << "  Puerto: " << router.port << "\n";
		cout << "  Control URL: " << router.controlURL << "\n";
		cout << "  Service Type: " << router.serviceType << "\n";
		cout << "[DEBUG] ========================================\n";

		validRouters.push_back(router);  // Agregar al vector en lugar de retornar
	}
	
	cout << "\n[DEBUG] ===== Total de routers UPnP válidos encontrados: " << validRouters.size() << " =====\n";
	
	if (validRouters.empty()) {
		cerr << "[ERROR] Ninguna respuesta SSDP contenía un router UPnP válido\n";
	}
	
	return validRouters;
}

string recieveUPnPServices(UPnPURL &urlParts) {
	cout << "[DEBUG]   -> Creando socket TCP...\n";
	int fd = socket(IPv4, STREAM, 0);

	if (fd < 0) {
		cerr << "[ERROR]   -> No se pudo crear socket TCP: " << strerror(errno) << "\n";
		return "";
	}

	// Timeout de 1 segundo para connect y recv
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
	cout << "[DEBUG]   -> Timeout de 1 segundo configurado\n";

	struct sockaddr_in routerAddress;
	routerAddress.sin_family = IPv4;
	routerAddress.sin_port = htons(urlParts.port);
	routerAddress.sin_addr.s_addr = inet_addr(urlParts.ip.c_str());

	cout << "[DEBUG]   -> Conectando a " << urlParts.ip << ":" << urlParts.port << "...\n";
	if (connect(fd, (sockaddr *)&routerAddress, sizeof(routerAddress)) < 0) {
		cerr << "[ERROR]   -> Error conectando: " << strerror(errno) << "\n";
		close(fd);
		return "";
	}
	cout << "[DEBUG]   -> Conexión establecida\n";

	string httpRequest = "GET " + urlParts.path + " HTTP/1.1\r\n";
	httpRequest += "Host: " + urlParts.ip + ":" + to_string(urlParts.port) + "\r\n";
	httpRequest += "Connection: close\r\n";
	httpRequest += "\r\n";

	cout << "[DEBUG]   -> Enviando HTTP GET...\n";
	if (send(fd, httpRequest.c_str(), httpRequest.size(), 0) < 0) {
		cerr << "[ERROR]   -> Error enviando: " << strerror(errno) << "\n";
		close(fd);
		return "";
	}
	cout << "[DEBUG]   -> Esperando respuesta...\n";

	char responseBuffer[4096];
	string response;
	int bytes;

	while ((bytes = recv(fd, responseBuffer, sizeof(responseBuffer), 0)) > 0) {
		response.append(responseBuffer, bytes);
	}
	
	close(fd);
	cout << "[DEBUG]   -> Respuesta recibida (" << response.size() << " bytes)\n";

	cout << "[DEBUG]   -> ========================================\n";
	cout << "[DEBUG]   -> Respuesta del dispositivo:\n";
	cout << "[DEBUG]   -> ========================================\n";
	cout << response << "\n";
	cout << "[DEBUG]   -> ========================================\n";

	return response;
}

UPnPURL parseURL(string &stringURL) {
	size_t location_pos = stringURL.find("LOCATION:");
	size_t url_start = stringURL.find("http", location_pos);
	size_t url_end = stringURL.find("\r\n", url_start);

	stringURL = stringURL.substr(url_start, url_end - url_start);
	stringURL.erase(0, stringURL.find_first_not_of(" \t\r\n"));
	stringURL.erase(stringURL.find_last_not_of(" \t\r\n") + 1);

	size_t protocol_end = stringURL.find("://");
	size_t ip_start = protocol_end + 3;
	size_t port_start = stringURL.find(":", ip_start);
	size_t path_start = stringURL.find("/", port_start);

	UPnPURL url;
	url.ip = stringURL.substr(ip_start, port_start - ip_start);
	url.port = stoi(stringURL.substr(port_start + 1, path_start - port_start - 1));
	url.path = stringURL.substr(path_start);

	cout << "[DEBUG]   Parseado -> IP: " << url.ip << " Puerto: " << url.port << " Path: " << url.path << "\n";

	return url;
}

UPnPService extractUPnPService(string xmlContent) {
	UPnPService service;

	cout << "[DEBUG]   -> Buscando WANIPConnection:1...\n";
	size_t pos = xmlContent.find("WANIPConnection:1");
	if (pos != string::npos) {
		cout << "[DEBUG]   -> WANIPConnection:1 encontrado\n";
		service.serviceType = "WANIPConnection:1";
	}
	else if ((pos = xmlContent.find("WANIPConnection:2")) != string::npos) {
		cout << "[DEBUG]   -> WANIPConnection:2 encontrado\n";
		service.serviceType = "WANIPConnection:2";
	}
	else if ((pos = xmlContent.find("WANPPPConnection:1")) != string::npos) {
		cout << "[DEBUG]   -> WANPPPConnection:1 encontrado\n";
		service.serviceType = "WANPPPConnection:1";
	} else {
		cout << "[DEBUG]   -> No se encontró servicio WAN\n";
		return service;
	}

	cout << "[DEBUG]   -> Extrayendo controlURL...\n";
	size_t ctrl_start = xmlContent.find("<controlURL>", pos);
	size_t ctrl_end = xmlContent.find("</controlURL>", ctrl_start);
	service.controlURL = xmlContent.substr(ctrl_start + 12, ctrl_end - ctrl_start - 12);
	cout << "[DEBUG]   -> controlURL: " << service.controlURL << "\n";

	cout << "[DEBUG]   -> Extrayendo SCPDURL...\n";
	size_t scpd_start = xmlContent.find("<SCPDURL>", pos);
	size_t scpd_end = xmlContent.find("</SCPDURL>", scpd_start);
	service.SCPDURL = xmlContent.substr(scpd_start + 9, scpd_end - scpd_start - 9);
	cout << "[DEBUG]   -> SCPDURL: " << service.SCPDURL << "\n";

	return service;
}

int connectUPnP(int serverSocket, int port) {
	cout << "[DEBUG] ========== INICIO connectUPnP ==========\n";
	
	int fd = createSearchSocket();
	if (fd < 0) {
		cerr << "[ERROR] Fallo creando socket\n";
		return -1;
	}
	
	askRouterUPnPURL(fd);
	vector<UPnPRouter> routers = findAllValidUPnPRouters(fd);
	
	close(fd);
	cout << "[DEBUG] ========== FIN connectUPnP ==========\n";
	return 0;
}