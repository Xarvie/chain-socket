#include "SystemReader.h"

#if defined(OS_DARWIN) && !defined(SELECT_SERVER)

#ifndef KQUEUEPOLLER_H
#define KQUEUEPOLLER_H

#include "NetStruct.h"
#include "Buffer.h"
#include "SystemInterface.h"


class Poller {
public:
    Poller(int port, int threadsNum);

    virtual ~Poller();

    virtual int onAccept(Session &conn, const Addr &addr) { return 0; }

    virtual int onReadMsg(Session &conn, int bytesNum) { return bytesNum; }

    virtual int onWriteBytes(Session &conn, int len) { return 0; }

    int sendMsg(Session &conn, const Msg &msg);

    int run();

    void closeSession(Session &conn);

protected:
    int handleReadEvent(struct kevent *event);

    int handleWriteEvent(struct kevent *event);

    void workerThreadCB(int index);

    void listenThreadCB();

    bool createListenSocket(int port);

    int listenSocket = 0;
    int port = 0;
    int maxWorker = 0;
    volatile bool isRunning = false;

    std::vector<std::thread> workThreads;
    std::thread listenThread;
    std::vector<moodycamel::ConcurrentQueue<sockInfo> > taskQueue;
    std::vector<Session *> sessions;
    std::vector<int> queue;

    struct kevent *events = NULL;
    std::vector<struct kevent> event_set;
    std::vector<struct kevent *> event_list;



};

#endif /* KQUEUEPOLLER_H */
#endif
