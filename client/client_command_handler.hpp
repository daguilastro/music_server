#pragma once

#include <cstdint>
#include <map>
#include <string>

using std::map;
using std::string;

// ===== ESTRUCTURA DE CALLBACK =====
struct EpollCallbackData {
    int fd;
    void (*handler)(int);
};

enum ClientCodes : uint8_t {
    CLIENT_CODE_ADD = 1,
    CLIENT_CODE_SEARCH = 2,
    CLIENT_CODE_MODIFY = 3,
    CLIENT_CODE_PLAY = 4,
    CLIENT_CODE_CLOSE = 5,
};

enum ServerCodes : uint8_t{
    SERVER_CODE_NOTIFICATION = 1,
    SERVER_CODE_AUDIO_START =2,
    SERVER_CODE_AUDIO = 3,
    SERVER_CODE_AUDIO_END = 4,
    SERVER_CODE_SEARCH_RESULT = 5,
};

#pragma pack(push, 1)
struct ServerMessage{
    uint8_t type;
    uint8_t id;
    int length;
    void* data;
};
#pragma pack (pop)

void handleServerEvent(int fd);

void handleMessageNotification(void* message);

string recieveNotification(int fd, int length);

void initializeServerMessages();