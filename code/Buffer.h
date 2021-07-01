#ifndef BUFFER_H_
#define BUFFER_H_

#include <stdint.h>

class MessageBuffer {
public:
    enum {
        BUFFER_SIZE = 1024,
        BUFFER_MAX_SIZE = 100 * 1024 * 1024
    };

    void setMaxSize(int size);

    void reset();

    void destroy();

    void push_back(int len, const unsigned char *buff1);

    void alloc();

    unsigned char *alloc(int size);

    void erase(int len);

    int size = 0;
    unsigned char *buff = nullptr;
    int capacity = 0;
    int maxSize = BUFFER_MAX_SIZE;
};

struct Session {
    enum Type {
        NONE = 0,
        ACCEPT = 1,
        CONNECT = 2,
        LISTEN = 3
    };
    uint64_t fd = 0;
    int sessionId = 0;
    MessageBuffer writeBuffer;
    MessageBuffer readBuffer;

    Type type = Type::ACCEPT;
    char heartBeats = 0;
    bool canRead = false;
    bool ioPending = false;
    bool isConnect = false;
    uint8_t wsState = 0;

    void reset() {
        sessionId = 0;
        heartBeats = 0;
        canRead = false;
        ioPending = false;
        isConnect = false;
        wsState = 0;
        readBuffer.reset();
        writeBuffer.reset();
    }

    void destroy() {
        readBuffer.destroy();
        writeBuffer.destroy();
    }
};

#endif /* BUFFER_H_ */
