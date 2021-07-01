#include "SystemReader.h"

#if defined(OS_LINUX) && !defined(SELECT_SERVER)

#ifndef EPOLLPOLL_H_
#define EPOLLPOLL_H_

struct epoll_event;
class EpollPoller {
public:
    EpollPoller(int maxEvent);

    ~EpollPoller();

    int pollMod(int sockFd, int flag/*EVENT_READ | EVENT_WRITE | EVENT_ONCE*/, void * ptr);

    int pollAdd(int sockFd, int flag/*EVENT_READ | EVENT_WRITE | EVENT_ONCE*/, void * ptr);

    int pollDel(int sockFd);

    int pollGetEvents(int time, int maxEvent);

    void* getEventConn(int index);

    int getEventTrigerFlag(int index);

public:
    int pollId = 0;
    struct epoll_event *events;
    struct epoll_event *ev1;
    int maxEvent;
};


#endif /* SERVER_EPOLLPOLL_H_ */
#endif