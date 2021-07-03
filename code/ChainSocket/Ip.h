//
// Created by ftp on 7/3/2021.
//

#ifndef CHAIN_SOCKET_IP_H
#define CHAIN_SOCKET_IP_H


#include <string>
#include <winsock.h>


class IP {
public:
	enum Version {
		V4 = 2, //AF_INET
		V6 = 23 //AF_INET
	};

	void fromString(std::string ip, std::string port, IP::Version ipv);

	int ai_flags;
	int ai_family;
	int ai_socktype;
	int ai_protocol;
	size_t ai_addrlen;
	std::string ai_canonname;
	struct sockaddr ai_addr;

};


#endif //CHAIN_SOCKET_IP_H
