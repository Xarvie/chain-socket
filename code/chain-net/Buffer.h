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
			void* tmpBuff = ::realloc(this->buff, this->capacity);
			if (tmpBuff == nullptr)
				abort();//TODO
			this->buff = (unsigned char*)tmpBuff;
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
		void* tmpBuff = ::realloc(this->buff, this->capacity);
		if (buff == nullptr)
			abort();//TODO
        this->buff = (unsigned char*)tmpBuff;


    }

    void record(int size)
    {
        this->size += size;
    }
    inline void erase(int len)
    {
        if(len == 0)
            return ;
        if(len < size)
            memmove(this->buff, this->buff+len, (size_t)this->size - len);
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

    int heartBeats = 0;
    bool canRead = false;
    bool canWrite = false;
    void reset() {
        sessionId = 0;
        heartBeats = 0;
        canRead = false;
        canWrite = false;
        readBuffer.reset();
        writeBuffer.reset();
    }
};

#endif /* BUFFER_H_ */
