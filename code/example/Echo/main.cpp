#include "echo.hpp"

int main() {
	Server server;
	server.init(IP::V6);
//	server.init(IP::V4);
	server.listenTo(TEST_PORT);
	for (int i = 0; i < 1; i++)
		server.connectTo(TEST_IPv6, TEST_PORT, 5000000);
//		server.connectTo(TEST_IPv4, TEST_PORT, 5000000);
	while (true)
		server.waitMsg();
	return 0;
}



