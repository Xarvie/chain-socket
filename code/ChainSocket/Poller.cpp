#include "Poller.h"
#include "SystemReader.h"
#include "NetStruct.h"
#include "SystemInterface.hpp"

#if !defined(IOCP_BACKEND)


#include "EpollPoller.h"
#include "SelectPoller.h"
#include "SystemInterface.hpp"
#include "Timer.h"
#include "PrivateHeaders.h"
#include "Log.h"

Poller::Poller() {
}

int Poller::init(Log *loggerp) {
    this->logger = loggerp;
    if (this->logger == NULL)
        this->logger = new Log();
#ifdef OS_WINDOWS
    WORD ver = MAKEWORD(2, 2);
    WSADATA dat;
    WSAStartup(ver, &dat);
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    this->poller = new(xmalloc(sizeof(SystemPoller))) SystemPoller(MAX_EVENT);
    this->curId = 0;
    this->maxId = 30000;
    sessions.resize(this->maxId);

    for (int i = 0; i < this->maxId; i++) {
        sessions[i] = NULL;
    }

    this->isRun = true;
    return 0;
}

int Poller::stop() {
    this->isRun = false;
    return 0;
}

int Poller::getPeerIpPort(int connId, std::string x, int port) {
    return 0;
}

int Poller::getLocalIpPort(int connId, std::string x, int port) {
    return 0;
}

int Poller::onRecv(Session *conn, int len) {
    if (conn == NULL)
        return 0;
    std::cout << "onRead" << len << std::endl;
    this->sendTo(conn, (unsigned char *) "hello", 5);
    return 0;
}

Session *Poller::session(int connId) {
    return this->sessions[connId];
}

int Poller::onSend(Session *conn, int len) {
    if (conn == NULL)
        return 0;
    std::cout << "onSend" << len << std::endl;
    return 0;
}

int Poller::onConnect(int connId, std::string ip, int port, int type) {
    if (this->sessions[connId] == NULL)
        return 0;
    std::cout << "onConnect" << ip << std::endl;
    return 0;
}

int Poller::sendTo(Session *connp, unsigned char *buf, int len) {
    Session &conn = *connp;
    if (conn.writeBuffer.size > 0) {
        conn.writeBuffer.push_back(len, buf);
        return 0;
    }
    conn.writeBuffer.push_back(len, buf);
    postSend(connp);
    return 0;
}

bool Poller::listenTo(int port) {
    int fd = this->createListenSocket(port);
    if (fd < 0)
        return -1;
    setSockNonBlock(fd);

    auto *connp = initNewSession(fd, Session::Type::LISTEN);
    if (connp == NULL) {
        this->logger->output("Listen Session exhausted", 0);
        closeSocket(fd);
        return false;
    }
    int id = connp->sessionId;
    void *x = reinterpret_cast<void *>(id);
    int ret = (*this->poller).pollAdd(fd, EVENT_READ, x);
    listenFds.push_back(fd);
    listenConns.push_back(connp);

    return true;
}

int Poller::onAccept(Session *lconnp, Session *aconnp) {
    if (lconnp == NULL)
        return 0;
    if (aconnp == NULL)
        return 0;
    Session &acceptConn = *aconnp;
    acceptConn.heartBeats = HEARTBEATS_COUNT;
    return 0;
}

int Poller::postRecv(Session *connp) {
    auto &poller = *this->poller;
    int id = connp->sessionId;
    void *x = reinterpret_cast<void *>(id);
    int ret2 = poller.pollAdd(connp->fd, EVENT_READ, x);
    return ret2;
}

int Poller::closeSession(int connId, int type) {
    if (sessions[connId] == NULL)
        return 0;
    Session *connp = sessions[connId];
    this->poller->pollDel(connp->fd);
    closeSocket(connp->fd);
    xfree(connp);
    sessions[connId] = NULL;
    return 0;
}

int Poller::loopOnce(int time) {
    auto &poll = *this->poller;
    int numEvents = poll.pollGetEvents(time, 1024);

    if (numEvents == -1) {

    } else if (numEvents > 0) {
        for (int i = 0; i < numEvents; i++) {
            void *idp = poll.getEventConn(i);
            int id = (int) reinterpret_cast<long>(idp);
            if (sessions[id] == NULL)
                continue;

            int sock = (int) sessions[id]->fd;
            int evFlag = poll.getEventTrigerFlag(i);

            switch (sessions[id]->type) {
                case Session::Type::CONNECT:
                case Session::Type::ACCEPT: {
                    int ev = EVENT_READ | EVENT_ONCE;
                    if (evFlag & EVENT_READ) {
                        int ret = 0;
                        ret = this->handleReadEvent(sessions[id]);
                        if (sessions[id] == NULL)
                            continue;
                        if (ret < 0) {
                            closeSession(id, CT_READ_ERROR2);
                            //std::cout << "read" << ret << std::endl;
                            continue;
                        }
                    }
                    if (evFlag & EVENT_WRITE) {
                        int ret = 0;
                        ret = this->postSend(sessions[id]);
                        if (sessions[id] == NULL)
                            continue;
                        if (ret < 0) {
                            closeSession(id, CT_SEND_ERROR2);
                        } else {
                            if (sessions[id]->writeBuffer.size > 0) {
                                ev |= EVENT_WRITE;

                                std::cout << "EVENT_WRITE MOD" << std::endl;
                            }
                        }
                    }
                    void *x = reinterpret_cast<void *>(id);
                    poll.pollMod(sessions[id]->fd, ev, x);
                    break;
                }
                case Session::Type::LISTEN: {
                    for (;;) {
                        int asock = accept(sock, NULL, NULL);
                        if (asock > 0) {

                            int ret = 0;
                            int clientFd = asock;
                            auto &poller = *this->poller;

                            int nRcvBufferLen = 32 * 1024 * 1024;
                            int nSndBufferLen = 32 * 1024 * 1024;
                            int nLen = sizeof(int);
                            setSockOpt(clientFd, SOL_SOCKET, SO_SNDBUF, &nSndBufferLen, nLen);
                            setSockOpt(clientFd, SOL_SOCKET, SO_RCVBUF, &nRcvBufferLen, nLen);

#ifdef OS_DARWIN
                            int opValue = 1;
                            if (0 > setSockOpt(clientFd, SOL_SOCKET, SO_NOSIGPIPE, &opValue, sizeof(opValue))) {
                                std::cout << "err: SO_NOSIGPIPE" << std::endl;
                            }
#endif
                            int nodelay = 1;
                            if (setSockOpt(clientFd, IPPROTO_TCP, TCP_NODELAY, &nodelay,
                                           sizeof(nodelay)) < 0)
                                perror("err: nodelay\n");
                            setSockNonBlock(clientFd);


                            Session *aconn = this->initNewSession(asock, Session::Type::ACCEPT);
                            if (aconn == NULL) {

                                this->logger->output("Accept Session exhausted", 0);
                                closeSocket(asock);
                                continue;
                            }
                            int aid = aconn->sessionId;

                            this->onAccept(sessions[id], sessions[aid]);
                            if (sessions[id] == NULL)
                                continue;
                        } else {
                            // err checkTODO
                            break;
                        }
                    }
                    break;
                }

                case Session::Type::NONE: {
                    std::abort();
                }
            }

        }
    } else if (numEvents == 0) {

    }

    return 0;
}


int Poller::handleReadEvent(Session *connp) {
    Session &conn = *connp;
    if (conn.readBuffer.size > conn.readBuffer.maxSize)
        return -1;

    unsigned char *buff = conn.readBuffer.buff + conn.readBuffer.size;

    int ret = recv(conn.fd, (char *) buff, conn.readBuffer.capacity - conn.readBuffer.size, 0);

    if (ret > 0) {
        conn.readBuffer.size += ret;
        conn.readBuffer.alloc();
        this->onRecv(connp, ret);
    } else if (ret == 0) {
        return -3;
    } else {
        if (!IsEagain()) {
            return -4;
        }
    }

    return 0;
}

int Poller::postSend(Session *connp) {
    Session &conn = *connp;
    if (conn.writeBuffer.size == 0)
        return 0;


    int ret = send(connp->fd, (const char *) conn.writeBuffer.buff,
                   conn.writeBuffer.size, 0);

    if (ret == -1) {

        if (!IsEagain()) {
            std::cout << "err: write" << getSockError() << std::endl;
            return -3;
        }

    } else if (ret == 0) {
    } else {
        conn.writeBuffer.erase(ret);
        this->onSend(connp, ret);
    }
    return 0;
}

int Poller::createListenSocket(int port) {
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);

    int reuse = 1;
    setSockOpt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int nRcvBufferLen = 32 * 1024 * 1024;
    int nSndBufferLen = 32 * 1024 * 1024;
    int nLen = sizeof(int);

    setSockOpt(listenSocket, SOL_SOCKET, SO_SNDBUF, &nSndBufferLen, nLen);
    setSockOpt(listenSocket, SOL_SOCKET, SO_RCVBUF, &nRcvBufferLen, nLen);

    setSockNonBlock(listenSocket);
    struct sockaddr_in lisAddr;
    lisAddr.sin_family = AF_INET;
    lisAddr.sin_port = htons(port);
    lisAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenSocket, (struct sockaddr *) &lisAddr, sizeof(lisAddr)) == -1) {
        std::cout << "bind" << std::endl;
        return -1;
    }

    if (::listen(listenSocket, 4096) < 0) {
        std::cout << "listen" << std::endl;
        return -1;
    }

    return listenSocket;
}

Session *Poller::initNewSession(uint64_t fd, Session::Type type1) {
    //onaccept
    //onconnect direct
    //onconnect by timer
    //listern to
    int i = 0;
    Session* p = NULL;
    for (; i < this->maxId; this->curId = (this->curId + 1) % this->maxId, i++) {
        auto &conn = sessions[this->curId];
        if (conn != NULL)
            continue;
        conn = (Session *) xmalloc(sizeof(Session));
        conn->reset();
        conn->sessionId = this->curId;
        conn->fd = (uint64_t) fd;

        conn->type = type1;

        p = conn;
        this->curId = (this->curId + 1) % this->maxId;
        break;
    }
    if (p == NULL) {
        return NULL;
    }

    return p;
}


#endif