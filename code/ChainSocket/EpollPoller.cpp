#include "SystemReader.h"

#if defined(OS_LINUX) && !defined(SELECT_SERVER)

#include "EpollPoller.h"

#include <sys/epoll.h>

#include <stdlib.h>
#include <unistd.h>

#define xmalloc malloc
#define xfree free

inline int getPollIndex(int id, int maxNum) {
#ifdef OS_WINDOWS
    int index = id / 4 % maxNum;
#else
    int index = id % maxNum;
#endif
    return index;
}

EpollPoller::EpollPoller(int maxEvent) : maxEvent(maxEvent) {
    pollId = epoll_create(1);
    ev1 = (struct epoll_event *)malloc(sizeof(struct epoll_event));
    events = (struct epoll_event *) xmalloc(maxEvent * sizeof(struct epoll_event));
}

EpollPoller::~EpollPoller() {
    close(pollId);
    xfree(ev1);
    xfree(events);
}

int EpollPoller::pollMod(int sockFd, int flag/*EVENT_READ | EVENT_WRITE | EVENT_ONCE*/, void* ptr) {
    //ev1->data.fd = sockFd;
    ev1->data.ptr = ptr;
    ev1->events = flag;
    epoll_ctl(pollId, EPOLL_CTL_MOD, sockFd, ev1);
    return 0;
}

int EpollPoller::pollAdd(int sockFd, int flag/*EVENT_READ | EVENT_WRITE | EVENT_ONCE*/, void* ptr) {
    //ev1->data.fd = sockFd;
    ev1->data.ptr = ptr;
    ev1->events = flag;

    return epoll_ctl(pollId, EPOLL_CTL_ADD, sockFd, ev1);
}

int EpollPoller::pollDel(int sockFd) {
    return 0;
}

int EpollPoller::pollGetEvents(int time, int maxEvent) {
    int numEvents = epoll_wait(pollId, events, maxEvent, time);
    return numEvents;
}

void* EpollPoller::getEventConn(int index) {
    return events[index].data.ptr;
}


int EpollPoller::getEventTrigerFlag(int index) {
    return events[index].events;
}


#endif /* SERVER_EPOLLPOLL_H_ */