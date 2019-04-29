#include "SystemReader.h"

#if defined(OS_WINDOWS) && !defined(SELECT_SERVER)

#include "IOCPPoller.h"

Poller::Poller(int port, int threadsNum) {
    this->port = port;
    this->maxWorker = threadsNum;
    sessions.resize(CONN_MAXFD);
    for (int i = 0; i < CONN_MAXFD; i++) {
        sessions[i] = (Session*)xmalloc(sizeof(Session));
        sessions[i]->reset();
        sessions[i]->sessionId = (uint64_t) i;
    }
}

void Poller::sendMsg(uint64_t sessionId, const Msg &msg) {
    if (sessions[sessionId]->writeBuffer.size > 0) {
        sessions[sessionId]->writeBuffer.push_back(msg.len, msg.buff);
    } else {
        sessions[sessionId]->writeBuffer.push_back(msg.len, msg.buff);
        this->continueSendMsg(sessionId);
    }
}

void Poller::workerThreadCB(int pollIndex) {

    HANDLE hIOCP = (HANDLE) iocps[pollIndex];
    BOOL bSuccess = FALSE;
    int nRet = 0;
    LPWSAOVERLAPPED lpOverlapped = nullptr;
    PER_SOCKET_CONTEXT *lpPerSocketContextnullptr = nullptr;
    PER_SOCKET_CONTEXT *lpPerSocketContext = nullptr;
    WSABUF buffRecv;
    WSABUF buffSend;
    DWORD dwRecvNumBytes = 0;
    DWORD dwSendNumBytes = 0;
    DWORD dwFlags = 0;
    DWORD dwIoSize = 0;

    while (this->isRunning) {
        bSuccess = GetQueuedCompletionStatus(hIOCP, &dwIoSize,
                                             (PDWORD_PTR) &lpPerSocketContextnullptr,
                                             (LPOVERLAPPED *) &lpOverlapped,
                                             INFINITE);
        if (!bSuccess)
            printf("GetQueuedCompletionStatus() failed: %d\n", GetLastError());
        lpPerSocketContext = (PER_SOCKET_CONTEXT *) lpOverlapped;
        if (lpPerSocketContext == nullptr) {
            return;
        }

        if (this->isRunning) {
            return;
        }

        if (!bSuccess || (bSuccess && (dwIoSize == 0))) {
            this->CloseClient(lpPerSocketContext, FALSE);
            continue;
        }

        uint64_t sessionId = lpPerSocketContext->sessionId;
        switch (lpPerSocketContext->IOOperation) {
            case ClientIoRead: {
                Msg msg;
                this->sessions[sessionId]->iocp_context;
                msg.buff = (unsigned char *) lpPerSocketContext->wsabuf.buf;
                msg.len = dwIoSize;
                {

                    lpPerSocketContext->IOOperation = ClientIoRead;
                    dwRecvNumBytes = 0;
                    dwFlags = 0;
                    buffRecv.buf = lpPerSocketContext->Buffer,
                            buffRecv.len = BUFFER_SIZE;


                    nRet = WSARecv(lpPerSocketContext->sessionId, &buffRecv, 1,
                                   &dwRecvNumBytes, &dwFlags, &lpPerSocketContext->Overlapped, nullptr);
                    if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                        printf("WSARecv() failed: %d\n", WSAGetLastError());
                        this->CloseClient(lpPerSocketContext, FALSE);
                    }

                }
                this->onReadMsg(sessionId, msg);
                break;
            }
            case ClientIoWrite:

                lpPerSocketContext->IOOperation = ClientIoWrite;
                lpPerSocketContext->nSentBytes += dwIoSize;

                this->sessions[sessionId]->writeBuffer.erase(dwIoSize);

                this->onWriteBytes(lpPerSocketContext->sessionId, dwIoSize);//TODO x

                if (this->sessions[sessionId]->writeBuffer.size > 0)
                    this->continueSendMsg(sessionId);

                break;
            case ClientIoConnect: {
                std::cout << "connect" << std::endl;
                break;
            }

        }
    }
    return;
}

int Poller::connect(std::string ip, std::string port) {
    DWORD dwBytesRet;
    GUID GuidConnectEx = WSAID_CONNECTEX;
    LPFN_CONNECTEX pfnConnectEx;
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (SOCKET_ERROR == WSAIoctl(g_sdListen, SIO_GET_EXTENSION_FUNCTION_POINTER,
                                 &GuidConnectEx,
                                 sizeof(GuidConnectEx),
                                 &pfnConnectEx,
                                 sizeof(pfnConnectEx),
                                 &dwBytesRet,
                                 nullptr,
                                 nullptr))
        std::cout << "ERR:xp" << std::endl;

    PER_SOCKET_CONTEXT *iocp_connect_context = (PER_SOCKET_CONTEXT *) xmalloc(sizeof(PER_SOCKET_CONTEXT));
    memset(iocp_connect_context, 0, sizeof(PER_SOCKET_CONTEXT));
    iocp_connect_context->IOOperation = RWMOD::ClientIoConnect;
    iocp_connect_context->Overlapped.hEvent = nullptr;

    sockaddr_in addr;
    memset(&addr, 0, sizeof(sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = htons(std::stoi(port));

    SOCKADDR_IN local;
    local.sin_family = AF_INET;
    local.sin_addr.S_un.S_addr = INADDR_ANY;
    local.sin_port = 0;
    if (SOCKET_ERROR == bind(g_sdListen, (LPSOCKADDR) &local, sizeof(local))) {
        printf("err: bind!\r\n");
        return -1;
    }


    PVOID lpSendBuffer = nullptr;
    DWORD dwSendDataLength = 0;
    DWORD dwBytesSent = 0;
    WINBOOL bResult = pfnConnectEx(sock,
                                   (sockaddr *) &addr,
                                   sizeof(sockaddr_in),
                                   lpSendBuffer,
                                   dwSendDataLength,
                                   &dwBytesSent,
                                   (OVERLAPPED *) iocp_connect_context);
    if (!bResult) {
        if (WSAGetLastError() != ERROR_IO_PENDING) {
            std::cout << "err: ConnectEx " << WSAGetLastError() << std::endl;
            return -1;
        } else;// 操作未决（正在进行中 … ）
        {

        }
    }
    return 0;
}

int Poller::continueSendMsg(uint64_t sessionId) {
    int sendBytes = std::min<int>(sessions[sessionId]->writeBuffer.size, BUFFER_SIZE);
    auto &lpPerSocketContext = sessions[sessionId]->iocp_write_context;
    memcpy(lpPerSocketContext->wsabuf.buf, sessions[sessionId]->writeBuffer.buff, sendBytes);//TODO ZEROCPY
    lpPerSocketContext->IOOperation = ClientIoWrite;
    lpPerSocketContext->nTotalBytes = sendBytes;
    lpPerSocketContext->nSentBytes = 0;
    lpPerSocketContext->wsabuf.len = sendBytes;
    int dwFlags = 0;
    DWORD dwSendNumBytes = 0;
    int nRet = WSASend(lpPerSocketContext->sessionId, &lpPerSocketContext->wsabuf, 1,
                       &dwSendNumBytes, dwFlags, &(lpPerSocketContext->Overlapped), nullptr);
    if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
        printf("WSASend() failed: %d\n", WSAGetLastError());
        this->CloseClient(lpPerSocketContext, FALSE);
        return -1;
    }
    return 0;
}

int Poller::closeSession(uint64_t sessionId) {
    return 0;
}

void Poller::listenThreadCB() {

    SOCKET sdAccept = INVALID_SOCKET;
    PER_SOCKET_CONTEXT *lpPerSocketContext = nullptr;
    DWORD dwRecvNumBytes = 0;
    DWORD dwFlags = 0;
    int nRet = 0;

    while (this->isRunning) {
        sdAccept = WSAAccept(g_sdListen, nullptr, nullptr, nullptr, 0);

        if (sdAccept == SOCKET_ERROR) {
            printf("WSAAccept() failed: %d\n", WSAGetLastError());
            exit(-16);
        }
        this->onAccept(sdAccept, Addr());
        int workerId = sdAccept / 4 % maxWorker;
        lpPerSocketContext = UpdateCompletionPort(workerId, sdAccept, ClientIoRead, TRUE);
        if (lpPerSocketContext == nullptr)
            exit(-16);


        sessions[sdAccept]->sessionId = sdAccept;
        sessions[sdAccept]->iocp_context = lpPerSocketContext;

        if (this->isRunning)
            break;
        nRet = WSARecv(sdAccept, &(lpPerSocketContext->wsabuf),
                       1, &dwRecvNumBytes, &dwFlags,
                       &(lpPerSocketContext->Overlapped), nullptr);
        if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
            printf("WSARecv() Failed: %d\n", WSAGetLastError());
            CloseClient(lpPerSocketContext, FALSE);
        }
    }


    for (auto &E: workThreads) {
        E.join();
    }


    for (auto &ipcpsE:iocps) {
        if (ipcpsE) {
            PostQueuedCompletionStatus(ipcpsE, 0, 0, nullptr);
        }
    }

    //TODO CtxtListFree();

    for (auto &ipcpsE:iocps) {
        if (ipcpsE) {
            CloseHandle(ipcpsE);
            ipcpsE = nullptr;
        }
    }

    if (g_sdListen != INVALID_SOCKET) {
        closesocket(g_sdListen);
        g_sdListen = INVALID_SOCKET;
    }

    if (sdAccept != INVALID_SOCKET) {
        closesocket(sdAccept);
        sdAccept = INVALID_SOCKET;
    }
}

int Poller::run(int port) {
    {/* init */
        WSADATA wsaData;
        int nRet = 0;
        if ((nRet = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
            printf("WSAStartup() failed: %d\n", nRet);
            return -1;
        }
        xxx = new std::recursive_mutex;

    }
    {/* init queue  */
        taskQueue.resize(this->maxWorker);
        isRunning = FALSE;
    }
    {/*create pollers*/
        iocps.resize(maxWorker);
        for (auto &E:iocps) {
            E = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
            if (E == nullptr) {
                printf("CreateIoCompletionPort() failed to create I/O completion port: %d\n",
                       GetLastError());
                exit(-16);
            }
        }
    }

    {/* start listen*/
        if (!createListenSocket(port))
            exit(-16);
    }
    {/* start workers*/
        for (int i = 0; i < this->maxWorker; ++i) {
            workThreads.emplace_back(std::thread([=] { this->workerThreadCB(i); }));
        }
    }
    {/* start listen*/
        this->listenThread = std::thread([=] { this->listenThreadCB(); });
    }
    {/*wait exit */
        listenThread.join();
        for (auto &E: this->workThreads) {
            E.join();
        }
    }
    {/*exit*/
        delete xxx;
        WSACleanup();
    }
    return 0;
}

bool Poller::createListenSocket(int port) {

    int nRet = 0;
    int nZero = 0;
    struct addrinfo hints = {0};
    struct addrinfo *addrlocal = nullptr;

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_IP;
    if (getaddrinfo(nullptr, std::to_string(port).c_str(), &hints, &addrlocal) != 0) {
        printf("getaddrinfo() failed with error %d\n", WSAGetLastError());
        return (FALSE);
    }

    if (addrlocal == nullptr) {
        printf("getaddrinfo() failed to resolve/convert the interface\n");
        return (FALSE);
    }

    g_sdListen = WSASocket(addrlocal->ai_family, addrlocal->ai_socktype, addrlocal->ai_protocol,
                           nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (g_sdListen == INVALID_SOCKET) {
        printf("WSASocket(g_sdListen) failed: %d\n", WSAGetLastError());
        return (FALSE);
    }

    nRet = bind(g_sdListen, addrlocal->ai_addr, (int) addrlocal->ai_addrlen);
    if (nRet == SOCKET_ERROR) {
        printf("bind() failed: %d\n", WSAGetLastError());
        return (FALSE);
    }

    nRet = listen(g_sdListen, 5);
    if (nRet == SOCKET_ERROR) {
        printf("listen() failed: %d\n", WSAGetLastError());
        return (FALSE);
    }

    //
    // Disable send buffering on the socket.  Setting SO_SNDBUF
    // to 0 causes winsock to stop buffering sends and perform
    // sends directly from our buffers, thereby reducing CPU usage.
    //
    // However, this does prevent the socket from ever filling the
    // send pipeline. This can lead to packets being sent that are
    // not full (i.e. the overhead of the IP and TCP headers is
    // great compared to the amount of data being carried).
    //
    // Disabling the send buffer has less serious repercussions
    // than disabling the receive buffer.
    //
    nZero = 0;
    nRet = setsockopt(g_sdListen, SOL_SOCKET, SO_SNDBUF, (char *) &nZero, sizeof(nZero));
    if (nRet == SOCKET_ERROR) {
        printf("setsockopt(SNDBUF) failed: %d\n", WSAGetLastError());
        return (FALSE);
    }

    //
    // Don't disable receive buffering. This will cause poor network
    // performance since if no receive is posted and no receive buffers,
    // the TCP stack will set the window size to zero and the peer will
    // no longer be allowed to send data.
    //

    //
    // Do not set a linger value...especially don't set it to an abortive
    // close. If you set abortive close and there happens to be a bit of
    // data remaining to be transfered (or data that has not been
    // acknowledged by the peer), the PER_SOCKET_CONTEXT will be forcefully reset
    // and will lead to a loss of data (i.e. the peer won't get the last
    // bit of data). This is BAD. If you are worried about malicious
    // clients connecting and then not sending or receiving, the server
    // should maintain a timer on each PER_SOCKET_CONTEXT. If after some point,
    // the server deems a PER_SOCKET_CONTEXT is "stale" it can then set linger
    // to be abortive and close the PER_SOCKET_CONTEXT.
    //

    /*
	LINGER lingerStruct;

	lingerStruct.l_onoff = 1;
	lingerStruct.l_linger = 0;

	nRet = setsockopt(g_sdListen, SOL_SOCKET, SO_LINGER,
					  (char *)&lingerStruct, sizeof(lingerStruct) );
	if( nRet == SOCKET_ERROR ) {
		printf("setsockopt(SO_LINGER) failed: %d\n", WSAGetLastError());
		return(FALSE);
	}
    */

    freeaddrinfo(addrlocal);
    return (TRUE);
}


PER_SOCKET_CONTEXT *Poller::UpdateCompletionPort(int workerId, SOCKET sd, RWMOD ClientIo,
                                                 BOOL bAddToList) {

    PER_SOCKET_CONTEXT *lpPerSocketContext = nullptr;


    lpPerSocketContext = CtxtAllocate(sd, RWMOD::ClientIoWrite);
    if (lpPerSocketContext == nullptr)
        return (nullptr);

    lpPerSocketContext = CtxtAllocate(sd, RWMOD::ClientIoRead);
    if (lpPerSocketContext == nullptr)
        return (nullptr);


    HANDLE iocp = iocps[workerId];
    iocp = CreateIoCompletionPort((HANDLE) sd, iocp, (DWORD_PTR) nullptr, 0);
    if (iocp == nullptr) {
        printf("CreateIoCompletionPort() failed: %d\n", GetLastError());
        //xfree(lpPerSocketContext);
        return (nullptr);
    }

    //TODO if (bAddToList) CtxtListAddTo(lpPerSocketContext);

    return (lpPerSocketContext);
}

void Poller::CloseClient(PER_SOCKET_CONTEXT *lpPerSocketContext,
                         BOOL bGraceful) {

    xxx->lock();

    if (lpPerSocketContext) {
        if (!bGraceful) {

            LINGER lingerStruct;

            lingerStruct.l_onoff = 1;
            lingerStruct.l_linger = 0;
            setsockopt(lpPerSocketContext->sessionId, SOL_SOCKET, SO_LINGER,
                       (char *) &lingerStruct, sizeof(lingerStruct));
        }
        closesocket(lpPerSocketContext->sessionId);
        lpPerSocketContext->sessionId = INVALID_SOCKET;
        //TODO: remove online list
        lpPerSocketContext = nullptr;
    } else {
        printf("CloseClient: lpPerSocketContext is nullptr\n");
    }
    xxx->unlock();
    return;
}

//
// Allocate a socket context for the new PER_SOCKET_CONTEXT.
//
PER_SOCKET_CONTEXT *Poller::CtxtAllocate(SOCKET sd, RWMOD ClientIO) {

    if (ClientIO == RWMOD::ClientIoRead) {
        PER_SOCKET_CONTEXT *lpPerSocketContext = sessions[sd]->iocp_context;

        xxx->lock();

        if (lpPerSocketContext) {
            lpPerSocketContext->sessionId = sd;
            lpPerSocketContext->Overlapped.Internal = 0;
            lpPerSocketContext->Overlapped.InternalHigh = 0;
            lpPerSocketContext->Overlapped.Offset = 0;
            lpPerSocketContext->Overlapped.OffsetHigh = 0;
            lpPerSocketContext->Overlapped.hEvent = nullptr;
            lpPerSocketContext->IOOperation = ClientIO;
            //lpPerSocketContext->pIOContextForward = nullptr;
            lpPerSocketContext->nTotalBytes = 0;
            lpPerSocketContext->nSentBytes = 0;
            lpPerSocketContext->wsabuf.buf = lpPerSocketContext->Buffer;
            lpPerSocketContext->wsabuf.len = sizeof(lpPerSocketContext->Buffer);

            ZeroMemory(lpPerSocketContext->wsabuf.buf, lpPerSocketContext->wsabuf.len);
        } else {
            printf("HeapAlloc() PER_SOCKET_CONTEXT failed: %d\n", GetLastError());
        }

        xxx->unlock();
        return (lpPerSocketContext);
    } else if (ClientIO == RWMOD::ClientIoWrite) {
        PER_SOCKET_CONTEXT *lpPerSocketContext = sessions[sd]->iocp_write_context;

        xxx->lock();

        if (lpPerSocketContext) {
            lpPerSocketContext->sessionId = sd;
            lpPerSocketContext->Overlapped.Internal = 0;
            lpPerSocketContext->Overlapped.InternalHigh = 0;
            lpPerSocketContext->Overlapped.Offset = 0;
            lpPerSocketContext->Overlapped.OffsetHigh = 0;
            lpPerSocketContext->Overlapped.hEvent = nullptr;
            lpPerSocketContext->IOOperation = ClientIO;
            //lpPerSocketContext->pIOContextForward = nullptr;
            lpPerSocketContext->nTotalBytes = 0;
            lpPerSocketContext->nSentBytes = 0;
            lpPerSocketContext->wsabuf.buf = lpPerSocketContext->Buffer;
            lpPerSocketContext->wsabuf.len = sizeof(lpPerSocketContext->Buffer);

            ZeroMemory(lpPerSocketContext->wsabuf.buf, lpPerSocketContext->wsabuf.len);
        } else {
            printf("HeapAlloc() PER_SOCKET_CONTEXT failed: %d\n", GetLastError());
        }

        xxx->unlock();
        return (lpPerSocketContext);
    }

    return nullptr;
}


#endif