#ifndef CHAIN_SOCKET_NETPOLLER_H
#define CHAIN_SOCKET_NETPOLLER_H

#include "SystemReader.h"

namespace moodycamel {
    class ConcurrentQueueDefaultTraits;

    template<typename T, typename Traits>
    class ConcurrentQueue;
}
#if 0
#elif defined(OS_WINDOWS)
#if defined(SELECT_BACKEND)

#include "Poller.h"

#else
#include "IOCPPoller.h"
#endif
#elif defined(OS_LINUX)
#include "Poller.h"
#elif defined(OS_DARWIN)

#include "Poller.h"

#else
#include "Poller.h"
#endif

#include <map>

class TimerManager;

class Timer;

class SelectPoller;

#include "socketInfo.h"

struct ConnectionTask {
    Session *sessPtr;
    int sessionId;
    int type;
    int len;
    void *data;
};

struct SendTask {
    unsigned char *data;
    int len;
};

struct CloseTask {
    int sessionId;
    int reason;
};

struct ConnectTask {
    char addr[128];
    int port;
    int timeOut;
};

class ConnectPoller {
public:
    ConnectPoller();

    ~ConnectPoller();

    int GetSocketStatus(int fd);

    void checkAllStatus();

    int onTimerCheckConnect(int interval);

    std::pair<int, uint64_t> connectTo(const char *ip, const int port, const int timeout);

    int tryConnect(const char *ip, const int port, const int timeout, uint64_t *sockFd);

    std::map<uint64_t, sockInfo> getResult();

private:
    TimerManager *tm;
    Timer *connectTimer;
    std::map<uint64_t, std::tuple<unsigned long long, int, std::string, int> > connectTime;
    //SelectPoller* connectPoller;
    std::map<uint64_t, sockInfo> resultMap;
};

class MsgHub : public Poller {
public:
    MsgHub();

    ~MsgHub();

    int initMsgHub();

    int waitMsg();

    virtual int onTask();

    int processConnectSockets(std::map<uint64_t, sockInfo> &eventMap);

    int connectTo(const char *ip, int port, int timeout);

    virtual int genTask(ConnectionTask *task);

    virtual int onAccept(Session *lconnp, Session *aconnp) override;

    virtual int onRecv(Session *conn, int len) override;

    virtual int onSend(Session *conn, int len) override;

    virtual int onConnect(Session *connp, std::string ip, int port);

    virtual int onConnectFailed(std::string ip, int port, int type);

protected:
    ConnectPoller connectPoller;
    moodycamel::ConcurrentQueue<ConnectionTask, moodycamel::ConcurrentQueueDefaultTraits> *q;
};

#endif //CHAIN_SOCKET_NETPOLLER_H
