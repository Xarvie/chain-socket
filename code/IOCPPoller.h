#include "SystemReader.h"

#if defined(IOCP_BACKEND)
#ifndef IOCPSERVER_H
#define IOCPSERVER_H

#include <vector>
#include <string>
#include "Buffer.h"
#include <map>
struct IocpData;
class Log;
class Poller {
public:
    Poller();

    virtual int onAccept(Session* lconnp, Session* aconnp);

    virtual int onRecv(Session* connp, int len);

    virtual int onSend(Session* connp, int len);

    virtual int onDisconnect(Session* connp, int type);

    int closeSession(int connId, int type);

    int sendTo(Session* connp, unsigned char *buf, int len);

    int postRecv(Session* connp);

    bool listenTo(int port);

    int loopOnce(int time);

    int tryConnect(const char *ip, const int port, const int timeout, uint64_t * sockFd);

    int init(Log* loggerp = NULL);

    int stop();

    int onTimerCheckConnect(int interval);

    Session *getSession(int connId);
    int postSend(Session* connp);

    int getPeerIpPort(int connId, std::string ip, int port);

    int getLocalIpPort(int connId, std::string ip, int port);

    Session* session(int connId);

protected:
    Session* initNewSession(uint64_t fd, Session::Type type);

    void reset();

    int post_accept_ex(Session* connp, uint64_t listenFd);
    int do_accept(int connIdconn);

    int do_recv(Session* connp, int len);

    int do_send(Session* connp, int len);



    bool isRun;
    std::vector<void *> iocps;
    std::vector<uint64_t> listenFds;
    int maxWorker;
    int curId;
    int maxId;
    std::vector<IocpData*> sessions;
    std::map<uint64_t , char[64]> peerAddrMap;
    Log* logger;
};

#endif
#endif