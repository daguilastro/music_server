#include "socket_utilities.hpp"
#include <iterator>
#include <netinet/in.h>
#include <sys/socket.h>


int createSearchSocket(){
    int fd = socket(IPv4, DGRAM, 0);

    struct sockaddr_in localAddress;
    localAddress.sin_family = IPv4;
    localAddress.sin_addr.s_addr = htonl(RECIEVE_EVERYWHERE);
    localAddress.sin_port = htons(0);  // Puerto 0 = SO elige
    
    if (bind(fd, (struct sockaddr *)&localAddress, sizeof(localAddress)) < 0) {
        cerr << "Error en bind: " << strerror(errno) << "\n";
        close(fd);
    }
    
return fd;
}

void askRouterUPnPURL(int searchSocket){

    const char* ssdpRequest = 
    "M-SEARCH * HTTP/1.1\r\n"
    "HOST: 238.255.255.250:1900\r\n"
    "MAN: \"ssdp:discover\"\r\n"
    "MX: 2\r\n"
    "ST: upnp:rootdevice\r\n"
    "\r\n";

    struct sockaddr_in multicastAddress;
    multicastAddress.sin_family = IPv4;
    multicastAddress.sin_addr.s_addr = inet_addr("239.255.255.250"); // Grupo multicast donde se unen los dispositivos UPnP
    multicastAddress.sin_port = htons(1900);

    if (sendto(searchSocket, ssdpRequest, strlen(ssdpRequest), 0, (sockaddr*) &multicastAddress, sizeof(multicastAddress)) < 0){
        cerr << "Error enviando SSDP: " << strerror(errno) << "\n";
        close(searchSocket);
    }
}

string recieveRouterUPnPURL(int searchSocket){
    char responseBuffer[2048];
    struct sockaddr_in responseAddress;
    socklen_t addressLength = sizeof(responseAddress);
    int n = recvfrom(searchSocket, responseBuffer, sizeof(responseBuffer) - 1, 0,(struct sockaddr *)&responseAddress, &addressLength);

    if (n < 0) {
        cerr << "Error recibiendo: " << strerror(errno) << "\n";
        close(searchSocket);
    }

    responseBuffer[n] = '\0';
    cout << "Respuesta de " << inet_ntoa(responseAddress.sin_addr) << "\n";
    cout << responseBuffer << "\n";

    return (string) responseBuffer;
}

string recieveUPnPServices(string &url){
    UPnPURL urlParts = parseURL(url);
    int fd = socket(IPv4, STREAM, 0);

    struct sockaddr_in routerAddress;
    
}

UPnPURL parseURL(string &stringURL){
    size_t protocol_end = stringURL.find("://");
    size_t ip_start = protocol_end + 3;
    size_t port_start = stringURL.find(":", ip_start);
    size_t path_start = stringURL.find("/", port_start);

    UPnPURL url;
    
    url.ip = stringURL.substr(ip_start, port_start - ip_start);
    url.port = stoi(stringURL.substr(port_start + 1, path_start - port_start - 1));
    url.path = stringURL.substr(path_start);

    return url;
}

int connectUPnP(){
    int fd = createSearchSocket();
    askRouterUPnPURL(fd);
    string response = recieveRouterUPnPURL(fd);
    return 0;
}
