#include "WebSocketPoller.hpp"

int main1() {
	WebSocketPoller server;
	server.init(IP::V6);
	server.listenTo(6010);
	while (true)
		server.waitMsg();
	return 0;
}