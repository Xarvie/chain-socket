#include "WebSocketPoller.hpp"
#include <iostream>

static int64_t x = 0;

class Server : public MsgHub {
public:
    virtual int genTask(ConnectionTask *task) override {
        q->enqueue(*task);
        return 0;
    }

    virtual int onTask() override {
        ConnectionTask task;
        if (q->try_dequeue(task)) {
            if ((Session*)this->sessions[task.sessionId] == task.sessPtr) {
                if (task.type == 1) {
                    if (task.len > sizeof(SendTask)) {
                        SendTask *t = (SendTask *) task.data;
                        this->sendTo(task.sessPtr, t->data, t->len);
                    }

                }
                if (task.type == 2) {
                    if (task.len > sizeof(CloseTask)) {
                        CloseTask *t = (CloseTask *) task.data;
                        this->closeSession(task.sessionId, t->reason);
                    }
                }
                if (task.type == 3) {
                    if (task.len > sizeof(ConnectTask)) {
                        ConnectTask *addr = (ConnectTask *) task.data;
                        this->connectTo((char *) task.data, addr->port, addr->timeOut);
                    }

                }
                if (task.type == 0) {
                    std::abort();
                }
            }
        } else {

        }
        return 0;
    }


    virtual int onAccept(Session *lconnp, Session *aconnp) override {
        std::cout << "onAccept :" << aconnp->fd << std::endl;

        this->postRecv(aconnp);

        return 0;
    }

    virtual int onRecv(Session *connp, int len) override {
        char *s = (char *) malloc(connp->readBuffer.size + 1);
        s[connp->readBuffer.size] = 0;
        memcpy(s, connp->readBuffer.buff, connp->readBuffer.size);

        std::cout << connp->sessionId << " onRecv :" << s << std::endl;

        connp->readBuffer.erase(len);
        x++;


        this->sendTo(connp, (unsigned char *) "hello", 5);

        if (x > 20) {
            //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            int needConnect = 1;
            if (connp->type == Session::Type::ACCEPT) {
                needConnect = 1;
            }
            this->closeSession(connp->sessionId, CT_NORMAL);

            if (needConnect == 1)
                this->connectTo("127.0.0.1", 6010, 5000000);
        }


        return 0;
    }

    virtual int onSend(Session *connp, int len) override {
        //std::cout << "send bytes :" << connp->fd << len << std::endl;
        return 0;
    }

    virtual int onConnect(Session *connp, std::string ip, int port) override {
        std::cout << "onConnect :" << ip << ":" << port << std::endl;
        this->postRecv(connp);
        this->sendTo(connp, (unsigned char *) "hello", 5);

        return 0;
    }

    virtual int onConnectFailed(std::string ip, int port, int type) override {
        std::cout << "connected Failed :" << type << ip << ":" << port << std::endl;
        return 0;
    }
};

class Server2 : public WebSocketPoller {
public:
    virtual int onReadMsg(Session &conn, unsigned char *curBuffer, unsigned int MsgSize) override {
        std::cout << "onReadMsg" << std::endl;
        return 0;
    }

    virtual int onHandShake(Session &conn) override {
        std::cout << "onHandShake" << std::endl;
        return 0;
    }
};

#include <unordered_map>
#include <limits>
#include "Timer.h"
#include <cstring>

int main() {
    Server server;
    server.init(NULL);
    server.listenTo(6010);
    for (int i = 0; i < 3; i++)
        server.connectTo("127.0.0.1", 6010, 5000000);
    while (true)
        server.waitMsg();
    return 0;
}