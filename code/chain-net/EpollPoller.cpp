
#include "SystemReader.h"

#if defined(OS_LINUX) && !defined(SELECT_SERVER)

#include "EpollPoller.h"
#include "SystemInterface.h"

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

    for (int epi = 0; epi < this->maxWorker; ++epi) {
        close(epolls[epi]);
    }
    close(listenSocket);
}

void Poller::closeSession(Session &conn) {
    if (conn.readBuffer.size < 0 || conn.writeBuffer.size < 0)
        return;
    int index = conn.sessionId % this->maxWorker;
    linger lingerStruct;

    lingerStruct.l_onoff = 1;
    lingerStruct.l_linger = 0;
    setsockopt(conn.sessionId, SOL_SOCKET, SO_LINGER,
               (char *) &lingerStruct, sizeof(lingerStruct));
    conn.readBuffer.size = -1;
    conn.writeBuffer.size = -1;
    closeSocket(conn.sessionId);
}

int Poller::sendMsg(Session &conn, const Msg &msg) {
    int fd = conn.sessionId;
    int len = msg.len;
    unsigned char *data = msg.buff;
    if (conn.writeBuffer.size > 0) {
        conn.writeBuffer.push_back(len, data);
        return 0;
    }

    int ret = write(conn.sessionId, data, len);
    if (ret > 0) {

        this->onWriteBytes(conn, len);
        if (ret == len)
            return 0;

        int left = len - ret;

        conn.writeBuffer.push_back(left, data + ret);

    } else {
        if (errno != EINTR && errno != EAGAIN)
            return -1;

        conn.writeBuffer.push_back(len, data);
    }


    return 0;
}

int Poller::handleReadEvent(Session &conn) {
    if (conn.readBuffer.size < 0)
        return -1;
    static Msg msg;
    unsigned char *buff = conn.readBuffer.buff + conn.readBuffer.size;

    int ret = recv(conn.sessionId, buff, conn.readBuffer.capacity - conn.readBuffer.size, 0);

    if (ret > 0) {
        conn.readBuffer.size += ret;
        conn.readBuffer.alloc();
        if (conn.readBuffer.size > 1024 * 1024 * 3) {
            return -1;
            //TODO close socket
        }
        //TODO
        int readBytes = onReadMsg(conn, ret);
        conn.readBuffer.size -= readBytes;
        if (conn.readBuffer.size < 0)
            return -1;
    } else if (ret == 0) {
        return -1;
    } else {
        if (errno != EINTR && errno != EAGAIN) {
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

    struct epoll_event event[30];
    struct epoll_event evReg;
    sockInfo task;
    moodycamel::ConcurrentQueue<sockInfo> &queue = taskQueue[index];
    while (this->isRunning) {
        while (queue.try_dequeue(task)) {
            if (task.event == ACCEPT_EVENT) {
                int ret = 0;
                int clientFd = task.fd;

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
                ret = ioctlsocket(clientFd, FIONBIO, (unsigned long *) &ul);//设置成非阻塞模式。
                if (ret == SOCKET_ERROR)
                    printf("err: ioctlsocket");
#else
                int flags = fcntl(clientFd, F_GETFL, 0);
                if (flags < 0)
                    printf("err: F_GETFL\n");

                if(fcntl(clientFd, F_SETFL, flags | O_NONBLOCK) < 0);
                    printf("err: F_SETFL\n");
#endif

                sessions[clientFd]->readBuffer.size = 0;
                sessions[clientFd]->writeBuffer.size = 0;

                evReg.data.fd = clientFd;
                evReg.events = EPOLLIN | EPOLLONESHOT;
                this->sessions[clientFd]->sessionId = clientFd;
                epoll_ctl(this->epolls[index], EPOLL_CTL_ADD, clientFd, &evReg);

            }

        }

        int numEvents = epoll_wait(epfd, event, 30, 1000);//TODO wait 1

        if (numEvents == -1) {
            //printf("wait\n %d", errno);
        } else if (numEvents > 0) {
            for (int i = 0; i < numEvents; i++) {
                int sock = event[i].data.fd;
                Session &conn = *this->sessions[sock];
                if (event[i].events & EPOLLOUT) {
                    if (this->handleWriteEvent(conn) == -1) {
                        this->closeSession(conn);
                        continue;
                    }
                }

                if (event[i].events & EPOLLIN) {
                    if (this->handleReadEvent(conn) == -1) {
                        this->closeSession(conn);
                        continue;
                    }
                }

                evReg.events = EPOLLIN | EPOLLONESHOT;
                if (conn.writeBuffer.size > 0)
                    evReg.events |= EPOLLOUT;
                evReg.data.fd = sock;
                epoll_ctl(epfd, EPOLL_CTL_MOD, conn.sessionId, &evReg);
            }
        }
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
                taskQueue[pollerId].enqueue(x);
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
    {/* start workers*/
        for (int i = 0; i < this->maxWorker; ++i) {
            workThreads.emplace_back(std::thread([=] { this->workerThreadCB(i); }));
        }
    }
    {/* start listen*/
        listenThread = std::thread([=] { this->listenThreadCB(); });
    }
    {/* wait exit*/
        listenThread.join();
        for (auto &E: this->workThreads) {
            E.join();
        }
    }
    {/*exit*/
        close(listenSocket);
    }
    return 0;
}

#endif