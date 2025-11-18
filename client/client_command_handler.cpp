#include "client_command_handler.hpp"
#include "client.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

extern map<uint8_t, void (*)(int, int)> serverMessages;
extern ClientState clientState;

void parseMessages(){
    
}

void recieveData() {
  ssize_t bytesRecieved = 0;
  while (clientState.inputBufferUsed < sizeof(clientState.inputBuffer)) {
    bytesRecieved = recv(
        clientState.fd, clientState.inputBuffer + clientState.inputBufferUsed,
        sizeof(clientState.inputBuffer) - clientState.inputBufferUsed, 0);
    if (bytesRecieved > 0) {
      clientState.inputBufferUsed += bytesRecieved;
    } else if (bytesRecieved == 0) {
      cout << "[INFO] Conexión cerrada " << endl;
      break;
    } else {
      if (errno == EAGAIN) {
        break;
      }
      close(clientState.fd);
      break;
    }
  }
  parseMessages();
}

void handleServerEvent(int fd) {}

void handleMessageNotification(int fd, int length) {
  string notification = recieveNotification(fd, length);
}

void initializeServerMessages() {
  serverMessages[SERVER_CODE_NOTIFICATION] = handleMessageNotification;
}