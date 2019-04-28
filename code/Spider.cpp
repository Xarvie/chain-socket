#include "Spider.h"

Spider &Spider::operator=(Spider &&rhs) noexcept {
    return *this;
    //TODO
}

Spider::~Spider() {
    //TODO
}

void Spider::run() {
    //TODO
}

void Spider::stop() {
    //TODO
}

int Spider::onAccept(uint64_t sessionId, const Addr &addr) {
    //TODO
    return 0;
}

int Spider::onReadMsg(uint64_t sessionId, int bytesNum) {
    //TODO
    return 0;
}

int Spider::onWriteBytes(uint64_t sessionId, int len) {
    //TODO
    return 0;
}