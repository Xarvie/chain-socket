#include "SystemReader.h"

#if defined(OS_DARWIN) && !defined(SELECT_SERVER)

#include "KqueuePoller.h"

#define on_error printf

Poller::Poller(int port, int threadsNum) {
    this->port = port;
    this->maxWorker = threadsNum;
    sessions.resize(CONN_MAXFD);
    for (int i = 0; i < CONN_MAXFD; i++) {
        sessions[i] = (Session*)xmalloc(sizeof(Session));
        sessions[i]->reset();
        sessions[i]->sessionId = (uint64_t) i;
    }
}

Poller::~Poller() {
    this->isRunning = false;

    for (auto &E:this->workThreads) {
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

    while (1) {
        while (queue.try_dequeue(event1)) {
            if (event1.event == ACCEPT_EVENT) {
                int ret = 0;
                int clientFd = event1.fd;

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

                    Session* conn = this->sessions[clientFd];

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
                    this->onAccept(*conn, Addr());
                }
            }
        }
        struct timespec timeout;
        timeout.tv_sec = 1;
        timeout.tv_nsec = 0;
        nev = kevent(this->queue[pollerIndex], NULL, 0, event_list[pollerIndex], 32, &timeout);
        if (nev == 0) {

            continue;
        } else if (nev < 0) {
            std::cout << "kevent < 0" << std::endl;
            return;
        }
        for (int event = 0; event < nev; event++) {
            if (event_list[pollerIndex][event].flags & EV_EOF) {

            }
            if (event_list[pollerIndex][event].flags & EVFILT_READ) {
                if (-1 == this->handleReadEvent(&event_list[pollerIndex][event])) {
                    int sock = event_list[pollerIndex][event].ident;
                    Session *conn = this->sessions[sock];
                    this->closeSession(*conn);
                }

            }
            if (event_list[pollerIndex][event].flags & EVFILT_WRITE) {
                int sock = event_list[pollerIndex][event].ident;
                Session *conn = this->sessions[sock];
                if (-1 == this->handleWriteEvent(&event_list[pollerIndex][event])) {
                    this->closeSession(*conn);
                } else{
                    EV_SET(&event_set[pollerIndex], conn->sessionId, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, NULL);
                }
            }
        }
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
        taskQueue[pollerId].enqueue(x);
    }
}

int Poller::handleReadEvent(struct kevent *event) {
    int sock = event->ident;
    Session *conn = this->sessions[sock];
    if (conn->readBuffer.size < 0)
        return -1;
    unsigned char *buff = conn->readBuffer.buff + conn->readBuffer.size;

    int ret = recv(conn->sessionId, buff, conn->readBuffer.capacity - conn->readBuffer.size, 0);
    if (ret > 0) {
        conn->readBuffer.size += ret;
        conn->readBuffer.alloc();
        if (conn->readBuffer.size > 1024 * 1024 * 3) {
            return -1;
            //TODO close socket
        }
        //TODO
        int readBytes = onReadMsg(*conn, ret);
        conn->readBuffer.size -= readBytes;
        if (conn->readBuffer.size < 0)
            return -1;
    } else if (ret == 0) {
        return -1;
    } else {
        if (errno != EWOULDBLOCK && errno != EAGAIN) return -1;
    }

    return 0;
}

int Poller::handleWriteEvent(struct kevent *event) {

    int sock = event->ident;
    int pollerIndex = sock % this->maxWorker;
    Session *conn = sessions[sock];
    if (conn->writeBuffer.size == 0)
        return 0;

    if (conn->writeBuffer.size < 0)
        return -1;

    int ret = send(conn->sessionId, (void *) conn->writeBuffer.buff,
                   conn->writeBuffer.size, 0);

    if (ret == -1) {
        if (errno != EINTR && errno != EAGAIN) {
            return -1;
        }
    }else if(ret == 0){
        //TODO
    }
    else {
        onWriteBytes(*conn, ret);
        conn->writeBuffer.erase(ret);
    }
    return 0;
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
    EV_SET(&event_set[index], conn.sessionId, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    EV_SET(&event_set[index], conn.sessionId, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

    closeSocket(conn.sessionId);
}

bool Poller::createListenSocket(int port)
 {
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
            event_list.push_back((struct kevent *) xmalloc(1024 * sizeof(struct kevent)));

        }
    {/* create listen*/
        this->createListenSocket(port);
    }

    for (int i = 0; i < this->maxWorker; i++) {
        workThreads.emplace_back(std::thread([=] { this->workerThreadCB(i); }));//TODO
    }

    listenThread = std::thread([=] { this->listenThreadCB(); });//TODO

    listenThread.join();
    for (auto &E:workThreads) {
        E.join();
    }

    return 0;
}

int Poller::sendMsg(Session& conn, const Msg &msg) {
    unsigned char *data = msg.buff;
    int fd = conn.sessionId;
    int len = msg.len;
    int pollerIndex = fd % this->maxWorker;
    int leftBytes = 0;
    if (conn.writeBuffer.size > 0) {
        conn.writeBuffer.push_back(len, data);
        return 0;
    } else {
        int ret = send(conn.sessionId, data, len, 0);
        if (ret > 0) {
            if (ret == len)
                return 0;

            leftBytes = len - ret;
            conn.writeBuffer.push_back(leftBytes, data + ret);
        } else {
            if (errno != EINTR && errno != EAGAIN)
                return -1;

            leftBytes = len;
            conn.writeBuffer.push_back(len, data);
        }
    }
    if (leftBytes > 0) {
        EV_SET(&event_set[pollerIndex], conn.sessionId, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, NULL);
    }


    return 0;
}

#endif