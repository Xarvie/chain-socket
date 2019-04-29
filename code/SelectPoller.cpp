#include "SystemReader.h"

#if defined(SELECT_SERVER)

#include "SystemInterface.h"
#include "SelectPoller.h"

Poller::Poller(int port, int threadsNum) {
    this->port = port;
    this->maxWorker = threadsNum;
    sessions.resize(CONN_MAXFD);
    for (int i = 0; i < CONN_MAXFD; i++) {
        sessions[i] = (Session *) xmalloc(sizeof(Session));
        sessions[i]->reset();
        sessions[i]->sessionId = (uint64_t) i;
    }
}

int Poller::sendMsg(Session &conn, const Msg &msg) {
    unsigned char *data = msg.buff;
    int len = msg.len;
    int leftBytes = 0;
    if (conn.writeBuffer.size > 0) {
        conn.writeBuffer.push_back(len, data);
        return 0;
    } else {
        int ret = send(conn.sessionId, (char *) data, len, 0);
        if (ret > 0) {
            if (ret == len)
                return 0;

            leftBytes = len - ret;
            conn.writeBuffer.push_back(leftBytes, data + ret);
        } else {
            int err = getSockError();

            if (!IsEagain()) {
                printf("err: send :%d \n", err);
                return -1;
            }
            conn.writeBuffer.push_back(len, data);
        }
    }
    return 0;
}

int Poller::handleReadEvent(Session &conn) {
    if (conn.readBuffer.size < 0)
        return -1;
    unsigned char *buff = conn.readBuffer.buff + conn.readBuffer.size;
    int ret = recv(conn.sessionId, (char *) buff, conn.readBuffer.capacity - conn.readBuffer.size, 0);

    if (ret > 0) {
        conn.readBuffer.size += ret;
        conn.readBuffer.alloc();
        if (conn.readBuffer.size > 1024 * 1024 * 3) {
            return -1;
        }
        //TODO
        int readBytes = onReadMsg(conn, ret);
        conn.readBuffer.size -= readBytes;
        if (conn.readBuffer.size < 0)
            return -1;
    } else if (ret == 0) {
        return -1;
    } else {
        int err = getSockError();

        if (!IsEagain()) {
            printf("err: recv %d", err);
            return -1;
        }
    }
    return 0;
}

int Poller::handleWriteEvent(Session &conn) {
    if (conn.writeBuffer.size == 0)
        return 0;
    if (conn.writeBuffer.size < 0)
        return -1;

    int ret = send(conn.sessionId, (char *) conn.writeBuffer.buff,
                   conn.writeBuffer.size, 0);
    if (ret == -1) {
        int err = getSockError();

        if (!IsEagain()) {
            printf("err:write %d\n", err);
            return -1;
        }
    } else if (ret == 0) {
        //TODO
    } else {
        onWriteBytes(conn, ret);
        conn.writeBuffer.erase(ret);
    }
    return 0;
}

void Poller::closeSession(Session &conn) {
    if (conn.readBuffer.size < 0 || conn.writeBuffer.size < 0)
        return;
#if defined(OS_WINDOWS)
    int index = conn.sessionId / 4 % this->maxWorker;
#else
    int index = conn.sessionId % this->maxWorker;
#endif
    std::set<uint64_t> &clientVec = this->clients[index];
    std::set<uint64_t> &acceptClientFdsVec = this->acceptClientFds[index];
    acceptClientFdsVec.erase(conn.sessionId);


    linger lingerStruct;

    lingerStruct.l_onoff = 1;
    lingerStruct.l_linger = 0;
    setsockopt(conn.sessionId, SOL_SOCKET, SO_LINGER,
               (char *) &lingerStruct, sizeof(lingerStruct));
    conn.readBuffer.size = -1;
    conn.writeBuffer.size = -1;
    closeSocket(conn.sessionId);
}

void Poller::workerThreadCB(int index) {
    std::set<uint64_t> &clientVec = this->clients[index];
    std::set<uint64_t> &acceptClientFdsVec = this->acceptClientFds[index];
    moodycamel::ConcurrentQueue<sockInfo> &queue = taskQueue[index];
    sockInfo event = {0};

    while (this->isRunning) {
        while (queue.try_dequeue(event)) {
            if (event.event == ACCEPT_EVENT) {
                int ret = 0;
                int clientFd = event.fd;

                int nRcvBufferLen = 80 * 1024;
                int nSndBufferLen = 1 * 1024 * 1024;
                int nLen = sizeof(int);

                setsockopt(clientFd, SOL_SOCKET, SO_SNDBUF, (char *) &nSndBufferLen, nLen);
                setsockopt(clientFd, SOL_SOCKET, SO_RCVBUF, (char *) &nRcvBufferLen, nLen);
#if defined(OS_WINDOWS)
                char nodelay = 1;
#else
                int nodelay = 1;
#endif
                if (setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &nodelay,
                               sizeof(nodelay)) < 0)
                    printf("err: nodelay");
#if defined(OS_WINDOWS)
                unsigned long ul = 1;
                ret = ioctlsocket(clientFd, FIONBIO, (unsigned long *) &ul);//设置成非阻塞模式。
                if (ret == SOCKET_ERROR)
                    printf("err: ioctlsocket");
#else
                int flags = fcntl(clientFd, F_GETFL, 0);
                if (flags < 0) printf("err: fcntl");

                ret = fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);
                if (ret < 0) printf("err: fcntl");
#endif

                acceptClientFdsVec.insert((uint64_t) event.fd);
                sessions[clientFd]->readBuffer.size = 0;
                sessions[clientFd]->writeBuffer.size = 0;
            }

        }

        {

            fd_set fdRead;
            fd_set fdWrite;
            fd_set fdExp;

            FD_ZERO(&fdRead);
            FD_ZERO(&fdWrite);
            FD_ZERO(&fdExp);

            uint64_t maxSock = 0;
            for (auto &E:acceptClientFdsVec)
                clientVec.insert(E);
            acceptClientFdsVec.clear();
            for (auto &E :clientVec) {
                FD_SET(E, &fdRead);
                FD_SET(E, &fdWrite);
                if (maxSock < E) {
                    maxSock = E;
                }
            }
            int sec = 1;
            timeval t = {sec, 0};
            if (maxSock == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sec * 1000));
                continue;
            }

            int ret = select(maxSock + 1, &fdRead, &fdWrite, &fdExp, &t);

            if (ret < 0) {

#if !defined(OS_WINDOWS)
                std::cout << "err: select" << errno << std::endl;
#else
                std::cout << "err: select " << WSAGetLastError() << std::endl;
#endif
                break;
            }

            if (ret > 0) {
                for (auto iter = clientVec.begin(); iter != clientVec.end();) {
                    uint64_t fd = *iter;
                    bool needDel = false;
                    if (FD_ISSET(fd, &fdRead)) {
                        if (handleReadEvent(*sessions[fd]) == -1) {
                            this->closeSession(*sessions[fd]);
                            iter = clientVec.erase(iter);
                            continue;
                        }
                    }
                    if (FD_ISSET(fd, &fdWrite)) {
                        if (handleWriteEvent(*sessions[fd]) == -1) {
                            this->closeSession(*sessions[fd]);
                            iter = clientVec.erase(iter);
                            continue;
                        }
                    }
                    iter++;
                }
            }

        }
    }

    for (auto &E : clientVec) {
        this->closeSession(*sessions[E]);
    }

}

int Poller::run() {
#ifdef OS_WINDOWS
    WORD ver = MAKEWORD(2, 2);
    WSADATA dat;
    WSAStartup(ver, &dat);

#else
    signal(SIGPIPE, SIG_IGN);
#endif
    this->isRunning = true;
    taskQueue.resize(maxWorker);
    clients.resize(maxWorker);
    acceptClientFds.resize(maxWorker);

    {/* create listen*/
        this->createListenSocket(port);
    }
    {/* start workers*/
        for (int i = 0; i < this->maxWorker; ++i) {
            workThreads.emplace_back(std::thread([=] { this->workerThreadCB(i); }));
        }
    }
    {/* start listen*/
        this->listenThread = std::thread([=] { this->listenThreadCB(); });
    }
    this->listenThread.join();
    for (auto &E:workThreads) {
        E.join();
    }

    return 0;
}

Poller::~Poller() {
    this->isRunning = false;

    for (auto &E:workThreads) {
        if(E.joinable())
            E.join();
    }
    if (this->listenThread.joinable()) {
        this->listenThread.join();
    }
    for (int i = 0; i < CONN_MAXFD; i++) {
        sessions[i]->readBuffer.destroy();
        sessions[i]->writeBuffer.destroy();
    }
    closeSocket(listenSocket);
}

bool Poller::createListenSocket(int port) {
    this->listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
#ifdef OS_WINDOWS
    addr.sin_addr.S_un.S_addr = INADDR_ANY;
#else
    addr.sin_addr.s_addr = INADDR_ANY;
#endif

#ifdef OS_WINDOWS
    //TODO
#else
    int opt_val = 1;
    setsockopt(this->listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));
#endif

    if (-1 == bind(this->listenSocket, (sockaddr *) &addr, sizeof(addr))) {
        printf("err: bind\n");
        return false;
    }

    if (-1 == ::listen(this->listenSocket, 1024)) {
        printf("err: listen\n");
        return false;
    }
    return true;
}

void Poller::listenThreadCB() {
    sockaddr_in clientAddr = {};
    int nAddrLen = sizeof(sockaddr_in);
    uint64_t clientSocket = 0;
    while (this->isRunning) {
#ifdef OS_WINDOWS
        clientSocket = accept(this->listenSocket, (sockaddr *) &clientAddr, &nAddrLen);
        if (INVALID_SOCKET == clientSocket) {//TODO
            printf("err: accept\n");
            exit(-2);
        }
        int pollerId = clientSocket / 4 % maxWorker;
#else
        clientSocket = accept(this->listenSocket, (sockaddr *) &clientAddr, (socklen_t *) &nAddrLen);
        if (-1 == clientSocket) {//TODO
            printf("err: accept\n");
            exit(-2);
        }
        int pollerId = clientSocket % maxWorker;
#endif

        sockInfo x;
        x.fd = clientSocket;
        x.event = ACCEPT_EVENT;
        taskQueue[pollerId].enqueue(x);
    }
}

#endif