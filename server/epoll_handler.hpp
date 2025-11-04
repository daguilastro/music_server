#pragma once
#include <sys/epoll.h>

using namespace std;

struct EpollCallbackData {
    int fd;
    void (*handler)(int fd, void* data);
    void* data;
};

// Funciones de epoll
int createEpoll();
int addToEpoll(int epollFd, int fd, void (*handler)(int, void*), void* data);
int removeFromEpoll(int epollFd, int fd);