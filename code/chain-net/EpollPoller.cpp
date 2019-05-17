
#include "SystemReader.h"

#if defined(OS_LINUX) && !defined(SELECT_SERVER)

#include "EpollPoller.h"
#include "SystemInterface.h"
#include "Timer.h"

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

Poller::~Poller() {
    this->isRunning = false;

    for (auto &E:workThreads) {
        if (E.joinable())
            E.join();
    }
    if (this->listenThread.joinable()) {
        this->listenThread.join();
    }
    for (int i = 0; i < CONN_MAXFD; i++) {
        sessions[i]->readBuffer.destroy();
        sessions[i]->writeBuffer.destroy();
    }

    for (int epi = 0; epi < this->maxWorker; ++epi) {
        close(epolls[epi]);
    }
    close(listenSocket);
}

void Poller::closeSession(Session &conn, int type) {
    if (conn.heartBeats == 0)
        return;
#ifdef OS_WINDOWS
    int index = conn.sessionId / 4 % this->maxWorker;
#else
    int index = conn.sessionId % this->maxWorker;
#endif
    linger lingerStruct;

    lingerStruct.l_onoff = 1;
    lingerStruct.l_linger = 0;
    setsockopt(conn.sessionId, SOL_SOCKET, SO_LINGER,
               (char *) &lingerStruct, sizeof(lingerStruct));
    conn.readBuffer.size = 0;
    conn.writeBuffer.size = 0;
    conn.heartBeats = 0;
    closeSocket(conn.sessionId);

    this->workerVec[index]->onlineSessionSet.erase(&conn);
    // TODO log std::cout << "close" << icc << std::endl;
    this->onDisconnect(conn, type);
}

int Poller::sendMsg(Session &conn, const Msg &msg) {
    if (conn.heartBeats == 0)
        return -1;
    int fd = conn.sessionId;
    int len = msg.len;
    unsigned char *data = msg.buff;
    if (conn.writeBuffer.size > 0) {
        conn.writeBuffer.push_back(len, data);
        return 0;
    }
    conn.writeBuffer.push_back(len, data);
    conn.canWrite = true;
    return 0;
}

int Poller::handleReadEvent(Session &conn) {
    if (conn.heartBeats <= 0 || conn.readBuffer.size > 1024 * 1024 * 3)
        return -1;
    static Msg msg;
    unsigned char *buff = conn.readBuffer.buff + conn.readBuffer.size;

    int ret = recv(conn.sessionId, buff, conn.readBuffer.capacity - conn.readBuffer.size, 0);

    if (ret > 0) {
        conn.readBuffer.size += ret;
        conn.readBuffer.alloc();
        conn.canRead = true;
        if (conn.readBuffer.size > 1024 * 1024 * 3) {
            return -2;
            //TODO close socket
        }
        conn.heartBeats = HEARTBEATS_COUNT;

        //TODO
        //int readBytes = onReadMsg(conn, ret);
        //conn.readBuffer.size -= readBytes;
        //if (conn.readBuffer.size < 0)
        //    return -1;
    } else if (ret == 0) {
        return -3;
    } else {
        if (errno != EINTR && errno != EAGAIN) {
            return -4;
        }
    }

    return 0;
}

int Poller::handleWriteEvent(Session &conn) {
    if (conn.heartBeats <= 0)
        return -1;
    if (conn.writeBuffer.size == 0)
        return 0;

    int ret = write(conn.sessionId, (void *) conn.writeBuffer.buff,
                    conn.writeBuffer.size);

    if (ret == -1) {

        if (errno != EINTR && errno != EAGAIN) {
            printf("err: write%d\n", errno);
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

void Poller::workerThreadCB(int index) {
    int epfd = this->epolls[index];

    struct epoll_event event[MAX_EVENT];
    struct epoll_event evReg;
    sockInfo task = {0};
    std::set<Session *> disconnectSet;
    bool isIdle = false;
    moodycamel::ConcurrentQueue<sockInfo> &queue = taskQueue[index];

    while (this->isRunning) {
        if (isIdle)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        this->workerVec[index]->lock.lock();
        while (queue.try_dequeue(task)) {
            if (task.event == CHECK_HEARTBEATS) {
                for (auto &E : this->workerVec[index]->onlineSessionSet) {
                    if (E->heartBeats <= 0)
                        continue;

                    if (E->heartBeats == 1) {
                        auto &conn = *this->sessions[E->sessionId];

                        disconnectSet.insert(&conn);
                    } else {
                        E->heartBeats--;
                    }
                }
                if (disconnectSet.size() > 0) {
                    for (auto &E :disconnectSet) {
                        E->heartBeats = -1;
                        this->workerVec[index]->evVec.emplace_back(std::pair<Session *, int>(E, REQ_DISCONNECT1));

                    }
                    disconnectSet.clear();
                }
            }
        }
        for (auto &E : this->workerVec[index]->onlineSessionSet) {
            Session &conn = *E;
            if (conn.canWrite == 0)
                continue;

            int ret = send(conn.sessionId, conn.writeBuffer.buff, conn.writeBuffer.size, 0);
            if (ret > 0) {

                conn.writeBuffer.erase(ret);
                if (conn.writeBuffer.size == 0)
                    conn.canWrite = 0;

            } else {
                if (errno != EINTR && errno != EAGAIN) {
                    if (conn.heartBeats > 0) {
                        E->heartBeats = -1;
                        this->workerVec[index]->evVec.emplace_back(std::pair<Session *, int>(E, REQ_DISCONNECT2));
                    }
                    continue;
                }
                evReg.events = EPOLLIN | EPOLLONESHOT;
                if (conn.writeBuffer.size > 0)
                    evReg.events |= EPOLLOUT;
                evReg.data.fd = (int) conn.sessionId;
                epoll_ctl(epfd, EPOLL_CTL_MOD, conn.sessionId, &evReg);
            }
        }
        this->onIdle(index);
        this->workerVec[index]->tm->DetectTimers();


        int numEvents = epoll_wait(epfd, event, MAX_EVENT, 0);//TODO wait 1

        if (numEvents == -1) {
            //printf("wait\n %d", errno);
        } else if (numEvents > 0) {
            isIdle = false;
            for (int i = 0; i < numEvents; i++) {
                int sock = event[i].data.fd;
                Session &conn = *this->sessions[sock];
                if (event[i].events & EPOLLOUT) {
                    if (this->handleWriteEvent(conn) == -1) {
                        if (conn.heartBeats > 0) {
                            conn.heartBeats = -1;
                            this->workerVec[index]->evVec.emplace_back(
                                    std::pair<Session *, int>(sessions[sock], REQ_DISCONNECT3));
                        }
                        std::cout << "write" << std::endl;
                        continue;
                    }
                }

                if (event[i].events & EPOLLIN) {
                    int ret = 0;
                    if ((ret = this->handleReadEvent(conn)) < 0) {
                        if (conn.heartBeats > 0) {
                            conn.heartBeats = -1;
                            this->workerVec[index]->evVec.emplace_back(
                                    std::pair<Session *, int>(sessions[sock], REQ_DISCONNECT4));
                        }
                        std::cout << "read" << ret << std::endl;
                        continue;
                    }
                }

                evReg.events = EPOLLIN | EPOLLONESHOT;
                if (conn.writeBuffer.size > 0)
                    evReg.events |= EPOLLOUT;
                evReg.data.fd = sock;
                epoll_ctl(epfd, EPOLL_CTL_MOD, conn.sessionId, &evReg);
            }
        } else if (numEvents == 0) {
            isIdle = true;
        }
        this->workerVec[index]->lock.unlock();
    }
}

void Poller::listenThreadCB() {
    int lisEpfd = epoll_create(5);

    struct epoll_event evReg;
    evReg.events = EPOLLIN;
    evReg.data.fd = this->listenSocket;


    epoll_ctl(lisEpfd, EPOLL_CTL_ADD, this->listenSocket, &evReg);

    struct epoll_event event;

    while (this->isRunning) {
        int numEvent = epoll_wait(lisEpfd, &event, 1, 1000);
        //TODO con
        if (numEvent > 0) {
            int sock = accept(this->listenSocket, NULL, NULL);
            if (sock > 0) {
                int pollerId = 0;
#ifdef OS_WINDOWS
                pollerId = sock / 4 % maxWorker;
#else
                pollerId = sock % maxWorker;
#endif
                sockInfo x;
                x.fd = sock;
                x.event = ACCEPT_EVENT;
                this->logicTaskQueue.enqueue(x);
            }
        }
    }

    close(lisEpfd);
}

bool Poller::createListenSocket(int port) {
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);

    int reuse = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int nRcvBufferLen = 32 * 1024 * 1024;
    int nSndBufferLen = 32 * 1024 * 1024;
    int nLen = sizeof(int);

    setsockopt(listenSocket, SOL_SOCKET, SO_SNDBUF, (char *) &nSndBufferLen, nLen);
    setsockopt(listenSocket, SOL_SOCKET, SO_RCVBUF, (char *) &nRcvBufferLen, nLen);

    int flag;
    flag = fcntl(listenSocket, F_GETFL);
    fcntl(listenSocket, F_SETFL, flag | O_NONBLOCK);

    struct sockaddr_in lisAddr;
    lisAddr.sin_family = AF_INET;
    lisAddr.sin_port = htons(port);
    lisAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenSocket, (struct sockaddr *) &lisAddr, sizeof(lisAddr)) == -1) {
        printf("bind");
        return false;
    }

    if (::listen(listenSocket, 4096) < 0) {
        printf("listen");
        return false;
    }

    return true;
}

int Poller::run() {

    {/* init */
        this->isRunning = true;
    }
    {/* init queue  */
        taskQueue.resize(this->maxWorker);
    }
    {/* create pollers*/
        epolls.resize(maxWorker);
        for (int epi = 0; epi < this->maxWorker; ++epi) {
            epolls[epi] = epoll_create(20);
        }
    }
    {/* create listen*/
        this->createListenSocket(port);
    }
    for (int i = 0; i < this->maxWorker; i++) {
        Worker *worker = new(xmalloc(sizeof(Worker))) Worker();
        worker->index = i;
        workerVec.push_back(worker);
        auto *tm1 = new(xmalloc(sizeof(TimerManager))) TimerManager();
        this->tm = new(xmalloc(sizeof(TimerManager))) TimerManager();
        worker->tm = tm1;
        this->onInit(i);
    }
    this->createTimerEvent(1000);

    {/* start workers*/
        for (int i = 0; i < this->maxWorker; ++i) {
            workThreads.emplace_back(std::thread([=] { this->workerThreadCB(i); }));
        }
    }

    {
        this->logicWorker = std::thread([=] { this->logicWorkerThreadCB(); });
    }

    {/* start listen*/
        listenThread = std::thread([=] { this->listenThreadCB(); });
    }

    this->heartBeatsThread = std::thread([this] {
        while (this->isRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(HEARTBEATS_INTERVAL * 1000));
            for (auto &E : this->taskQueue) {
                static sockInfo s;
                s.event = CHECK_HEARTBEATS;
                E.enqueue(s);
            }
        }

    });


    {/* wait exit*/
        this->heartBeatsThread.join();
        this->listenThread.join();
        this->logicWorker.join();
        for (auto &E: this->workThreads) {
            E.join();
        }
    }
    {/*exit*/
        close(listenSocket);
    }
    return 0;
}

int Poller::stop() {
    this->isRunning = false;
    return 0;
}

void Poller::logicWorkerThreadCB() {

    while (this->isRunning) {
        bool isIdle = true;

        for (int i = 0; i < this->maxWorker; i++) {
            this->workerVec[i]->lock.lock();
        }
        this->tm->DetectTimers();


        for (int i = 0; i < this->maxWorker; i++) {
            auto x = this->workerVec[i]->onlineSessionSet; //TODO
            for (auto &E2:x) {
                if (!E2->canRead)
                    continue;
                E2->canRead = false;
                isIdle = false;
                int readBytes = onReadMsg(*E2, E2->readBuffer.size);
                E2->readBuffer.size -= readBytes;//TODO size < 0
            }
            for (auto &E:this->workerVec[i]->evVec) {
                isIdle = false;
                Session *session = E.first;
                if (E.second == REQ_DISCONNECT1
                    || E.second == REQ_DISCONNECT2
                    || E.second == REQ_DISCONNECT3
                    || E.second == REQ_DISCONNECT4
                    || E.second == REQ_DISCONNECT5) {

                    this->closeSession(*session, E.second);
                }

            }
            this->workerVec[i]->evVec.clear();
        }
        sockInfo event = {0};
        while (logicTaskQueue.try_dequeue(event)) {
            switch (event.event) {
                case ACCEPT_EVENT: {
                    isIdle = false;
                    int ret = 0;
                    int clientFd = event.fd;
#if defined(OS_WINDOWS)
                    int index = clientFd / 4 % maxWorker;
#else
                    int index = clientFd % maxWorker;
#endif
                    int nRcvBufferLen = 80 * 1024;
                    int nSndBufferLen = 1 * 1024 * 1024;
                    int nLen = sizeof(int);

                    setsockopt(clientFd, SOL_SOCKET, SO_SNDBUF, (char *) &nSndBufferLen, nLen);
                    setsockopt(clientFd, SOL_SOCKET, SO_RCVBUF, (char *) &nRcvBufferLen, nLen);
                    int nodelay = 1;
                    if (setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &nodelay,
                                   sizeof(nodelay)) < 0)
                        perror("err: nodelay\n");
#if defined(OS_WINDOWS)
                    unsigned long ul = 1;
                    ret = ioctlsocket(clientFd, FIONBIO, (unsigned long *) &ul);
                    if (ret == SOCKET_ERROR)
                        printf("err: ioctlsocket");
#else
                    int flags = fcntl(clientFd, F_GETFL, 0);
                    if (flags < 0)
                        printf("err: F_GETFL \n");
                    ret = fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);
                    if (ret < 0)
                        printf("err: F_SETFL \n");
#endif
                    auto *conn = sessions[clientFd];
                    sessions[clientFd]->readBuffer.size = 0;
                    sessions[clientFd]->writeBuffer.size = 0;
                    conn->heartBeats = HEARTBEATS_COUNT;
                    struct epoll_event evReg;
                    evReg.data.fd = clientFd;
                    evReg.events = EPOLLIN;
                    this->sessions[clientFd]->sessionId = clientFd;
                    epoll_ctl(this->epolls[index], EPOLL_CTL_ADD, clientFd, &evReg);
                    this->workerVec[index]->onlineSessionSet.insert(conn);
                    this->onAccept(*sessions[clientFd], Addr());

                    break;
                }
                default: {
                    break;
                }
            }
        }


        for (int i = 0; i < this->maxWorker; i++) {
            for (auto &E:this->workerVec[i]->onlineSessionSet) //TODO
            {

            }
        }


        for (int i = 0; i < this->maxWorker; i++) {
            this->workerVec[i]->lock.unlock();
        }
        if (isIdle)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

    }

    for (int i = 0; i < this->maxWorker; i++) {
        std::set<Session *> backset = this->workerVec[i]->onlineSessionSet;
        for (auto &E : backset) {
            this->closeSession(*E, REQ_DISCONNECT5);
        }
    }

}

int Poller::createTimerEvent(int inv) {
    auto *t2 = new(xmalloc(sizeof(Timer))) Timer(*this->tm);
    t2->data = (char *) "bbb";
    t2->Start([this, inv](void *data) {
        this->onTimerEvent(inv);
    }, inv);

    return 0;
}

#endif