#include "SystemReader.h"

#if defined(IOCP_BACKEND)

#include "IOCPPoller.h"
#include "Buffer.h"
#include "NetStruct.h"
#include "SystemInterface.hpp"
#include <mswsock.h>
#include <ws2tcpip.h>
#include "xmalloc.h"
#include "Log.h"
#include <iostream>


static GUID ax_guid = WSAID_ACCEPTEX;
static GUID as_guid = WSAID_GETACCEPTEXSOCKADDRS;

LPFN_ACCEPTEX lpfn_AcceptEx;
LPFN_GETACCEPTEXSOCKADDRS lpfn_GetAcceptExSockAddrs;


typedef struct {
    OVERLAPPED overlapped;
    SOCKET self;
    SOCKET accept;
    WSABUF wsa_buf;
    RWMOD action;

} Pre_IO_Context, *LP_Pre_IO_Context;

struct IocpData {
    Session conn;
    sockaddr_in peer_addr;
    LP_Pre_IO_Context io_ctx_r;
    LP_Pre_IO_Context io_ctx_s;

    HANDLE x;

	void init() {
		conn.init();
		io_ctx_r = (LP_Pre_IO_Context) xmalloc(sizeof(Pre_IO_Context));
		memset(io_ctx_r, 0, sizeof(Pre_IO_Context));
		io_ctx_s = (LP_Pre_IO_Context) xmalloc(sizeof(Pre_IO_Context));
		memset(io_ctx_s, 0, sizeof(Pre_IO_Context));
	}

    void reset() {
		destroy();
		init();
    }

    void destroy() {
        conn.destroy();
        xfree(io_ctx_r);
        xfree(io_ctx_s);
    }
};

extern int initIocp();


int Poller::post_accept_ex(Session *connp, uint64_t listenFd) {
    if (connp == NULL) {
        return 0;
    }
    IocpData &conn = *(IocpData *) connp;
    auto &io_ctx_r = conn.io_ctx_r;
    SOCKET &listener = listenFd;
    io_ctx_r->self = listener;

    ZeroMemory(&(io_ctx_r->overlapped), sizeof(OVERLAPPED));
    io_ctx_r->action = RWMOD::ClientIoAccept;


    io_ctx_r->accept = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (INVALID_SOCKET == io_ctx_r->accept) {
        std::cout << "WSASocket() failed." << std::endl;
        return -1;
    }
    static char b[] = {0};
    static WSABUF tmpBuf = {0, b};
    //this->peerAddrMap[io_ctx_r->accept / 4] = 0;

    if (0 == lpfn_AcceptEx(
            listener,
            io_ctx_r->accept,
            &this->peerAddrMap[connp->sessionId],
            0,//io_ctx->wsa_buf.len - (sizeof(SOCKADDR_IN) + 16) * 2,

            sizeof(SOCKADDR_IN) + 16,//sizeof(SOCKADDR_IN) + 16,//sizeof(SOCKADDR_IN) + 16,
            sizeof(SOCKADDR_IN) + 16,//sizeof(SOCKADDR_IN) + 16,//sizeof(SOCKADDR_IN) + 16,

            NULL,
            &(io_ctx_r->overlapped)
    )) {
        if (WSA_IO_PENDING != WSAGetLastError()) {
            std::cout << "LPFN_ACCEPTEX() failed. last error: " << WSAGetLastError() << std::endl;
            return -1;
        }
    }
    return 0;
}

int Poller::do_accept(int connId) {
    if (this->sessions[connId] == NULL) {
        return 0;
    }
    IocpData &listenConn = *this->sessions[connId];


    SOCKET listener = listenConn.io_ctx_r->self;
    memset(&this->peerAddrMap[connId], 0, 64);
    if (-1 == setsockopt(listenConn.io_ctx_r->accept, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *) &listener,
                         sizeof(SOCKET))) {
        std::cout << "setsockopt(SO_UPDATE_ACCEPT_CONTEXT) failed. error: " << WSAGetLastError() << std::endl;
        return -1;
    }

    std::cout << "do_accept, client socket: " << listenConn.io_ctx_r->accept << std::endl;


    return 0;
}

Poller::Poller() {

}

Session *Poller::initNewSession(uint64_t fd, Session::Type type) {
    //onaccept
    //onconnect direct
    //onconnect by timer
    //listern to
    int i = 0;
    Session* p = NULL;
    for(; i < this->maxId; this->curId = (this->curId + 1) % this->maxId, i++) {
        auto &data = sessions[this->curId];
        if (data != NULL)
            continue;
        data = (IocpData *) xmalloc(sizeof(IocpData));
        data->init();
        data->conn.sessionId = this->curId;
        data->conn.fd = (uint64_t) fd;
        p = (Session*)data;
        data->conn.type = type;
        this->curId = (this->curId + 1) % this->maxId;
        break;
    }
    if (p == NULL) {
        return NULL;
    }

    IocpData &data = *(IocpData*)p;
    setSockNonBlock(fd);

    if (NULL == (CreateIoCompletionPort((HANDLE) fd, this->iocps[0], (ULONG_PTR) p->sessionId, 0))) {
        std::cout << "CreateIoCompletionPort() failed. error: " << GetLastError() << std::endl;
        return NULL;
    }

    ZeroMemory (&(data.io_ctx_r->overlapped), sizeof(OVERLAPPED));
    data.io_ctx_r->wsa_buf.len = 0;
    data.io_ctx_r->wsa_buf.buf = NULL;
    data.io_ctx_r->action = RWMOD::ClientIoRead;

    ZeroMemory(&(data.io_ctx_s->overlapped), sizeof(OVERLAPPED));
    data.io_ctx_s->wsa_buf.len = 0;
    data.io_ctx_s->wsa_buf.buf = NULL;
    data.io_ctx_s->action = RWMOD::ClientIoWrite;

    data.io_ctx_r->accept = fd;
    data.io_ctx_s->accept = fd;

    return p;
}

int Poller::init(Log* loggerp) {
    this->logger = loggerp;
    if(this->logger == NULL)
        this->logger = new Log();
    this->maxId = 1000;
    this->curId = 0;
    sessions.resize(this->maxId);

    for (int i = 0; i < this->maxId; i++) {
        sessions[i] = NULL;
    }


    this->iocps.resize(1);
    HANDLE &iocp = this->iocps[0];
    if (-1 == initIocp()) {
        std::cout << "init failed." << std::endl;
        return -1;
    }
    if ((iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)) == NULL) {
        std::cout << "CreateIoCompletionPort Failed，err: " << GetLastError() << std::endl;
        return -1;
    }
    this->isRun = true;

    return 0;
}

void Poller::reset() {
    iocps.clear();
    listenFds.clear();
    for (int i = 0; i < sizeof(sessions) / sizeof(sessions[0]); i++) {
        if (sessions[i]) {
            sessions[i]->reset();
        }
    }
}

int Poller::stop() {
    if (isRun == false)
        return 0;
    CloseHandle(this->iocps[0]);
    for (auto &E: this->listenFds) {
        closesocket(E);
    }
    for (auto &E: this->sessions) {
        E->conn.readBuffer.destroy();
        E->conn.writeBuffer.destroy();
        if (E != NULL) {
            xfree(E);
            E = NULL;
        }
    }
//TODO free resource
    this->iocps[0] = NULL;
    //WSACleanup();
    return 0;
}

int initIocp(void) {
    SYSTEM_INFO sys_info;
    WSADATA wsa_data;

    DWORD ret;
    SOCKET s;


    GetSystemInfo(&sys_info);

//    printf("System memery page size: %d \n", sys_info.dwPageSize);
//    printf("System cpus: %d \n", sys_info.dwNumberOfProcessors);


    if ((ret = WSAStartup(0x0202, &wsa_data)) != 0) {
        std::cout << "WSAStartup() failed. error: " << ret << std::endl;
        return -1;
    }

    if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2) {
        std::cout << "Require Windows Socket Version 2.2 Error!" << std::endl;
        WSACleanup();
        return -1;
    }


    s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (s == -1) {
        std::cout << "socket() failed." << std::endl;
        return -1;
    }


    if (-1 == WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &ax_guid, sizeof(GUID),
                       &lpfn_AcceptEx, sizeof(LPFN_ACCEPTEX), &ret, NULL, NULL)) {
        std::cout << "WSAIoctl(LPFN_ACCEPTEX) failed." << std::endl;
        return -1;
    }

    if (-1 == WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &as_guid, sizeof(GUID),
                       &lpfn_GetAcceptExSockAddrs, sizeof(LPFN_GETACCEPTEXSOCKADDRS),
                       &ret, NULL, NULL)) {
        std::cout << "WSAIoctl(LPFN_GETACCEPTEXSOCKADDRS) failed." << std::endl;
        return -1;
    }


    if (-1 == closesocket(s)) {
        std::cout << "closesocket() failed." << std::endl;
        return -1;
    }

    return 0;
}

int Poller::loopOnce(int time) {
    HANDLE iocp = (HANDLE) this->iocps[0];
    OVERLAPPED_ENTRY entries[1024];
    DWORD completion_count;
    DWORD bytes = 0;


    BOOL r = GetQueuedCompletionStatusEx(iocp,
                                         entries,
                                         1024,
                                         &completion_count,
                                         time,
                                         FALSE);
    if (!r) {
        DWORD err = GetLastError();
        if (err == ERROR_ABANDONED_WAIT_0) {
            // Completion port closed while inside get status.
            return 0;
        } else if (err == ERROR_INVALID_HANDLE) {
            // Completion port closed before call to get status.
            return 0;
        } else if (WAIT_TIMEOUT == err) {
            return 0;
        } else if (ERROR_NETNAME_DELETED == err || ERROR_OPERATION_ABORTED == err) {
            std::cout << "The socket was closed. error: " << err << std::endl;
            return 0;
        }
    }


    for (unsigned i = 0; i < completion_count; ++i) {
        LP_Pre_IO_Context io_ctx = (LP_Pre_IO_Context) entries[i].lpOverlapped;
        Session *connData = (Session*)sessions[entries[i].lpCompletionKey];

        int id = entries[i].lpCompletionKey;

        // auto & connData = sessions[connDataId];
        if (connData == NULL) {
            std::cout << "NULL" << std::endl;
            continue;
        }

        //Session*& connp = (Session*)&connData->conn;
        bytes = entries[i].dwNumberOfBytesTransferred;
        if (NULL == io_ctx) {
            std::cout << "GetQueuedCompletionStatus() returned no operation" << std::endl;
            continue;
        }

        int listenId = io_ctx->self / 4;

        int clientId = io_ctx->accept / 4;

        switch (io_ctx->action) {
            case RWMOD::ClientIoAccept: {
                SOCKET client = (int64_t) io_ctx->accept;


                do_accept(listenId);
                IocpData *aData = (IocpData *) this->initNewSession(client, Session::Type::ACCEPT);
                if (aData == NULL) {
                    this->logger->output("Accept Session exhausted", 0);
                    closeSocket(client);
                    continue;
                }
                int aId = aData->conn.sessionId;

                this->onAccept((Session*)sessions[id], (Session*)sessions[aId]);
                if (connData == NULL)
                    break;
                for (;;) {
                    auto sdAccept = WSAAccept(io_ctx->self, nullptr, nullptr, nullptr, 0);
                    if (sdAccept == SOCKET_ERROR) {

                        //std::cout << "accept NONE." << GetLastError() << std::endl;
                        break;
                    }
                    IocpData *aData = (IocpData *) this->initNewSession(sdAccept, Session::Type::ACCEPT);

                    if (aData == NULL) {
                        this->logger->output("Accept Session exhausted", 0);
                        closeSocket(client);
                        continue;
                    }
                    aId = aData->conn.sessionId;

                    this->onAccept((Session*)sessions[id], (Session*)sessions[aId]);

                }
                post_accept_ex((Session*)sessions[id], io_ctx->self);
                break;
            }


            case RWMOD::ClientIoRead: {
                do_recv((Session*)sessions[id], bytes);
                break;
            }

            case RWMOD::ClientIoWrite: {
                do_send((Session*)sessions[id], bytes);

                break;
            }


            default:
                //               printf("ERROR: No action match! \n");
                break;
        }
    }
    return 0;
}

Session *Poller::getSession(int connId) {
    return (Session*)this->sessions[connId];
}

int Poller::postRecv(Session *connp) {
    if (connp == NULL)
        return 0;

    DWORD flags = 0;
    DWORD bytes = 0;
    DWORD err_no;
    int ret;
    IocpData &data = *(IocpData *) connp;
    auto &io_ctx = data.io_ctx_r;
    ZeroMemory(&(io_ctx->overlapped), sizeof(OVERLAPPED));
    io_ctx->action = RWMOD::ClientIoRead;


    static CHAR b[] = {0};
    static WSABUF tmpBuf = {0, b};
    ret = WSARecv(io_ctx->accept, &(tmpBuf), 1, &bytes, &flags, &(io_ctx->overlapped), NULL);

    err_no = WSAGetLastError();
    if (-1 == ret && WSA_IO_PENDING != err_no) {
        if (err_no == WSAEWOULDBLOCK) std::cout << "WSARecv() not ready" << std::endl;

        std::cout << "WSARecv() faild. client socket: " << io_ctx->accept << ", error: " << err_no << std::endl;

        return -1;
    }

    return 0;
}


int Poller::do_recv(Session *connp, int len) {
    if (connp == NULL)
        return 0;
    IocpData &data = *(IocpData *) connp;
    int id = connp->sessionId;
    data.conn.readBuffer.alloc();
    int readNum = 0;

    while (true) {


        int ret = recv(connp->fd, (char *) data.conn.readBuffer.buff + data.conn.readBuffer.size,
                       data.conn.readBuffer.capacity - data.conn.readBuffer.size, 0);

        if (ret > 0) {
            data.conn.readBuffer.size += ret;
            data.conn.readBuffer.alloc();
            readNum+=ret;

        } else if (ret == 0) {
            closeSession(connp->sessionId, CT_READ_ZERO);
            //std::cout << "read 0" << std::endl;
            return 0;
        } else {
            if (IsEagain()) {
                if(readNum>0)
                {
                    this->onRecv(connp, readNum);
                    if((Session*)sessions[id] == NULL)
                    {
                        return 0;
                    }
                }


                postRecv(connp);
                return 0;
                //postRecv(connId);
            } else {
                //std::cout << getSockError() << std::endl;
                this->logger->output("read ERROR", 0);
                closeSession(connp->sessionId, CT_READ_ERROR);
                return 0;
            }

        }

    }


    static CHAR b[] = {0};
    static WSABUF tmpBuf = {0, b};

    data.io_ctx_r->wsa_buf.len = 0;
    data.io_ctx_r->wsa_buf.buf = NULL;
    data.io_ctx_r->action = RWMOD::ClientIoRead;

    return 0;
}

int Poller::onAccept(Session *lconnp, Session *aconnp) {
    //postRecv(acceptConn);
    return 0;
}

int Poller::onRecv(Session *connp, int len) {
    if (connp == NULL)
        return 0;
    connp->readBuffer.erase(len);
    return 0;
}

int Poller::sendTo(Session *connp, unsigned char *buf, int len) {
    if (connp == NULL)
        return 0;
    if (connp->writeBuffer.size > 0) {
        connp->writeBuffer.push_back(len, buf);
        return 0;
    }
    connp->writeBuffer.push_back(len, buf);

    postSend(connp);
    return 0;
}

int Poller::getPeerIpPort(int connId, std::string ip, int port) {
    if (sessions[connId] == NULL)
        return 0;
    IocpData &data = *sessions[connId];

    if (data.conn.type == Session::Type::NONE) {
        SOCKADDR_IN *remote_sock_addr = (SOCKADDR_IN *) &this->peerAddrMap[connId];
        return 0;
    } else {
        struct sockaddr_in sockAddrx[2];
        memset(&sockAddrx, 0, sizeof(sockAddrx));
        int nSockAddrLen = sizeof(sockAddrx);

        if (SOCKET_ERROR == getpeername(connId * 4, (sockaddr *) &sockAddrx, &nSockAddrLen)) {
            std::cout << "error getpeername" << std::endl;
            return 0;
        }
        return 0;
    }

    return 0;
}

int Poller::getLocalIpPort(int connId, std::string ip, int port) {
    struct sockaddr_in sockAddrx[2];
    memset(&sockAddrx, 0, sizeof(sockAddrx));
    int nSockAddrLen = sizeof(sockAddrx);

    if (SOCKET_ERROR == getsockname(connId * 4, (sockaddr *) &sockAddrx, &nSockAddrLen)) {
        std::cout << "error getsockname" << std::endl;
        return 0;
    }


    return 0;
}

Session *Poller::session(int connId) {
    if (this->sessions[connId] == NULL)
        return NULL;
    return &this->sessions[connId]->conn;
}

int Poller::postSend(Session *connp) {
    if (connp == NULL)
        return 0;
    IocpData &data = *(IocpData *) connp;
    DWORD flags = 0;
    DWORD bytes = 0;
    DWORD err_no;
    int ret;
    int id = connp->sessionId;
    auto &io_ctx = data.io_ctx_s;
    //io_ctx->wsa_buf.buf = (char *) data.conn.writeBuffer.buff;
    //io_ctx->wsa_buf.len = data.conn.writeBuffer.size;
    //io_ctx->overlapped.hEvent = WSACreateEvent();
    io_ctx->action = RWMOD::ClientIoWrite;


    if (data.conn.writeBuffer.size == 0)
        return 0;


    while (data.conn.writeBuffer.size > 0) {
        int tmpRet = send(connp->fd, (char *) data.conn.writeBuffer.buff, data.conn.writeBuffer.size, 0);
        if (tmpRet > 0) {
            data.conn.writeBuffer.erase(tmpRet);
            this->onSend(connp, tmpRet);
            if (this->sessions[id] == NULL)
                return 0;
        } else if (tmpRet == 0) {
            std::cout << "write：ret = 0" << std::endl;
        } else if (tmpRet < 0) {
            if (IsEagain()) {
                static char b[] = {0};
                static WSABUF tmpBuf = {0, b};

                ret = WSASend(io_ctx->accept, &tmpBuf, 1, &bytes, 0, &(io_ctx->overlapped), NULL);
                err_no = WSAGetLastError();

                if (-1 == ret && err_no != WSA_IO_PENDING) {
                    std::cout << "WSASend() faild. error: " << err_no << std::endl;
                    this->closeSession(connp->sessionId, CT_WSEND_ERROR1);
                    //WSAResetEvent(io_ctx->overlapped.hEvent);
                    return -1;
                }

                if (err_no == WSA_IO_PENDING) {
                    std::cout << "WSASend() posted. bytest: " << bytes << " err: " << err_no << std::endl;
                }

                std::cout << "write：EAGIN ret < 0" << std::endl;
            } else {
                this->closeSession(connp->sessionId, CT_WSEND_ERROR2);
                std::cout << "write： ret < 0" << std::endl;
            }

            break;
        }

    }







    /*
    WSAWaitForMultipleEvents(1, &io_ctx->overlapped.hEvent, TRUE, INFINITE, TRUE);
    printf("WSAWaitForMultipleEvents() failed. err: %d\n", WSAGetLastError());

    WSAGetOverlappedResult(io_ctx->accept, &io_ctx->overlapped, &io_ctx->bytes_send, FALSE, &flags);
    printf("WSAGetOverlappedResult() send bytes: %d\n", io_ctx->bytes_send);

    WSAResetEvent(io_ctx->overlapped.hEvent);
    printf("WSAResetEvent() err: %d\n", WSAGetLastError());
    */

    return 0;
}

int Poller::onSend(Session *connp, int len) {


    return 0;
}

int Poller::onDisconnect(Session *connp, int type) {

    return 0;
}

int Poller::closeSession(int connId, int type) {
    if (sessions[connId] == NULL)
        return 0;
    IocpData &data = *sessions[connId];
    closeSocket(sessions[connId]->conn.fd);
    data.destroy();
    //CloseHandle(data.x);
    shutdown(sessions[connId]->conn.fd, SD_BOTH);
    memset(&data, 0, sizeof(IocpData));
    xfree(sessions[connId]);
    sessions[connId] = NULL;
    return 0;
}

int Poller::do_send(Session *connp, int len) {
    if (connp == NULL)
        return 0;
    if (connp->writeBuffer.size > 0)
        postSend(connp);
    //shutdown(io_ctx->accept, SD_BOTH);
    return 0;
}


bool Poller::listenTo(int port) {
    SOCKET listener = 0;
    HANDLE &iocp = this->iocps[0];
    LINGER linger;
    SOCKADDR_IN inet_addr;
    int opt_val = 1;
    int opt_len = sizeof(int);
    int Max_Threads = 1;
    if ((listener = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET) {
        std::cout << "WSASocket() failed. error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return -1;
    }

    unsigned long ul = 1;
    int ret = ioctlsocket(listener, FIONBIO, (unsigned long *) &ul);
    if (ret == SOCKET_ERROR) {
        std::cout << "err: ioctlsocket" << std::endl;
        return -1;
    }


    if (-1 == setsockopt(listener, SOL_SOCKET, SO_KEEPALIVE, (const char *) &opt_val, opt_len)) {
        std::cout << "setsockopt(SO_KEEPALIVE) failed." << std::endl;
        closesocket(listener);
        WSACleanup();
        CloseHandle(iocp);
        return -1;
    }

    // closesocket: return immediately and send RST
    linger.l_onoff = 1;
    linger.l_linger = 0;
    if (-1 == setsockopt(listener, SOL_SOCKET, SO_LINGER, (char *) &linger, sizeof(linger))) {
        std::cout << "setsockopt(SO_LINGER) failed." << std::endl;
        closesocket(listener);
        WSACleanup();
        CloseHandle(iocp);
        return -1;
    }

    // Windows only support SO_REUSEADDR
    if (-1 == setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char *) &opt_val, opt_len)) {
        std::cout << "setsockopt(SO_REUSEADDR) failed." << std::endl;
        closesocket(listener);
        WSACleanup();
        CloseHandle(iocp);
        return -1;
    }


//    if (INVALID_HANDLE_VALUE == CreateIoCompletionPort((HANDLE) listener, iocp, 0, 0)) {
//        printf("CreateIoCompletionPort(listener) failed.");
//        closesocket(listener);
//        WSACleanup();
//        CloseHandle(iocp);
//        return -1;
//    }


    inet_addr.sin_family = AF_INET;
    inet_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    inet_addr.sin_port = htons(port);
    if (SOCKET_ERROR == bind(listener, (PSOCKADDR) &inet_addr, sizeof(inet_addr))) {
        std::cout << "bind() failed." << std::endl;
        closesocket(listener);
        WSACleanup();
        CloseHandle(iocp);
        return -1;
    }


    if (SOCKET_ERROR == listen(listener, SOMAXCONN)) {
        std::cout << "listen() failed." << std::endl;
        closesocket(listener);
        WSACleanup();
        CloseHandle(iocp);
        return -1;
    }


    std::cout << "Listen on prot:" << port << std::endl;

    this->listenFds.push_back(listener);

    IocpData *data = (IocpData *) this->initNewSession(listener, Session::Type::LISTEN);
    if (data == NULL) {
        this->logger->output("Listen Session exhausted", 0);
        return false;
    }

    Session *connp = &data->conn;
    post_accept_ex(connp, listener);

    return true;
}

int Poller::tryConnect(const char *ip, const int port, const int timeout, uint64_t *sockFd) {
    int64_t connectFd = socket(AF_INET, SOCK_STREAM, 0);

    int nRcvBufferLen = 32 * 1024 * 1024;
    int nSndBufferLen = 32 * 1024 * 1024;
    int nLen = sizeof(int);

    setSockOpt(connectFd, SOL_SOCKET, SO_SNDBUF, &nSndBufferLen, nLen);
    setSockOpt(connectFd, SOL_SOCKET, SO_RCVBUF, &nRcvBufferLen, nLen);

    if (timeout > 0)
        setSockNonBlock(connectFd);

    struct sockaddr_in targetAddr;
    targetAddr.sin_family = AF_INET;
    targetAddr.sin_port = htons(port);
    targetAddr.sin_addr.s_addr = inet_addr(ip);


    int ret = connect(connectFd, (struct sockaddr *) &targetAddr, sizeof(targetAddr));
    if (ret < 0) {
        /* if connect error */

        if (!IsConnecting()) {
            std::cout << getSockError() << std::endl;
            std::cout << "\nConnecting Failed! " << getSockError() << std::endl;
            closeSocket(connectFd);
            return -1;
        } else {
            if (sockFd) {
                *sockFd = connectFd;
            }
            return 0;
        }
    } else {

    }
    if (timeout == 0)
        setSockNonBlock(connectFd);
    if (sockFd) {
        *sockFd = connectFd;
    }
    return 1;
}


#endif