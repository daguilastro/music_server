#include "epoll_handler.hpp"
#include <iostream>
#include <unistd.h>
#include <cstring>

int createEpoll() {
    int epollFd = epoll_create1(0);
    if (epollFd < 0) {
        cerr << "[ERROR] No se pudo crear epoll\n";
        return -1;
    }
    return epollFd;
}

int addToEpoll(int epollFd, int fd, void (*handler)(int, void*), void* data) {
    EpollCallbackData* callback = new EpollCallbackData;
    callback->fd = fd;
    callback->handler = handler;
    callback->data = data;

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.ptr = callback;

    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event) != 0) {
        cerr << "[ERROR] No se pudo agregar FD " << fd << " a epoll: " 
                  << strerror(errno) << "\n";
        delete callback;
        return -1;
    }

    return 0;
}

int removeFromEpoll(int epollFd, int fd) {
    return epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr);
}