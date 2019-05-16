#include "chain-net/Poller.h"
#include <mutex>

class Server : public Poller {
public:
    std::mutex lock;
    explicit Server(int port, int maxWorker = 4) : Poller(port, maxWorker) {

    }

    virtual int onAccept(Session &conn, const Addr &addr) override {

        lock.lock();
        std::cout << "accept SessionID:" << conn.sessionId << std::endl;
        lock.unlock();
        return 0;
    }

    int onReadMsg(Session &conn, int bytesNum) override {
        Msg msg;
        msg.buff = conn.readBuffer.buff;
        msg.len = conn.readBuffer.size;

        this->sendMsg(conn, msg);

        return bytesNum;
    }

    int onWriteBytes(Session &conn, int len) override {
        return 0;
    }

    int onDisconnect(Session &conn) override {
        lock.lock();
        std::cout << "disconnect SessionID:" << conn.sessionId << std::endl;
        lock.unlock();
        return 0;
    }
};

int main() {
    Server sx(9876, 8);
    sx.run();
    return 0;
}