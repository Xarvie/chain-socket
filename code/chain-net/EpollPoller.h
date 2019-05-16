#include "SystemReader.h"

#if defined(OS_LINUX) && !defined(SELECT_SERVER)

#ifndef EPOLLPOLL_H_
#define EPOLLPOLL_H_

#include "NetStruct.h"
#include "Buffer.h"
#include <functional>
#include "SystemInterface.h"
#include <mutex>

class TimerManager;

class Worker {
public:
    std::set<Session *> onlineSessionSet;
    int index;

    TimerManager *tm;
    std::mutex lock;
    std::vector<std::pair<Session*, int>> evVec;
};

class Poller {
public:
    Poller(int port, int threadsNum);

    virtual ~Poller();

    virtual int onAccept(Session &conn, const Addr &addr) { return 0; }

    virtual int onReadMsg(Session &conn, int bytesNum) { return bytesNum; }

    virtual int onWriteBytes(Session &conn, int len) { return 0; }

    virtual int onDisconnect(Session &conn) { return 0; }

    virtual int onIdle(int pollIndex) { return 0; }

    virtual int onInit(int pollIndex) { return 0; }

    virtual int onTimerEvent(int interval) { std::cout << "aaa" << std::endl; return 0; }


    int sendMsg(Session &conn, const Msg &msg);

    int run();

    int stop();


    void closeSession(Session &conn);

protected:
    int createTimerEvent(int inv);

    int handleReadEvent(Session &conn);

    int handleWriteEvent(Session &conn);

    void workerThreadCB(int index);

    void listenThreadCB();

    bool createListenSocket(int port);

    void logicWorkerThreadCB();

    std::vector<Session *> sessions;

    int maxWorker = 0;
    std::vector<int> epolls;
    int listenSocket = 0;
    int port = 0;
    std::vector<std::thread> workThreads;
    std::thread listenThread;
    std::vector<moodycamel::ConcurrentQueue<sockInfo> > taskQueue;
    volatile bool isRunning = false;
    std::vector<Worker*> workerVec;
    std::thread heartBeatsThread;

    TimerManager *tm;
    std::thread logicWorker;
    moodycamel::ConcurrentQueue<sockInfo> logicTaskQueue;
};

#endif /* SERVER_EPOLLPOLL_H_ */
#endif