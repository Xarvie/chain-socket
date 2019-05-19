#include <SystemReader.h>
#ifndef SERVER_PRIVATEHEADERS_H
#define SERVER_PRIVATEHEADERS_H

#if defined(OS_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#undef FD_SETSIZE
#define FD_SETSIZE  1024
#include <ws2tcpip.h>
#include <Windows.h>
#endif

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

#endif //SERVER_PRIVATEHEADERS_H
