#include "SystemReader.h"

#if !defined(IOCP_BACKEND)
#ifndef MAIN_POLLER_H
#define MAIN_POLLER_H


#include "Buffer.h"

#include <functional>
#include <mutex>
#include <vector>
#include <tuple>

class TimerManager;

class Log;

#if 0
#elif defined(EPOLL_BACKEND)
#include "EpollPoller.h"
typedef EpollPoller SystemPoller;
#elif defined(SELECT_BACKEND)

#include "SelectPoller.h"

typedef SelectPoller SystemPoller;
#elif defined(KQUEUE_BACKEND)

#include "KqueuePoller.h"

typedef KqueuePoller SystemPoller;
#endif


class Poller {
public:
    Poller();

    virtual int onAccept(Session *lconnp, Session *aconnp);

    virtual int onRecv(Session *conn, int len);

    virtual int onSend(Session *conn, int len);

    virtual int onConnect(int connId, std::string ip, int port, int type);

    int sendTo(Session *conn, unsigned char *buf, int len);

    int postRecv(Session *connp);

    int closeSession(int connId, int type);

    bool listenTo(int port);

    int loopOnce(int time);

    int init(Log *logger = NULL);

    int stop();

    int getPeerIpPort(int connId, std::string x, int port);

    int getLocalIpPort(int connId, std::string x, int port);

    inline Session *getSession(int connid) {
        return this->sessions[connid];
    }

    Session *session(int connId);

protected:
    int handleReadEvent(Session *connp);

    int postSend(Session *conn);

    int createListenSocket(int port);

    Session *initNewSession(uint64_t fd, Session::Type type);

public:
    std::vector<int> listenFds;
    std::vector<Session *> listenConns;
    int maxWorker;
    std::vector<Session *> sessions;

    Log *logger;
    SystemPoller *poller;
    bool isRun;

    int curId;
    int maxId;
};

#endif
#endif //MAIN_POLLER_H
