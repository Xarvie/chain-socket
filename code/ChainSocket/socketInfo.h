//
// Created by Xarvie on 1/16/2020.
//

#ifndef CHAIN_SOCKET_SOCKETINFO_H
#define CHAIN_SOCKET_SOCKETINFO_H

#include <string>

struct sockInfo {
    int port;
    std::string ip;
    uint64_t fd;
    int ret;
    char task;/*1:listen 2:connect 3:disconnect*/
    char event;/*1 listen 2:connect*/
};
#endif //CHAIN_SOCKET_SOCKETINFO_H
