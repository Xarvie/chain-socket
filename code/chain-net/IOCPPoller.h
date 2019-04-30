#include "SystemReader.h"
#if defined(OS_WINDOWS) && !defined(SELECT_SERVER)

#ifndef IOCPSERVER_H
#define IOCPSERVER_H

#include "NetStruct.h"
#include "Buffer.h"

// TODO close all client
struct PER_SOCKET_CONTEXT {
    WSAOVERLAPPED Overlapped;
    char Buffer[BUFFER_SIZE];
    WSABUF wsabuf;
    int nTotalBytes;
    int nSentBytes;
    RWMOD IOOperation;
    SOCKET SocketAccept;
    uint64_t sessionId;
};

class Session {
public:
    uint64_t sessionId = 0;
    int64_t preHeartBeats = 0;
    MessageBuffer readBuffer;
    MessageBuffer writeBuffer;
    PER_SOCKET_CONTEXT* iocp_context = NULL;
    PER_SOCKET_CONTEXT* iocp_write_context = NULL;
    void reset()
    {
        sessionId = 0;
        preHeartBeats = 0;
        readBuffer.reset();
        writeBuffer.reset();
        if(iocp_context == NULL)
            iocp_context = (PER_SOCKET_CONTEXT*)xmalloc(sizeof(PER_SOCKET_CONTEXT));
        memset(iocp_context, 0, sizeof(PER_SOCKET_CONTEXT));

        if(iocp_write_context == NULL)
            iocp_write_context = (PER_SOCKET_CONTEXT*)xmalloc(sizeof(PER_SOCKET_CONTEXT));
        memset(iocp_write_context, 0, sizeof(PER_SOCKET_CONTEXT));

    }
};


class Poller {
public:
    Poller(int port, int threadsNum);
    virtual int onAccept(Session &conn, const Addr &addr) = 0;
    virtual int onReadMsg(Session &conn, const Msg &msg) = 0;
    virtual int onWriteBytes(Session &conn, int len) = 0;
    int continueSendMsg(uint64_t sessionId);
    void sendMsg(uint64_t sessionId, const Msg &msg);
    bool createListenSocket(int port);
    int connect(std::string ip, std::string port);

    void workerThreadCB(int pollIndex);
    void listenThreadCB();
    PER_SOCKET_CONTEXT* UpdateCompletionPort(int workerId, SOCKET s, RWMOD ClientIo, BOOL bAddToList);
    int closeSession(uint64_t sessionId);

    void CloseClient(PER_SOCKET_CONTEXT* lpPerSocketContext, BOOL bGraceful);

    PER_SOCKET_CONTEXT* CtxtAllocate(SOCKET s, RWMOD ClientIO);

    int run(int port);

    volatile bool isRunning = false;
    std::vector<HANDLE> iocps;
    SOCKET g_sdListen = INVALID_SOCKET;
    int maxWorker = 4;
    std::thread listenThread;
    std::vector<std::list<Session*>*> onlineSessionLists;
    std::vector<std::thread> workThreads;
    std::vector<Session*> sessions;
    std::vector<moodycamel::ConcurrentQueue<int>> taskQueue;
    std::recursive_mutex *xxx = NULL;

};

#endif
#endif