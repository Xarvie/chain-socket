#ifndef SELECTPOLLER_H_
#define SELECTPOLLER_H_

#include "SystemReader.h"


#include <map>
#include <vector>
#include <cstdint>

typedef union select_data {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} select_data_t;

class SelectPoller {
protected:
    struct SocketData {
        int flag = 0;
        int fd = 0;
        select_data_t data;
    };
public:
    SelectPoller(int maxEvent);

    ~SelectPoller();

    int pollMod(int sockFd, int flag/*EVENT_READ | EVENT_WRITE | EVENT_ONCE*/, void *ptr);

    int pollAdd(int sockFd, int flag/*EVENT_READ | EVENT_WRITE | EVENT_ONCE*/, void *ptr);

    int pollDel(int sockFd);

    int pollGetEvents(int timeout, int maxEvent);

    void *getEventConn(int index);

    int getEventTrigerFlag(int index);

public:
    int maxEvent = 0;
    int pollId = 0;
    std::map<int, SocketData> clients;
    std::vector<SocketData> evClients;
};


#endif