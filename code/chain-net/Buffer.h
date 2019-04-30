#ifndef BUFFER_H_
#define BUFFER_H_
#include "NetStruct.h"

class MessageBuffer
{
public:

    void reset()
    {
//        if(buff != nullptr)
//            destroy();
        buff = (unsigned char*)xmalloc(xx::BUFFER_SIZE);
        capacity = BUFFER_SIZE;
        size = 0;
    }
    void destroy()
    {
        capacity = 0;
        free(buff);
        buff = nullptr;
        size = 0;
    }

    void push_back(int len, const unsigned char* buff1)
    {
        int newSize = size + len;
        if(newSize >= capacity - BUFFER_SIZE * 2)
        {
            this->capacity = (((size + len)/BUFFER_SIZE)+3)*BUFFER_SIZE;
            this->buff = (unsigned char*)::realloc(this->buff, this->capacity);
            if(buff == nullptr)
                ;//TODO
        }

        memcpy(this->buff + size, buff1, len);
        this->size += len;
    }

    void alloc()
    {
        if(this->capacity - this->size >= BUFFER_SIZE * 2)
        {
            return ;
        }
        int newSize = ((this->size / BUFFER_SIZE)+3)*BUFFER_SIZE;
        this->capacity = newSize;
        this->buff = (unsigned char*)::realloc(this->buff, this->capacity);
        if(buff == nullptr)
            ;//TODO

    }

    void record(int size)
    {
        this->size += size;
    }
    inline void erase(int len)
    {
        memmove(this->buff, this->buff+len, this->size - len);
        this->size -= len;
    }
//private:
    std::list<int> size_list;
    int size = 0;
    unsigned char* buff = nullptr;
    int capacity = 0;
};

struct Session {
    uint64_t sessionId = 0;
    MessageBuffer writeBuffer;
    MessageBuffer readBuffer;

    void reset() {
        sessionId = 0;
        readBuffer.reset();
        writeBuffer.reset();
    }
};

#endif /* BUFFER_H_ */
