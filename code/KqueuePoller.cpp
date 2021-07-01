#include "SystemReader.h"

#if defined(KQUEUE_BACKEND)

#include "KqueuePoller.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <SystemInterface.hpp>

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

inline void PollInit() {
#ifdef OS_WINDOWS
    WORD ver = MAKEWORD(2, 2);
    WSADATA dat;
    WSAStartup(ver, &dat);
#else
    signal(SIGPIPE, SIG_IGN);
#endif
}

KqueuePoller::KqueuePoller(int maxEvent) : maxEvent(maxEvent) {
    pollId = kqueue();
    event_list = (struct kevent *) xmalloc(maxEvent * sizeof(struct kevent));
    event_set = (struct kevent *) xmalloc(sizeof(struct kevent));
}

KqueuePoller::~KqueuePoller() {
    close(pollId);
    xfree(event_list);
    xfree(event_set);
}

int KqueuePoller::disableWrite(int sockFd) {
    return 0;
}

int KqueuePoller::pollMod(int sockFd, int flag/*EVENT_READ | EVENT_WRITE | EVENT_ONCE*/, void *ptr) {
    int flag1 = EV_ADD;
    if (flag & EVENT_ONCE)
        flag1 |= EV_ONESHOT;

    if (flag & EVENT_WRITE) {
        EV_SET(event_set, sockFd, EVFILT_WRITE, flag1, 0, 0, ptr);
        if (kevent(pollId, event_set, 1, NULL, 0, NULL) == -1) {
            printf("error\n");
        }
    }

    if (flag & EVENT_READ) {
        EV_SET(event_set, sockFd, EVFILT_READ, flag1, 0, 0, ptr);
        if (kevent(pollId, event_set, 1, NULL, 0, NULL) == -1) {
            printf("error\n");
        }
    }

    return 0;
}

int KqueuePoller::pollAdd(int sockFd, int flag/*EVENT_READ | EVENT_WRITE | EVENT_ONCE*/, void *ptr) {
    int flag1 = EV_ADD;
    if (flag & EVENT_ONCE)
        flag1 |= EV_ONESHOT;
    if (flag & EVENT_WRITE) {
        EV_SET(event_set, sockFd, EVFILT_WRITE, flag1, 0, 0, ptr);
        if (kevent(pollId, event_set, 1, NULL, 0, NULL) == -1) {
            printf("error\n");
        }
    }

    if (flag & EVENT_READ) {
        EV_SET(event_set, sockFd, EVFILT_READ, flag1, 0, 0, ptr);
        if (kevent(pollId, event_set, 1, NULL, 0, NULL) == -1) {
            printf("error\n");
        }
    }

    return 0;
}

int KqueuePoller::pollDel(int sockFd) {
//    int flag1 = EV_DELETE;
//
//        EV_SET(event_set, sockFd, EVFILT_WRITE, flag1, 0, 0, 0);
//        if (kevent(pollId, event_set, 1, NULL, 0, NULL) == -1) {
//            printf("error\n");
    return 0;
}

int KqueuePoller::pollGetEvents(int time, int maxEvent) {
    struct timespec timeout;
    timeout.tv_sec = time / 1000;
    timeout.tv_nsec = time % 1000 * 1000;
    int numEvents = kevent(pollId, nullptr, 0, event_list, MAX_EVENT, &timeout);
    return numEvents;
}

void *KqueuePoller::getEventConn(int index) {
    return event_list[index].udata;
}

int KqueuePoller::getEventTrigerFlag(int index) {
    int retFlag = 0;
    if (event_list[index].flags & EVFILT_WRITE)
        retFlag |= EVENT_WRITE;
    if (event_list[index].flags & EVFILT_READ)
        retFlag |= EVENT_READ;
    return retFlag;
}

#endif /* KQUEUE_BACKEND */
