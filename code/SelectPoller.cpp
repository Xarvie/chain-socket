#include "SystemReader.h"


#include "SelectPoller.h"
#include "NetStruct.h"
#include "PrivateHeaders.h"
#include "Timer.h"

#if defined(OS_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#undef FD_SETSIZE
#define FD_SETSIZE  1024

#include <mswsock.h>
#include <ws2tcpip.h>

#endif


inline int getPollIndex(int id, int maxNum) {
#ifdef OS_WINDOWS
    int index = id / 4 % maxNum;
#else
    int index = id % maxNum;
#endif
    return index;
}

void PollInit() {
#ifdef OS_WINDOWS
    WORD ver = MAKEWORD(2, 2);
    WSADATA dat;
    WSAStartup(ver, &dat);
#else
    signal(SIGPIPE, SIG_IGN);
#endif
}


SelectPoller::SelectPoller(int maxEvent) : maxEvent(maxEvent) {

}

SelectPoller::~SelectPoller() {
}

int SelectPoller::pollMod(int sockFd, int flag/*EVENT_READ | EVENT_WRITE | EVENT_ONCE*/, void *ptr) {
    SocketData x;
    bool oneshot = false;
    if (flag & EVENT_ONCE)
        oneshot = true;
    x.flag = flag;
    x.fd = sockFd;
    x.data.ptr = ptr;
    clients[sockFd] = x;
    return 0;
}

int SelectPoller::pollAdd(int sockFd, int flag/*EVENT_READ | EVENT_WRITE | EVENT_ONCE*/, void *ptr) {
    SocketData x;
    bool oneshot = false;
    if (flag & EVENT_ONCE)
        oneshot = true;
    x.flag = flag;
    x.fd = sockFd;
    x.data.ptr = ptr;
    clients[sockFd] = x;
    return 0;
}

int SelectPoller::pollDel(int sockFd) {
    if (clients.count(sockFd) > 0)
        clients.erase(sockFd);
    return 0;
}

int SelectPoller::pollGetEvents(int timeout, int maxEvent) {
    long long nowtick = TimerManager::GetCurrentMillisecs();
    long long outtick = nowtick + timeout;

    for (;;) {
        evClients.clear();
        auto it = clients.begin();
        int sz = (int) clients.size();
        for (int allc = 0; allc < sz;) {
            fd_set fdRead;
            fd_set fdWrite;
            fd_set fdExp;

            FD_ZERO(&fdRead);
            FD_ZERO(&fdWrite);
            FD_ZERO(&fdExp);

            uint64_t maxSock = 0;
            int counter = 0;
            auto it2 = it;
            for (; it != clients.end() && counter < 1023; it++) {
                counter++;
                int trigger = false;
                auto &E = *it;
                if (E.second.flag & EVENT_READ) {
                    FD_SET(E.second.fd, &fdRead);
                }
                if (E.second.flag & EVENT_WRITE) {
                    FD_SET(E.second.fd, &fdWrite);
                }

                if (maxSock < E.second.fd) {
                    maxSock = E.second.fd;
                }
            }
            allc += counter;

            if (maxSock == 0) {
                return 0;
            }
            timeval t = {0, 0};

            int ret = select((int) maxSock + 1, &fdRead, &fdWrite, &fdExp, &t);
            if (ret < 0) {

#if !defined(OS_WINDOWS)
                std::cout << "err: select" << errno << std::endl;
#else
                std::cout << "err: select " << WSAGetLastError() << std::endl;
#endif
            }


            for (int i = 0; i < counter; i++, it2++) {
                auto &E = *it2;
                int trigger_flag = 0;
                if (E.second.flag & EVENT_READ && FD_ISSET(E.second.fd, &fdRead)) {
                    trigger_flag |= EVENT_READ;
                }
                if (E.second.flag & EVENT_WRITE && FD_ISSET(E.second.fd, &fdWrite)) {
                    trigger_flag |= EVENT_WRITE;
                }
                if (trigger_flag != 0) {
                    select_data_t data;
                    data.fd = 0;
                    trigger_flag |= E.second.flag & EVENT_ONCE ? EVENT_ONCE : 0;
                    evClients.emplace_back(SocketData{trigger_flag, E.second.fd, data});
                    if (evClients.size() > maxEvent)
                        break;
                }

            }
        }
        for (auto &E :evClients) {
            if (E.flag & EVENT_ONCE)
                clients.erase(E.fd);
        }

        if (evClients.size() > 0 || TimerManager::GetCurrentMillisecs() > outtick) {
            return evClients.size();
        } else {
            if (timeout < 10);
            else if (timeout < 100)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            else
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    return 0;
}

void *SelectPoller::getEventConn(int index) {
    return evClients[index].data.ptr;
}

int SelectPoller::getEventTrigerFlag(int index) {
    return evClients[index].flag;
}
