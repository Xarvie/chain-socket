#ifndef ECHO_H
#define ECHO_H

#include "chain-socket.h"
#include <iostream>

static int64_t x = 0;

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
		std::cout << "onAccept :" << aconnp->fd << std::endl;

		this->postRecv(aconnp);

		return 0;
	}

	int onRecv(Session *connp, int len) override {
		char *s = (char *) malloc(connp->readBuffer.size + 1);
		s[connp->readBuffer.size] = 0;
		memcpy(s, connp->readBuffer.buff, connp->readBuffer.size);

		std::cout << connp->sessionId << " onRecv :" << s << std::endl;

		connp->readBuffer.erase(len);
		x++;


		this->sendTo(connp, (unsigned char *) "hello", 5);

		if (x > 20) {
			this->closeSession(connp->sessionId, CT_NORMAL);
			this->connectTo("127.0.0.1", 6010, 5000000);
		}
		return 0;
	}

	int onSend(Session *connp, int len) override {
		//std::cout << "send bytes :" << connp->fd << len << std::endl;
		return 0;
	}

	int onConnect(Session *connp, std::string ip, int port) override {
		std::cout << "onConnect :" << ip << ":" << port << std::endl;
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