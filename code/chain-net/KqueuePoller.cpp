#include "SystemReader.h"

#if defined(OS_DARWIN) && !defined(SELECT_SERVER)

#include "KqueuePoller.h"
#include "Timer.h"

#define on_error printf

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

    for (auto &E:this->workThreads) {
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
    close(listenSocket);
}

struct event_data {
    char buffer[BUFFER_SIZE];
    int buffer_read;
    int buffer_write;
    Poller *this_ptr;
    int type;

    int (*on_read)(struct event_data *self, struct kevent *event);

    int (*on_write)(struct event_data *self, struct kevent *event);
};

void Poller::workerThreadCB(int pollerIndex) {
    int nev = 0;
    bool dequeueRet = false;
    sockInfo event1;
    moodycamel::ConcurrentQueue<sockInfo> &queue = taskQueue[pollerIndex];
    std::set<Session *> disconnectSet;

    bool isIdle = false;

    while (this->isRunning) {
        if (isIdle)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        this->workerVec[pollerIndex]->lock.lock();
        while (queue.try_dequeue(event1)) {
            if (event1.event == CHECK_HEARTBEATS) {
                for (auto &E : this->workerVec[pollerIndex]->onlineSessionSet) {
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
                        this->workerVec[pollerIndex]->evVec.emplace_back(std::pair<Session *, int>(E, REQ_DISCONNECT1));
                    }
                    disconnectSet.clear();
                }
            }
        }

        for (auto &E : this->workerVec[pollerIndex]->onlineSessionSet) {
            Session &conn = *E;
            if (conn.canWrite == false)
                continue;

            int ret = send(conn.sessionId, conn.writeBuffer.buff, conn.writeBuffer.size, 0);
            if (ret > 0) {

                conn.writeBuffer.erase(ret);
                if (conn.writeBuffer.size == 0)
                    conn.canWrite = false;

            } else {
                if (errno != EINTR && errno != EAGAIN) {
                    if (conn.heartBeats > 0) {
                        E->heartBeats = -1;
                        this->workerVec[pollerIndex]->evVec.emplace_back(std::pair<Session *, int>(E, REQ_DISCONNECT2));
                    }
                    continue;
                }

                EV_SET(&event_set[pollerIndex], conn.sessionId, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, NULL);
            }
        }


        this->onIdle(pollerIndex);
        this->workerVec[pollerIndex]->tm->DetectTimers();

        struct timespec timeout;
        timeout.tv_sec = 0;
        timeout.tv_nsec = 0;
        nev = kevent(this->queue[pollerIndex], nullptr, 0, event_list[pollerIndex], MAX_EVENT, &timeout);
        if (nev == 0) {
            isIdle = true;
            this->workerVec[pollerIndex]->lock.unlock();
            continue;
        } else if (nev < 0) {
            std::cout << "kevent < 0" << std::endl;
            return;
        }
        isIdle = false;
        for (int event = 0; event < nev; event++) {
            if (event_list[pollerIndex][event].flags & EV_EOF) {

            }
            if (event_list[pollerIndex][event].flags & EVFILT_READ) {
                if (-1 == this->handleReadEvent(&event_list[pollerIndex][event])) {
                    int sock = event_list[pollerIndex][event].ident;
                    Session &conn = *this->sessions[sock];
                    if (conn.heartBeats > 0) {
                        conn.heartBeats = -1;
                        this->workerVec[pollerIndex]->evVec.emplace_back(
                                std::pair<Session *, int>(sessions[sock], REQ_DISCONNECT4));
                    }
                    continue;
                }

            }
            if (event_list[pollerIndex][event].flags & EVFILT_WRITE) {
                int sock = event_list[pollerIndex][event].ident;
                Session *conn = this->sessions[sock];
                if (-1 == this->handleWriteEvent(&event_list[pollerIndex][event])) {
                    int sock = event_list[pollerIndex][event].ident;
                    Session &conn = *this->sessions[sock];
                    if (conn.heartBeats > 0) {
                        conn.heartBeats = -1;
                        this->workerVec[pollerIndex]->evVec.emplace_back(
                                std::pair<Session *, int>(sessions[sock], REQ_DISCONNECT3));
                    }
                    continue;
                } else {
                    EV_SET(&event_set[pollerIndex], conn->sessionId, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0,
                           NULL);//TODO
                }
            }
        }
        this->workerVec[pollerIndex]->lock.unlock();
    }
}

void Poller::listenThreadCB() {
    int err, flags;

    struct sockaddr client;
    socklen_t client_len = sizeof(client);

    while (this->isRunning) {
        int client_fd = accept(this->listenSocket, &client, &client_len);
        if (client_fd < 0) {
            on_error("Accept failed (should this be fatal?): %s\n", strerror(errno));
        }
        int pollerId = 0;
#ifdef OS_WINDOWS
        pollerId = client_fd / 4 % maxWorker;
#else
        pollerId = client_fd % maxWorker;
#endif
        sockInfo x;
        x.fd = client_fd;
        x.event = ACCEPT_EVENT;
        this->logicTaskQueue.enqueue(x);
    }
}

int Poller::handleReadEvent(struct kevent *event) {
    int sock = event->ident;
    Session *conn = this->sessions[sock];
    if (conn->heartBeats <= 0 || conn->readBuffer.size > 1024 * 1024 * 3)
        return -1;
    unsigned char *buff = conn->readBuffer.buff + conn->readBuffer.size;

    int ret = recv(conn->sessionId, buff, conn->readBuffer.capacity - conn->readBuffer.size, 0);
    if (ret > 0) {
        conn->readBuffer.size += ret;
        conn->readBuffer.alloc();
        conn->canRead = true;
        if (conn->readBuffer.size > 1024 * 1024 * 3) {
            return -2;
            //TODO close socket
        }
        conn->heartBeats = HEARTBEATS_COUNT;

    } else if (ret == 0) {
        return -3;
    } else {
        if (!IsEagain()) return -4;
    }

    return 0;
}

int Poller::handleWriteEvent(struct kevent *event) {

    int sock = event->ident;
    int pollerIndex = sock % this->maxWorker;
    Session *conn = sessions[sock];

    if (conn->heartBeats <= 0)
        return -1;
    if (conn->writeBuffer.size == 0)
        return 0;


    int ret = send(conn->sessionId, (void *) conn->writeBuffer.buff,
                   conn->writeBuffer.size, 0);

    if (ret == -1) {
        if (!IsEagain()) {
            return -1;
        }
    } else if (ret == 0) {
        //TODO
    } else {
        onWriteBytes(*conn, ret);
        conn->writeBuffer.erase(ret);
    }
    return 0;
}

void Poller::closeSession(Session &conn, int type) {
    if (conn.heartBeats == 0)
        return;
    int index = conn.sessionId % this->maxWorker;
    linger lingerStruct;

    lingerStruct.l_onoff = 1;
    lingerStruct.l_linger = 0;
    setsockopt(conn.sessionId, SOL_SOCKET, SO_LINGER,
               (char *) &lingerStruct, sizeof(lingerStruct));
    conn.readBuffer.size = 0;
    conn.writeBuffer.size = 0;
    conn.heartBeats = 0;
    EV_SET(&event_set[index], conn.sessionId, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    EV_SET(&event_set[index], conn.sessionId, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

    closeSocket(conn.sessionId);
    this->workerVec[index]->onlineSessionSet.erase(&conn);
    this->onDisconnect(conn, type);
}

bool Poller::createListenSocket(int port) {
    int err = 0, flags = 0;
    this->listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (this->listenSocket < 0) on_error("err: socket");
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    int opt_val = 1;
    setsockopt(this->listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

    err = bind(this->listenSocket, (struct sockaddr *) &server, sizeof(server));
    if (err < 0) on_error("Could not bind server socket: %s\n", strerror(errno));

    flags = fcntl(this->listenSocket, F_GETFL, 0);
    if (flags < 0) on_error("Could not get server socket flags: %s\n", strerror(errno));

    // err = fcntl(this->listenSocket, F_SETFL, flags | O_NONBLOCK);
    //if (err < 0) on_error("Could set server socket to be non blocking: %s\n", strerror(errno));

    err = ::listen(this->listenSocket, SOMAXCONN);
    if (err < 0) on_error("Could not listen: %s\n", strerror(errno));
    return 0;
}

int Poller::run() {
    signal(SIGPIPE, SIG_IGN);
    this->isRunning = true;
    taskQueue.resize(this->maxWorker);
    event_set.resize(this->maxWorker);
    for (int i = 0; i < this->maxWorker; i++) {
        this->queue.push_back(kqueue());
        if (this->queue[i] < 0) on_error("Could not create kqueue: %s\n", strerror(errno));
        event_list.push_back((struct kevent *) xmalloc(MAX_EVENT * sizeof(struct kevent)));

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

    for (int i = 0; i < this->maxWorker; i++) {
        workThreads.emplace_back(std::thread([=] { this->workerThreadCB(i); }));//TODO
    }

    {
        this->logicWorker = std::thread([=] { this->logicWorkerThreadCB(); });
    }

    listenThread = std::thread([=] { this->listenThreadCB(); });//TODO

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

    this->heartBeatsThread.join();
    this->listenThread.join();
    this->logicWorker.join();
    for (auto &E:workThreads) {
        E.join();
    }

    return 0;
}

int Poller::sendMsg(Session &conn, const Msg &msg) {
    if (conn.heartBeats == 0)
        return -1;
    unsigned char *data = msg.buff;
    int fd = conn.sessionId;
    int len = msg.len;
    int pollerIndex = fd % this->maxWorker;
    int leftBytes = 0;
    if (conn.writeBuffer.size > 0) {
        conn.writeBuffer.push_back(len, data);
        return 0;
    }
    conn.writeBuffer.push_back(len, data);
    conn.canWrite = true;


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

                    {
                        int nodelay = 1;
                        if (setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &nodelay,
                                       sizeof(nodelay)) < 0)
                            perror("error: nodelay");

                        int nRcvBufferLen = 80 * 1024;
                        int nSndBufferLen = 1 * 1024 * 1024;
                        int nLen = sizeof(int);

                        setsockopt(clientFd, SOL_SOCKET, SO_SNDBUF, (char *) &nSndBufferLen, nLen);
                        setsockopt(clientFd, SOL_SOCKET, SO_RCVBUF, (char *) &nRcvBufferLen, nLen);

                        Session *conn = this->sessions[clientFd];

                        int flags = fcntl(clientFd, F_GETFL, 0);
                        if (flags < 0) on_error("Could not get client socket flags: %s\n", strerror(errno));

                        int err = fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);
                        if (err < 0) on_error("Could not set client socket to be non blocking: %s\n", strerror(errno));
                        int pollerIndex = clientFd % this->maxWorker;

                        EV_SET(&event_set[pollerIndex], clientFd, EVFILT_READ, EV_ADD, 0, 0, NULL);
                        if (kevent(this->queue[pollerIndex], &event_set[pollerIndex], 1, NULL, 0, NULL) == -1) {
                            printf("error\n");
                        }
                        EV_SET(&event_set[pollerIndex], clientFd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, NULL);
                        if (kevent(this->queue[pollerIndex], &event_set[pollerIndex], 1, NULL, 0, NULL) == -1) {
                            printf("error\n");
                        }
                        conn->readBuffer.size = 0;
                        conn->writeBuffer.size = 0;
                        conn->heartBeats = HEARTBEATS_COUNT;
                        this->workerVec[pollerIndex]->onlineSessionSet.insert(conn);
                        this->onAccept(*conn, Addr());
                    }

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