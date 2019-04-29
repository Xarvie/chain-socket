#ifndef SERVER_1SOCKETINTERFACE_H
#define SERVER_1SOCKETINTERFACE_H
#include <string>
#include <iostream>

#if defined(OS_WINDOWS)
#include <ws2tcpip.h>
#include <winsock2.h>
#else
#include <unistd.h>
#include <stdlib.h>
#endif
#include <cstdint>
#include "SystemReader.h"

static int closeSocket(uint64_t fd) {
#ifdef OS_WINDOWS
    return closesocket(fd);
#else
    return close((int)fd);
#endif
}

static int getSockError() {
#ifdef OS_WINDOWS
    return WSAGetLastError();
#else
    return errno;
#endif
}

static int IsEagain() {
    int err = getSockError();
#if defined(OS_WINDOWS)
    if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK || err == WSAEWOULDBLOCK)
        return 1;
#endif
    if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK)
        return 1;
    return 0;
}


#endif //SERVER_SOCKETINTERFACE_H
