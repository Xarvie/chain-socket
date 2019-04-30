#ifndef SERVER_NETSTRUCT_H
#define SERVER_NETSTRUCT_H

#include "SystemReader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <time.h>
#include <vector>
#include <iostream>
#include <string>
#include <list>
#include <thread>
#include <mutex>
#include <utility>
#include <map>
#include <set>

#if defined(OS_WINDOWS)
#include <mswsock.h>
#include <ws2tcpip.h>
#else
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

#include "Queue.h"


#define CONN_MAXFD 65535

enum xx {
    BUFFER_SIZE = 8192
};

#define xmalloc malloc
#define xfree free

struct sockInfo
{
    //TODO move construct
    int port;
    char ip[128];
    int fd;
    int ret;
    char task;/*1:listen 2:connect 3:disconnect*/
    char event;/*1 listen 2:connect*/
};

enum RWMOD {
    ClientIoAccept,
    ClientIoConnect,
    ClientIoRead,
    ClientIoWrite
};

struct Addr {
    std::string ip;
    std::string port;
    int type;
};

struct Msg {
    int len;
    unsigned char *buff;
};

enum {
    ACCEPT_EVENT,
    RW_EVENT
};
enum {
    REQ_DISCONNECT,
    REQ_SHUTDOWN,
    REQ_CONNECT
};


#endif //SERVER_NETSTRUCT_H
