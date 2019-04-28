#ifndef MAIN_SPIDER_H
#define MAIN_SPIDER_H

#include "SystemReader.h"
#include "NetStruct.h"
#include "Poller.h"
#include "Buffer.h"

class Spider;
class Session
{
public:
    uint64_t sessionId;
    int64_t preHeartBeats = 0;
    MessageBuffer writeBuffer;
    MessageBuffer readBuffer;

    void reset()
    {
        sessionId = 0;
        preHeartBeats = 0;
        readBuffer.reset();
        writeBuffer.reset();
    }
};

class PollerData{
public:
    PollerData(){

    }
    virtual ~PollerData(){

    }
    Spider* parent;
    std::vector<Session*> sessions;
    std::thread workThread;
    moodycamel::ConcurrentQueue<sockInfo> taskQueue;
    std::set<uint64_t> clients;
    std::set<uint64_t> acceptClientFds;
    int pollID;
};

class Spider : public Poller {
public:

    Spider &operator=(Spider &&rhs) noexcept;

    virtual ~Spider();

    void run();

    void stop();

    virtual int onAccept(uint64_t sessionId, const Addr &addr) ;

    virtual int onReadMsg(uint64_t sessionId, int bytesNum) ;

    virtual int onWriteBytes(uint64_t sessionId, int len) ;

private:

    volatile bool isRunning = false;
    volatile int maxWorker = 4;
    volatile std::vector<PollerData*> pollerData;
    uint64_t lisSock = 0;
    std::thread listenThread;

};


#endif //MAIN_SPIDER_H
