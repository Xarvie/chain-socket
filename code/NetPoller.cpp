#include "NetPoller.h"
#include "Timer.h"
#include "SelectPoller.h"
#include <iostream>
#include "NetStruct.h"
#include "SystemInterface.hpp"

#include "Queue.h"
#include "Log.h"

ConnectPoller::ConnectPoller() {


    tm = new(malloc(sizeof(TimerManager))) TimerManager();
    connectTimer = new(malloc(sizeof(Timer))) Timer(*tm);
    connectTimer->data = (char *) "bbb";

    //connectPoller = new(malloc(sizeof(SelectPoller))) SelectPoller(1023);
    resultMap.clear();
}

ConnectPoller::~ConnectPoller() {

}

int ConnectPoller::GetSocketStatus(int connectFd) {
    int error = 0;
    int ret = send(connectFd, NULL, 0, 0);
    if (ret == -1) {
        error = IsCONNECTERR();
        if (error == 0)
            return -1; // conn pending
        return error;
    } else {
        return 0;
    }
    return -1;
}

void ConnectPoller::checkAllStatus() {
    auto &eventMap = this->resultMap;
    auto MSTick = TimerManager::GetCurrentMillisecs();
    for (auto it = this->connectTime.begin(); it != this->connectTime.end();) {
        const uint64_t connectFd = it->first;
        const std::tuple<unsigned long long, int, std::string, int> &tuplex = it->second;
        const unsigned long long limitTime = std::get<0>(tuplex);
        int curTime = std::get<1>(tuplex);
        std::string ip = std::get<2>(tuplex);
        int port = std::get<3>(tuplex);

        if (MSTick > limitTime) {
            closeSocket(connectFd);
            it = this->connectTime.erase(it);
            //this->connectPoller->pollDel(connectFd);
            sockInfo sockx;
            sockx.fd = connectFd;
            sockx.ip = ip;
            sockx.port = port;
            sockx.event = CONNECT_EVENT;
            sockx.ret = -1;
            eventMap[connectFd] = sockx;
            //this->logicTaskQueue.enqueue(sockx);
            std::cout << "on connect error time out" << std::endl;
            continue;
        }
        int rt = GetSocketStatus(connectFd);

        if (rt == -1) {
            //DONOTHING
        } else if (rt == 0) {
            it = this->connectTime.erase(it);
            //this->connectPoller->pollDel(connectFd);
            sockInfo sockx;
            sockx.fd = connectFd;
            sockx.ip = ip;
            sockx.port = port;
            sockx.event = CONNECT_EVENT;
            sockx.ret = 1;
            eventMap[connectFd] = sockx;
            continue;
        } else {
            closeSocket(connectFd);
            it = this->connectTime.erase(it);
            //this->connectPoller->pollDel(connectFd);
            sockInfo sockx;
            sockx.fd = connectFd;
            sockx.ip = ip;
            sockx.port = port;
            sockx.event = CONNECT_EVENT;
            sockx.ret = -1;
            eventMap[connectFd] = sockx;
            continue;
        }
        it++;
    }


    if (this->connectTime.empty()) {
        this->connectTimer->stop();
    }

    return;
}

int ConnectPoller::onTimerCheckConnect(int interval) {
    this->checkAllStatus();
    return 0;
}

std::map<uint64_t, sockInfo> ConnectPoller::getResult() {
    this->resultMap.clear();
    this->tm->DetectTimers();
    return this->resultMap;
}

int ConnectPoller::tryConnect(const char *ip, const int port, const int timeout, uint64_t *sockFd) {
    int64_t connectFd = socket(AF_INET, SOCK_STREAM, 0);

    int nRcvBufferLen = 32 * 1024 * 1024;
    int nSndBufferLen = 32 * 1024 * 1024;
    int nLen = sizeof(int);

    setSockOpt(connectFd, SOL_SOCKET, SO_SNDBUF, &nSndBufferLen, nLen);
    setSockOpt(connectFd, SOL_SOCKET, SO_RCVBUF, &nRcvBufferLen, nLen);

    if (timeout > 0)
        setSockNonBlock(connectFd);

    struct sockaddr_in targetAddr;
    targetAddr.sin_family = AF_INET;
    targetAddr.sin_port = htons(port);
    targetAddr.sin_addr.s_addr = inet_addr(ip);


    int ret = connect(connectFd, (struct sockaddr *) &targetAddr, sizeof(targetAddr));
    if (ret < 0) {
        /* if connect error */

        if (!IsConnecting()) {
            std::cout << getSockError() << std::endl;
            std::cout << "\nConnecting Failed! " << getSockError() << std::endl;
            closeSocket(connectFd);
            return -1;
        } else {
            if (sockFd) {
                *sockFd = connectFd;
            }
            return 0;
        }
    } else {

    }
    if (timeout == 0)
        setSockNonBlock(connectFd);
    if (sockFd) {
        *sockFd = connectFd;
    }
    return 1;
}

std::pair<int, uint64_t> ConnectPoller::connectTo(const char *ip, const int port, const int timeout) {
    uint64_t connectSocket = 0;
    int ret = tryConnect(ip, port, timeout, &connectSocket);

    if (ret == 0) {
        // PENDING
        bool empty = this->connectTime.empty();
        auto msTick = TimerManager::GetCurrentMillisecs();
        this->connectTime[connectSocket] = std::tuple<unsigned long long, int, std::string, int>(msTick + timeout, 0,
                                                                                                 ip, port);
        //connectPoller->pollAdd(connectSocket, EVENT_WRITE);
        if (empty) {
            int inv = 10;
            connectTimer->Start([this, inv](void *data) {
                this->onTimerCheckConnect(inv);
            }, inv, Timer::TimerType::CIRCLE);
        }
    }
    return std::pair<int, uint64_t>(ret, connectSocket);
}

int MsgHub::connectTo(const char *ip, int port, int timeout) {
    std::pair<int, uint64_t> r = this->connectPoller.connectTo(ip, port, timeout);
    int ret = r.first;
    uint64_t connectSocket = r.second;
    if (ret == 1) {
        // OK
        auto newSession = this->initNewSession(connectSocket, Session::Type::CONNECT);
        if (newSession == NULL) {
            this->logger->output("Connect Session exhausted", 0);
            this->onConnectFailed(ip, port, -1);
            return -1;
        }
        this->onConnect(newSession, ip, port);
    } else if (ret < 0) {
        this->onConnectFailed(ip, port, -1);
        // ERR
    } else {

    }
    return 0;
}

int MsgHub::processConnectSockets(std::map<uint64_t, sockInfo> &eventMap) {
    for (auto &E:eventMap) {
        if (E.second.ret == 1) {

            auto newSession = this->initNewSession(E.first, Session::Type::CONNECT);
            if (newSession == NULL) {
                this->logger->output("Connect Session exhausted", 0);
                this->onConnectFailed(E.second.ip, E.second.port, E.second.ret);
                continue;
            }
            this->onConnect(newSession, E.second.ip, E.second.port);

        } else {
            this->onConnectFailed(E.second.ip, E.second.port, E.second.ret);
        }
    }
    return 0;
}

MsgHub::MsgHub() {
    q = new moodycamel::ConcurrentQueue<ConnectionTask, moodycamel::ConcurrentQueueDefaultTraits>;

}

MsgHub::~MsgHub() {
    delete q;
}

int MsgHub::initMsgHub() {
    return 0;
}

int MsgHub::onTask() {
    ConnectionTask task;
    if (q->try_dequeue(task)) {
        if ((Session*)this->sessions[task.sessionId] == task.sessPtr) {
            if (task.type == 1) {
                if (task.len > sizeof(SendTask)) {
                    SendTask *t = (SendTask *) task.data;
                    this->sendTo(task.sessPtr, t->data, t->len);
                }

            }
            if (task.type == 2) {
                if (task.len > sizeof(CloseTask)) {
                    CloseTask *t = (CloseTask *) task.data;
                    this->closeSession(task.sessionId, t->reason);
                }
            }
            if (task.type == 3) {
                if (task.len > sizeof(ConnectTask)) {
                    ConnectTask *addr = (ConnectTask *) task.data;
                    this->connectTo((char *) task.data, addr->port, addr->timeOut);
                }

            }
            if (task.type == 0) {
                std::abort();
            }
        }
    } else {

    }
    return 0;
}

int MsgHub::waitMsg() {


    std::map<uint64_t, sockInfo> rt = this->connectPoller.getResult();
    this->processConnectSockets(rt);
    int leftTick = 10;

    this->Poller::loopOnce(leftTick);


    return 0;
}

int MsgHub::onConnect(Session *connp, std::string ip, int port) {
    if (connp == NULL)
        std::abort();
    connp->heartBeats = HEARTBEATS_COUNT;
    return 0;
}

int MsgHub::onConnectFailed(std::string ip, int port, int type) {
    std::cout << "onConnectFailed" << std::endl;
    return 0;
}

int MsgHub::onAccept(Session *lconnp, Session *aconnp) {
    this->Poller::onAccept(lconnp, aconnp);
    this->postRecv(aconnp);
    return 0;
}

int MsgHub::onRecv(Session *conn, int len) {
    std::cout << " MsgHub::onRecv" << std::endl;
    return 0;
}

int MsgHub::onSend(Session *conn, int len) {
    return 0;
}

int MsgHub::genTask(ConnectionTask *task) {
    this->q->enqueue(*task);
    return 0;
}