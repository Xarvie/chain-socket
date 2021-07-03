//
// Created by ftp on 7/3/2021.
//

#include "Ip.h"
#include <ws2tcpip.h>

void IP::fromString(std::string ip, std::string port, IP::Version ipv) {
	//addrinfo结构体是为了消除IPv6协议与IPv4协议之间的差异，编制统一的程序而追加的结构。并且允许多个IPv4地址或IPv6地址链成链表
	struct addrinfo hints, //服务器端地址信息
	*result = NULL; //若有多个地址，res是地址信息链表指针
	memset(&hints, 0, sizeof(hints));//如果没有这句话就会出现绑定错误,也就是在调用getaddrinfo()之前该参数必须清0
	hints.ai_family = ipv;  //地址簇,这里指定是ipv6协议,其值可以是 AF_INET:ipv4, AF_INET6:ipv6
	hints.ai_socktype = SOCK_STREAM;  //套接字类型,这里是流式,其值可以是 SOCK_STREAM:流式, SOCK_DGRAM:数据报, SOCK_RAW:原始套接字
	hints.ai_protocol = IPPROTO_TCP;  //传输层协议,这里是TCP协议,其值可以是: IPPROTO_TCP:TCP, IPPROTO_UDP:UDP, 若为0系统根据套接字类型自动选择

	// Ip cim es port beallitas
	int iResult = getaddrinfo(ip.c_str(), port.c_str(), &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
	}
	ai_flags = result->ai_flags;
	ai_family = result->ai_family;
	ai_socktype = result->ai_socktype;
	ai_protocol = result->ai_protocol;
	ai_addrlen = result->ai_addrlen;
	ai_canonname = result->ai_canonname;
	ai_addr = *result->ai_addr;

	freeaddrinfo(result);
}