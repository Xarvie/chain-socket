
#include "SystemReader.h"

#if defined(SELECT_SERVER)
#ifndef SERVER_SELECTSERVER_H
#define SERVER_SELECTSERVER_H

#include "NetStruct.h"
#include "Buffer.h"

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
    int handleReadEvent(Session &conn);

    int handleWriteEvent(Session &conn);

    void workerThreadCB(int index);

    void listenThreadCB();

    bool createListenSocket(int port);


    enum {
        ACCEPT_EVENT,
        RW_EVENT
    };
    enum {
        REQ_DISCONNECT,
        REQ_SHUTDOWN,
        REQ_CONNECT
    };
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
};

#endif //SERVER_SELECTSERVER_H
#endif