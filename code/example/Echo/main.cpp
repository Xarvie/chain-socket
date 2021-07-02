#include "echo.hpp"

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