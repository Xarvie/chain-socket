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
    HEARTBEATS_COUNT = 90,
    HEARTBEATS_INTERVAL = 5,
    MAX_EVENT = 4096
};

#define xmalloc malloc
#define xfree free


enum RWMOD {
    ClientIoNULL,
    ClientIoAccept,
    ClientIoConnect,
    ClientIoRead,
    ClientIoWrite
};

struct Addr {
    std::string ip;
    int port;
    int type;
};

struct Msg {
    int len;
    unsigned char *buff;
};

enum {
    ACCEPT_EVENT,
    CHECK_HEARTBEATS,
    RW_EVENT,
    CONNECT_EVENT
};
enum {
    CT_NORMAL,
    CT_READ_ZERO,
    CT_READ_ERROR,
    CT_WSEND_ERROR1,
    CT_WSEND_ERROR2,
    CT_READ_ERROR2,
    CT_SEND_ERROR2,
    CT_ERROR_END,
    CT_WEBSOCKET_READ_HEAD_ERROR,
    CT_WEBSOCKET_EXTENSION_ON,
    CT_WEBSOCKET_CONTINUATION_ON

};

#if defined(OS_LINUX)
enum {
    EVENT_READ = EPOLLIN,
    EVENT_WRITE = EPOLLOUT,
    EVENT_ONCE = EPOLLONESHOT
};
#else
enum {
    EVENT_READ = 0x1,
    EVENT_WRITE = 0x2,
    EVENT_ONCE = 0x4
};
#endif


#endif //SERVER_NETSTRUCT_H
