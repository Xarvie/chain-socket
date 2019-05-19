
#include "SystemReader.h"

#if defined(SELECT_SERVER)
#ifndef SERVER_SELECTSERVER_H
#define SERVER_SELECTSERVER_H

#include "NetStruct.h"
#include "Buffer.h"
#include <functional>
#include <mutex>

class TimerManager;

class Worker {
public:
    std::set<Session *> onlineSessionSet;
    TimerManager *tm;
    std::mutex lock;
    std::vector<std::pair<Session*, int>> evVec;
    int index;
};

class Poller {
public:
    Poller(int port, int threadsNum);

    virtual ~Poller();

    virtual int onAccept(Session &conn, const Addr &addr) { return 0; }

    virtual int onReadMsg(Session &conn, int bytesNum) { return bytesNum; }

    virtual int onWriteBytes(Session &conn, int len) { return 0; }

    virtual int onDisconnect(Session &conn, int type) { return 0; }

    virtual int onIdle(int pollIndex) { return 0; }

    virtual int onInit(int pollIndex) { return 0; }

    virtual int onTimerEvent(int interval) {return 0; }

    inline TimerManager &getUserTimerManager(Session &conn) {
#if defined(OS_WINDOWS)
        int id = (conn.sessionId / 4) % this->maxWorker;
        return *workerVec[id]->tm;
#else
        int id = conn.sessionId % this->maxWorker;
        return *workerVec[id]->tm;
#endif
    }

//    inline TimerManager &getPollerTimer() {
//
//    }

    typedef std::function<void(int)> TimerCallBack;

    int onTimerCallback(int poller);


    int sendMsg(Session &conn, const Msg &msg);

    int run();

    int stop();

    void closeSession(Session &conn, int type);

protected:

    int createTimerEvent(int inv);

    int handleReadEvent(Session &conn);

    int handleWriteEvent(Session &conn);

    void workerThreadCB(int index);

    void logicWorkerThreadCB();

    void listenThreadCB();

    bool createListenSocket(int port);

    std::vector<Session *> sessions;
    int maxWorker = 0;
    uint64_t listenSocket = 0;
    int port = 0;
    volatile bool isRunning = false;
    std::vector<std::thread> workThreads;
    std::thread listenThread;
    std::vector<moodycamel::ConcurrentQueue<sockInfo>> taskQueue;
    std::vector<std::set<uint64_t>> clients;
    std::vector<std::set<uint64_t>> acceptClientFds;
    std::vector<Worker *> workerVec;
    std::thread heartBeatsThread;

    TimerManager *tm;
    std::thread logicWorker;
    moodycamel::ConcurrentQueue<sockInfo> logicTaskQueue;
};

#endif //SERVER_SELECTSERVER_H
#endif