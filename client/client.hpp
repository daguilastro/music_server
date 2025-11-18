#pragma once


#include <cstddef>
#include <cstdint>
#include <sys/types.h>

struct ClientState{
    int fd;

    uint8_t inputBuffer[1024 * 512];
    size_t inputBufferUsed = 0;

    uint8_t currentSearchId = 0;
    uint8_t currentAudioId = 0;
};