
#define xmalloc malloc
#define xfree free

#include "Buffer.h"
#include <stdlib.h>
#include <cstring>

void MessageBuffer::init() {
    buff = (unsigned char *) xmalloc(BUFFER_SIZE);
    capacity = BUFFER_SIZE;
    maxSize = BUFFER_MAX_SIZE;
    size = 0;
}

void MessageBuffer::reset(){
	destroy();
	init();
}

void MessageBuffer::setMaxSize(int size) {
    this->maxSize = size;
}

void MessageBuffer::destroy() {
    capacity = 0;
    xfree(buff);
    buff = nullptr;
    size = 0;
}

void MessageBuffer::push_back(int len, const unsigned char *buff1) {
    int newSize = size + len;
    if (newSize >= capacity - BUFFER_SIZE * 2) {
        this->capacity = (((size + len) / BUFFER_SIZE) + 3) * BUFFER_SIZE;
        void *tmpBuff = ::realloc(this->buff, this->capacity);
        if (tmpBuff == nullptr)
            abort();
        this->buff = (unsigned char *) tmpBuff;
    }
    memcpy(this->buff + size, buff1, len);
    this->size += len;
}

void MessageBuffer::alloc() {
    if (this->capacity - this->size >= BUFFER_SIZE * 2) {
        return;
    }
    int newSize = ((this->size / BUFFER_SIZE) + 3) * BUFFER_SIZE;
    this->capacity = newSize;
    void *tmpBuff = ::realloc(this->buff, this->capacity);
    if (buff == nullptr)
        abort();
    this->buff = (unsigned char *) tmpBuff;
}

unsigned char *MessageBuffer::alloc(int size) {
    this->size += size;
    if (this->capacity - this->size >= BUFFER_SIZE * 2) {
        return this->buff + this->size - size;
    }
    this->capacity = ((this->size / BUFFER_SIZE) + 3) * BUFFER_SIZE;
    void *tmpBuff = ::realloc(this->buff, this->capacity);
    if (tmpBuff == nullptr)
        abort();
    this->buff = (unsigned char *) tmpBuff;
    return this->buff + this->size - size;
}

void MessageBuffer::erase(int len) {
    if (len == 0)
        return;
    if (len < size)
        memmove(this->buff, this->buff + len, (size_t) this->size - len);
    this->size -= len;
}


