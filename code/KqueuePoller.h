#include "SystemReader.h"

#if defined(KQUEUE_BACKEND)

#ifndef KQUEUEPOLLER_H
#define KQUEUEPOLLER_H

struct kevent;

class KqueuePoller {
public:
    KqueuePoller(int maxEvent);

    ~KqueuePoller();

    int disableWrite(int sockFd);

    int pollMod(int sockFd, int flag/*EVENT_READ | EVENT_WRITE | EVENT_ONCE*/, void *ptr);

    int pollAdd(int sockFd, int flag/*EVENT_READ | EVENT_WRITE | EVENT_ONCE*/, void *ptr);

    int pollDel(int sockFd);

    int pollGetEvents(int time, int maxEvent);

    void *getEventConn(int index);

    int getEventTrigerFlag(int index);

public:
    int pollId = 0;
    struct kevent *event_set;
    struct kevent *event_list;
    int maxEvent;
};

#endif /* KQUEUEPOLLER_H */
#endif
