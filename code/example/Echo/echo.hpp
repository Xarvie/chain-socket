#ifndef ECHO_H
#define ECHO_H

#include "chain-socket.h"
#include <iostream>

static int64_t x = 0;
#define TEST_IPv6 "fe80::2d99:3cb7:d1e7:24fb%16"
#define TEST_IPv4 "127.0.0.1"
#define TEST_PORT 6010
class Server : public MsgHub {
public:
	int genTask(ConnectionTask *task) override {
		q->enqueue(*task);
		return 0;
	}

	int onTask() override {
		ConnectionTask task;
		if (q->try_dequeue(task)) {
			if ((Session *) this->sessions[task.sessionId] == task.sessPtr) {
				if (task.type == SEND_TO) {
					if (task.len > sizeof(SendTask)) {
						SendTask *t = (SendTask *) task.data;
						this->sendTo(task.sessPtr, t->data, t->len);
					}

				}
				if (task.type == CLOSE_SESSION) {
					if (task.len > sizeof(CloseTask)) {
						CloseTask *t = (CloseTask *) task.data;
						this->closeSession(task.sessionId, t->reason);
					}
				}
				if (task.type == CONNECT_TO) {
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


	int onAccept(Session *lconnp, Session *aconnp) override {
		std::cout << "server: "<< lconnp->sessionId << "onAccept :" << aconnp->sessionId << std::endl;

		this->postRecv(aconnp);

		return 0;
	}

	int onRecv(Session *connp, int len) override {
		char *s = (char *) malloc(connp->readBuffer.size + 1);
		s[connp->readBuffer.size] = 0;
		memcpy(s, connp->readBuffer.buff, connp->readBuffer.size);

		std::cout << connp->sessionId << " onRecv :" << s << std::endl;

		connp->readBuffer.erase(len);
		x+=len;

		this->sendTo(connp, (unsigned char *) "hello", 5);

		this->closeSession(connp->sessionId, CT_NORMAL);
		this->connectTo(TEST_IPv6, TEST_PORT, 5000000);
		return 0;
	}

	int onSend(Session *connp, int len) override {
		//std::cout << "send bytes :" << connp->fd << len << std::endl;
		return 0;
	}
	int onDisconnect(Session* connp, int type) override {
		std::cout << "onDisconnect :" << connp->sessionId << "type:" << type << std::endl;
	}
	int onConnect(Session *connp, std::string ip, int port) override {
		std::cout << "onConnect :" << ip << ":" << port << " sid" << connp->sessionId << std::endl;
		this->postRecv(connp);
		this->sendTo(connp, (unsigned char *) "hello", 5);

		return 0;
	}

	int onConnectFailed(std::string ip, int port, int type) override {
		std::cout << "connected Failed :" << type << ip << ":" << port << std::endl;
		return 0;
	}
};

#endif