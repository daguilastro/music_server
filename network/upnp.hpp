#pragma once

#include <string>
#include <vector>

using namespace std;

// ===== ESTRUCTURAS =====

struct UPnPURL {
    string ip;
    int port;
    string path;
};

struct UPnPService {
    string serviceType;
    string controlURL;
    string SCPDURL;
};

struct UPnPRouter {
    string ip;
    int port;
    string controlURL;
    string SCPDURL;
    string serviceType;
};

// ===== FUNCIONES PÚBLICAS =====

int connectUPnP(int serverSocket, int port, UPnPRouter& usedRouter);
int connectLocal(int serverSocket);
bool closeRouterPort(UPnPRouter& router, int externalPort);

// ===== FUNCIONES INTERNAS (usadas internamente por upnp.cpp) =====

int createSearchSocket();
void askRouterUPnPURL(int searchSocket);
vector<UPnPRouter> findAllValidUPnPRouters(int searchSocket);
bool tryOpenPortWithRouter(UPnPRouter& router, int serverSocket, int externalPort);
string getExternalIPAddress(UPnPRouter& router);
bool addPortMapping(UPnPRouter& router, int externalPort, int internalPort, string internalIP, string protocol, string description);
string sendSOAPRequest(UPnPRouter& router, string soapAction, string soapBody);
UPnPURL parseURL(string& stringURL);
string recieveUPnPServices(UPnPURL& urlParts);
UPnPService extractUPnPService(string xmlContent);