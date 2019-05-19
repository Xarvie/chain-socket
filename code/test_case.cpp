#include "chain-net/Poller.h"
#include <mutex>

uint64_t count = 0;

class Server : public Poller {
public:
    std::mutex lock;

    explicit Server(int port, int maxWorker = 4) : Poller(port, maxWorker) {

    }

    virtual int onAccept(Session &conn, const Addr &addr) override {

        //lock.lock();
        //std::cout << "accept SessionID:" << conn.sessionId << std::endl;
        //lock.unlock();
        return 0;
    }

    int onReadMsg(Session &conn, int bytesNum) override {
        Msg msg;
        msg.buff = conn.readBuffer.buff;
        msg.len = conn.readBuffer.size;
        int online = 0;

        if (++count % 100'000 == 0) {
            for (int i = 0; i < this->maxWorker; i++) {
                online += this->workerVec[i]->onlineSessionSet.size();
            }
            std::cout << count / 10000 << "-online:" << online << std::endl;
        }
        this->sendMsg(conn, msg);

        return bytesNum;
    }

    int onWriteBytes(Session &conn, int len) override {
        return 0;
    }

    int onDisconnect(Session &conn, int type) override {
        lock.lock();
        int online = 0;
        for (int i = 0; i < this->maxWorker; i++) {
            online += this->workerVec[i]->onlineSessionSet.size();
        }
        std::cout << "disconnect SessionID:" << conn.sessionId << " type:" << type << " online:" << online << std::endl;
        lock.unlock();
        return 0;
    }
};

int main() {
    Server sx(9876, 1);
    sx.run();
    return 0;
}