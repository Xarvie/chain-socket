#ifndef SERVER_1SOCKETINTERFACE_H
#define SERVER_1SOCKETINTERFACE_H

#include "SystemReader.h"
#include <cstdint>
#include <cstdlib>
#if defined(OS_WINDOWS)
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#endif

#if defined(OS_DARWIN)
#include <sys/event.h>
#endif
#if defined(OS_LINUX)
#include <sys/epoll.h>
#endif

#if defined(OS_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#undef FD_SETSIZE
#define FD_SETSIZE  1024
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <ws2tcpip.h>
//#include <Windows.h>
#endif


inline int closeSocket(uint64_t fd) {
#ifdef OS_WINDOWS
	return closesocket(fd);
#else
	return close((int) fd);
#endif
}

inline int getSockError() {
#ifdef OS_WINDOWS
	return WSAGetLastError();
#else
	return errno;
#endif
}

inline int IsCONNECTERR() {
	int err = getSockError();
#if defined(OS_WINDOWS)
	if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK || err == WSAEWOULDBLOCK || err == WSAENOTCONN ||
		err == ENOTCONN)
		return 0;
#else
	if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK || err == ENOTCONN)
		return 0;
#endif
	return err;
}

inline int IsEagain() {
	int err = getSockError();
#if defined(OS_WINDOWS)
	if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK || err == WSAEWOULDBLOCK)
		return 1;
#else
	if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK)
		return 1;
#endif
	return 0;
}


inline int IsConnecting() {
	int err = getSockError();
#if defined(OS_WINDOWS)
	if (err == EINPROGRESS || err == WSAEWOULDBLOCK)
		return 1;
#else
	if (err == EINPROGRESS)
		return 1;
#endif
	return 0;
}

#if defined(OS_WINDOWS)

inline int setSockOpt(SOCKET s, int level, int optname, void *optval, int optlen) {
	return setsockopt(s, level, optname, (const char *) optval, optlen);
}

inline int getSockOpt(SOCKET s, int level, int optname, void *optval, int *optlen) {
	return getsockopt(s, level, optname, (char *) optval, optlen);
}

#else

inline int setSockOpt(int s, int level, int optname, void *optval, int optlen) {
	return setsockopt(s, level, optname, optval, optlen);
}

inline int getSockOpt(int s, int level, int optname, void *optval, socklen_t *optlen) {
	return getsockopt(s, level, optname, optval, optlen);
}

#endif

inline int setSockNonBlock(int fd) {
#if defined(OS_WINDOWS)
	unsigned long ul = 1;
	int ret = ioctlsocket(fd, FIONBIO, (unsigned long *) &ul);
	if (ret == SOCKET_ERROR)
		printf("err: ioctlsocket");
	return ret;
#else
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		printf("err: F_GETFL \n");
	int ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if (ret < 0)
		printf("err: F_SETFL \n");
	return ret;
#endif
}





//================ other ====================



#endif //SERVER_SOCKETINTERFACE_H
