#ifndef WEBSOCKET_POLLER_HPP
#define WEBSOCKET_POLLER_HPP


#include "websocket.h"
#include "NetPoller.h"
#include "NetStruct.h"


static uint64_t count = 0;
static unsigned long long ix = 0;

static int memstr(unsigned char *full_data, int full_data_len, void *substr, int sublen) {
    if (full_data == NULL || full_data_len <= 0 || substr == NULL || sublen == 0) {
        return -1;
    }
    unsigned char *subbyte = (unsigned char *) substr;

    int i;
    unsigned char *cur = full_data;
    int last_possible = full_data_len - sublen + 1;
    for (i = 0; i < last_possible; i++) {
        if (*cur == *subbyte) {
            if (memcmp(cur, substr, sublen) == 0) {
                return i;
            }
        }
        cur++;
    }

    return -1;
}


static size_t getPayloadLength(const uint8_t *inputFrame, size_t inputLength,
                               uint8_t *payloadFieldExtraBytes, enum wsFrameType *frameType) {
    size_t payloadLength = inputFrame[1] & 0x7F;
    *payloadFieldExtraBytes = 0;
    if ((payloadLength == 0x7E && inputLength < 4) || (payloadLength == 0x7F && inputLength < 10)) {
        *frameType = WS_INCOMPLETE_FRAME;
        return 0;
    }
    if (payloadLength == 0x7F && (inputFrame[3] & 0x80) != 0x0) {
        *frameType = WS_ERROR_FRAME;
        return 0;
    }

    if (payloadLength == 0x7E) {
        uint16_t payloadLength16b = 0;
        *payloadFieldExtraBytes = 2;
        memcpy(&payloadLength16b, &inputFrame[2], *payloadFieldExtraBytes);
        payloadLength = ntohs(payloadLength16b);
    } else if (payloadLength == 0x7F) {
        *frameType = WS_ERROR_FRAME;
        return 0;

        /* // implementation for 64bit systems
        uint64_t payloadLength64b = 0;
        *payloadFieldExtraBytes = 8;
        memcpy(&payloadLength64b, &inputFrame[2], *payloadFieldExtraBytes);
        if (payloadLength64b > SIZE_MAX) {
            *frameType = WS_ERROR_FRAME;
            return 0;
        }
        payloadLength = (size_t)ntohll(payloadLength64b);
        */
    }

    return payloadLength;
}


class WebSocketPoller : public MsgHub {
public:
    enum {
        WS_WAIT_HANDSHAKE_WAIT,
        WS_WAIT_HANDSHAKE_REQ,
        WS_WAIT_FRAME
    };

    explicit WebSocketPoller() {
    }

    virtual int genTask(ConnectionTask *task) override {
        q->enqueue(*task);
        return 0;
    }

    virtual int onTask() override {
        ConnectionTask task;
        if (q->try_dequeue(task)) {
            if ((Session*)this->sessions[task.sessionId] == task.sessPtr) {
                if (task.type == 1) {
                    if (task.len > sizeof(SendTask)) {
                        SendTask *t = (SendTask *) task.data;
                        this->sendMsg(task.sessPtr, t->data, t->len);
                    }

                }
                if (task.type == 2) {
                    if (task.len > sizeof(CloseTask)) {
                        CloseTask *t = (CloseTask *) task.data;
                        this->closeSession(task.sessionId, t->reason);
                    }
                }
                if (task.type == 3) {
                    if (task.len > sizeof(ConnectTask)) {
                        ConnectTask *addr = (ConnectTask *) task.data;
                        this->connectTo((char *) addr->addr, addr->port, addr->timeOut);
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


    virtual int onAccept(Session *lconnp, Session *aconnp) override {
        std::cout << "onAccept :" << aconnp->fd << std::endl;
        this->postRecv(aconnp);
        nullHandshake(&hs);
        enum wsState state = WS_STATE_OPENING;
        enum wsFrameType frameType = WS_INCOMPLETE_FRAME;
        return 0;
    }

    virtual int onReadMsg(Session &conn, unsigned char *curBuffer, unsigned int MsgSize) {
        std::cout << "onReadMsg" << std::endl;
        return 0;
    }

    int sendMsg1(Session *connp, unsigned char *curBuffer, unsigned int MsgSize) {

        enum wsFrameType frameType = WS_TEXT_FRAME;
        assert(frameType < 0x10);

        unsigned char outFrame[10];
        int frameHeadLen = 0;
        //conn.writeBuffer.alloc();
        outFrame[0] = 0x80 | frameType;

        if (MsgSize <= 125) {
            outFrame[1] = MsgSize;
            frameHeadLen = 2;
        } else if (MsgSize <= 0xFFFF) {
            outFrame[1] = 126;
            uint16_t payloadLength16b = htons(MsgSize);
            memcpy(&outFrame[2], &payloadLength16b, 2);
            frameHeadLen = 4;
        } else {
            assert(MsgSize <= 0xFFFF);

            /*
            outFrame[1] = 127;
            dataLength = htonll(dataLength);
            memcpy(&outFrame[2], &dataLength, 8);
            frameHeadLen = 10;
            */
        }


        this->sendTo(connp, outFrame, frameHeadLen);

        this->sendTo(connp, curBuffer, MsgSize);

        //memcpy(&outFrame[frameHeadLen], curBuffer, MsgSize);
        return 0;
    }

    int sendMsg2(Session *connp, unsigned char *curBuffer, unsigned int MsgSize, bool useMask) {

        const uint8_t masking_key[4] = {0x12, 0x34, 0x56, 0x78};
        std::vector<uint8_t> header;
        header.assign(2 + (MsgSize >= 126 ? 2 : 0) + (MsgSize >= 65536 ? 6 : 0) + (useMask ? 4 : 0), 0);

        enum wsFrameType frameType = WS_TEXT_FRAME;
        assert(frameType < 0x10);

        unsigned char outFrame[10];
        int frameHeadLen = 0;
        //conn.writeBuffer.alloc();
        outFrame[0] = 0x80 | frameType;

        if (MsgSize <= 125) {
            outFrame[1] = (MsgSize & 0xff) | (useMask ? 0x80 : 0);
            frameHeadLen = 2;
            if (useMask) {
                outFrame[2] = masking_key[0];
                outFrame[3] = masking_key[1];
                outFrame[4] = masking_key[2];
                outFrame[5] = masking_key[3];
                frameHeadLen += 4;
            }
        } else if (MsgSize <= 0xFFFF) {
            outFrame[1] = 126;
            uint16_t payloadLength16b = htons(MsgSize);
            memcpy(&outFrame[2], &payloadLength16b, 2);

            outFrame[1] = 126 | (useMask ? 0x80 : 0);
            outFrame[2] = (MsgSize >> 8) & 0xff;
            outFrame[3] = (MsgSize >> 0) & 0xff;
            frameHeadLen = 4;
            if (useMask) {
                outFrame[4] = masking_key[0];
                outFrame[5] = masking_key[1];
                outFrame[6] = masking_key[2];
                outFrame[7] = masking_key[3];
                frameHeadLen += 4;
            }
        } else {
            assert(MsgSize <= 0xFFFF);

        }


        if (useMask) {

            for (size_t i = 0; i != MsgSize; ++i) {
                curBuffer[i] ^= masking_key[i & 0x3];
            }
        }

        Msg msg1;
        msg1.len = frameHeadLen;
        msg1.buff = outFrame;

        this->sendTo(connp, outFrame, frameHeadLen);

        this->sendTo(connp, curBuffer, MsgSize);

        //memcpy(&outFrame[frameHeadLen], curBuffer, MsgSize);
        return 0;
    }

    int sendMsg(Session *connp, unsigned char *curBuffer, unsigned int MsgSize) {
        if (connp == NULL)
            return 0;
        if (connp->isConnect) {
            sendMsg2(connp, curBuffer, MsgSize, true);
        } else {
            sendMsg2(connp, curBuffer, MsgSize, false);
        }
        return 0;
    }

    virtual int onRecv(Session *connp, int len) override {
        //std::cout << " WebSocketPoller::onRecv" << std::endl;
        if (connp == NULL)
            return 0;
        auto &conn = *connp;
        if (conn.wsState == WS_WAIT_HANDSHAKE_WAIT) {

            do {
                int pos = memstr(conn.readBuffer.buff, conn.readBuffer.size, (void *) "\r\n\r\n", 4);
                if (-1 == pos)
                    break;

                enum wsFrameType frameType = wsParseHandshake(conn.readBuffer.buff, pos + 1, &hs);
                if (frameType == wsFrameType::WS_ERROR_FRAME) {
                    closeSession(connp->sessionId, CT_NORMAL);

                    return 0;
                }

                assert(hs.key);

                char *responseKey = NULL;
                uint8_t length = strlen(hs.key) + strlen_P(secret);
                responseKey = (char *) malloc(length);
                memcpy(responseKey, hs.key, strlen(hs.key));
                memcpy_P(&(responseKey[strlen(hs.key)]), secret, strlen_P(secret));
                unsigned char shaHash[20];
                memset(shaHash, 0, sizeof(shaHash));
                sha1(shaHash, responseKey, length);
                size_t base64Length = base64(responseKey, length, shaHash, 20);
                responseKey[base64Length] = '\0';
                int lenmsg = strlen_P("HTTP/1.1 101 Switching Protocols\r\n") +
                             strlen_P(upgradeField) + strlen_P(websocket) + strlen_P("\r\n")
                             + strlen_P(connectionField) + strlen_P(upgrade2) + strlen_P("\r\n")
                             + strlen_P("Sec-WebSocket-Accept: ") + strlen_P(responseKey) + strlen_P("\r\n\r\n");
                char *outFrame = (char *) malloc(lenmsg + 1);

                int written = sprintf_P((char *) outFrame,
                                        PSTR("HTTP/1.1 101 Switching Protocols\r\n"
                                             "%s%s\r\n"
                                             "%s%s\r\n"
                                             "Sec-WebSocket-Accept: %s\r\n\r\n"),
                                        upgradeField,
                                        websocket,
                                        connectionField,
                                        upgrade2,
                                        responseKey);

                this->sendTo(connp, (unsigned char *) outFrame, lenmsg);
                free(responseKey);
                free(outFrame);
                conn.wsState = WS_WAIT_FRAME;
                conn.readBuffer.erase(pos + 4);
                if (conn.readBuffer.size == 0)
                    return 0;
            } while (0);
        } else if (conn.wsState == WS_WAIT_HANDSHAKE_REQ) {
            if (conn.readBuffer.size < sizeof("HTTP/1.1 101"))
                return 0;

            int pos = memstr(conn.readBuffer.buff, conn.readBuffer.size, (void *) "\r\n\r\n", 4);

            if (-1 == pos) {
                return 0;
            }
            int pos2 = memstr(conn.readBuffer.buff, conn.readBuffer.size, (void *) "HTTP/1.1 101", 4);

            if (-1 == pos2) {
                closeSession(connp->sessionId, CT_WEBSOCKET_READ_HEAD_ERROR);
                return 0;
            }

            conn.wsState = WS_WAIT_FRAME;
            conn.readBuffer.erase(pos + sizeof("\r\n\r\n") - 1);
            this->onHandShake(conn);
            if (conn.readBuffer.size == 0)
                return 0;
        }

        if (conn.wsState == WS_WAIT_FRAME) {
            do {
                int parseSize = 0;
                for (;;) {
                    int inputLength = conn.readBuffer.size - parseSize;
                    uint8_t *inputFrame = (uint8_t *) conn.readBuffer.buff + parseSize;

                    if (inputLength < 2)
                        break;

                    if ((inputFrame[0] & 0x70) != 0x0) // extensions off
                    {
                        closeSession(conn.sessionId, CT_WEBSOCKET_EXTENSION_ON);
                        return WS_ERROR_FRAME;
                    }

                    if ((inputFrame[0] & 0x80) != 0x80) // continuation
                    {
                        closeSession(conn.sessionId, CT_WEBSOCKET_CONTINUATION_ON);
                        return WS_ERROR_FRAME;
                    }

                    uint8_t opcode = inputFrame[0] & 0x0F;
                    if (opcode == WS_TEXT_FRAME ||
                        opcode == WS_BINARY_FRAME ||
                        opcode == WS_CLOSING_FRAME ||
                        opcode == WS_PING_FRAME ||
                        opcode == WS_PONG_FRAME
                            ) {

                        bool use_mask = (inputFrame[1] & 0x80) == 0x80;
                        uint8_t N0 = (inputFrame[1] & 0x7f);
                        int header_size = 2 + (N0 == 126 ? 2 : 0) + (N0 == 127 ? 8 : 0) + (use_mask ? 4 : 0);
                        int N = 0;
                        enum wsFrameType frameType = (wsFrameType) opcode;
                        uint8_t masking_key[4] = {0, 0, 0, 0};
                        if (inputLength < header_size) { break; }
                        int i = 0;
                        if (N0 < 126) {
                            N = N0;
                            i = 2;
                        } else if (N0 == 126) {
                            N = 0;
                            N |= ((uint64_t) inputFrame[2]) << 8;
                            N |= ((uint64_t) inputFrame[3]) << 0;
                            i = 4;
                        } else if (N0 == 127) {
                            abort();
                        }

                        if (use_mask) {
                            masking_key[0] = ((uint8_t) inputFrame[i + 0]) << 0;
                            masking_key[1] = ((uint8_t) inputFrame[i + 1]) << 0;
                            masking_key[2] = ((uint8_t) inputFrame[i + 2]) << 0;
                            masking_key[3] = ((uint8_t) inputFrame[i + 3]) << 0;
                        } else {
                            masking_key[0] = 0;
                            masking_key[1] = 0;
                            masking_key[2] = 0;
                            masking_key[3] = 0;
                        }
                        if (inputLength < header_size + N) { break; }
                        if (false) {}
                        else if (
                                opcode == WS_CONTINUATION_FRAME
                                || opcode == WS_TEXT_FRAME
                                || opcode == WS_BINARY_FRAME
                                ) {
                            if (use_mask) {
                                for (size_t i = 0; i != N; ++i) {
                                    inputFrame[i + header_size] ^= masking_key[i & 0x3];
                                }
                            }
                            this->onReadMsg(conn, inputFrame + header_size, N);
                            parseSize += (header_size + N);
                        } else if (opcode == WS_PING_FRAME) {
                            if (use_mask) {
                                for (size_t i = 0; i != N; ++i) {
                                    inputFrame[i + header_size] ^= masking_key[i & 0x3];
                                }
                            }
                            //std::string data(rxbuf.begin()+ws.header_size, rxbuf.begin()+header_size+(size_t)N);
                            //sendData(wsheader_type::PONG, data.size(), data.begin(), data.end());
                        } else if (opcode == WS_PONG_FRAME) {}
                        else if (opcode == WS_CLOSING_FRAME) { ; }
                        else { abort(); /*fprintf(stderr, "ERROR: Got unexpected WebSocket message.\n"); close(); */}


                    } else {

                        break;
                    }


                }
                conn.readBuffer.erase(parseSize);


            } while (0);
        }

        return 0;
    }

    int onDisconnect(Session &conn, int type) {

        std::cout << "disconnect SessionID:" << conn.sessionId << " type:" << type << std::endl;
        return 0;
    }

    virtual int onHandShake(Session &conn) {
        return 0;
    }

    virtual int onConnect(Session *connp, std::string ip, int port) override {
        if (connp == NULL)
            return 0;
        this->postRecv(connp);
        connp->heartBeats = HEARTBEATS_COUNT;

        std::string getStr = "GET / HTTP/1.1\r\nHost: ";
        getStr += ip;
        getStr += ":";
        getStr += std::to_string(port);
        getStr += "\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:69.0) Gecko/20100101 Firefox/69.0\r\nAccept: */*\r\nAccept-Language: en-US,zh-CN;q=0.8,zh;q=0.7,zh-TW;q=0.5,zh-HK;q=0.3,en;q=0.2\r\nAccept-Encoding: gzip, deflate\r\nSec-WebSocket-Version: 13\r\nOrigin: http://www.websocket-test.com\r\nSec-WebSocket-Extensions: permessage-deflate\r\nSec-WebSocket-Key: L/shkja7V4sYs+Ng+ayRwQ==\r\nConnection: keep-alive, Upgrade\r\nPragma: no-cache\r\nCache-Control: no-cache\r\nUpgrade: websocket\r\n\r\n";
        connp->isConnect = true;
        sendTo(connp, (unsigned char *) getStr.c_str(), getStr.length());
        connp->wsState = WS_WAIT_HANDSHAKE_REQ;
//		logger->output((std::string("") + "connect SessionID:" + std::to_string(connp->sessionId)).c_str(), 0);
       // std::cout << "connect SessionID:" << connp->sessionId << std::endl;
        return 0;
    }


    int makeFrameHead(unsigned char outFrame[], unsigned int MsgSize, bool useMask, unsigned char masking_key[]) {
        std::vector<uint8_t> header;
        header.assign(2 + (MsgSize >= 126 ? 2 : 0) + (MsgSize >= 65536 ? 6 : 0) + (useMask ? 4 : 0), 0);

        enum wsFrameType frameType = WS_TEXT_FRAME;
        assert(frameType < 0x10);
        int frameHeadLen = 0;
        //conn.writeBuffer.alloc();
        outFrame[0] = 0x80 | frameType;

        if (MsgSize <= 125) {
            outFrame[1] = (MsgSize & 0xff) | (useMask ? 0x80 : 0);
            frameHeadLen = 2;
            if (useMask) {
                outFrame[2] = masking_key[0];
                outFrame[3] = masking_key[1];
                outFrame[4] = masking_key[2];
                outFrame[5] = masking_key[3];
                frameHeadLen += 4;
            }
        } else if (MsgSize <= 0xFFFF) {
            outFrame[1] = 126;
            uint16_t payloadLength16b = htons(MsgSize);
            memcpy(&outFrame[2], &payloadLength16b, 2);

            outFrame[1] = 126 | (useMask ? 0x80 : 0);
            outFrame[2] = (MsgSize >> 8) & 0xff;
            outFrame[3] = (MsgSize >> 0) & 0xff;
            frameHeadLen = 4;
            if (useMask) {
                outFrame[4] = masking_key[0];
                outFrame[5] = masking_key[1];
                outFrame[6] = masking_key[2];
                outFrame[7] = masking_key[3];
                frameHeadLen += 4;
            }
        } else {
            assert(MsgSize <= 0xFFFF);

        }


        return frameHeadLen;
    }

    void mask(unsigned char *curBuffer, unsigned int MsgSize, unsigned char masking_key[]) {
        if (true) {
            for (size_t i = 0; i != MsgSize; ++i) {
                curBuffer[i] ^= masking_key[i & 0x3];
            }
        }
    }

public:
    struct handshake hs;
};

#endif //WEBSOCKET_POLLER_HPP