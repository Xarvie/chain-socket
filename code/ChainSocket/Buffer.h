#ifndef BUFFER_H_
#define BUFFER_H_

#include <cstdint>

class MessageBuffer {
public:
	enum {
		BUFFER_SIZE = 1024,
		BUFFER_MAX_SIZE = 100 * 1024 * 1024
	};

	void setMaxSize(int size);

	void init();

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

union UserData {
	void *ptr;
	int fd;
	uint32_t u32;
	uint64_t u64;
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

	UserData userData = {nullptr};

	void (*onUserDataDestory)(UserData *udata) = nullptr;

	void init() {
		sessionId = 0;
		heartBeats = 0;
		canRead = false;
		ioPending = false;
		isConnect = false;
		userData.ptr = nullptr;
		onUserDataDestory = nullptr;
		readBuffer.init();
		writeBuffer.init();
	}

	void reset() {
		destroy();
		init();
	}

	void destroy() {
		if (userData.ptr && onUserDataDestory)
			onUserDataDestory(&userData);
		readBuffer.destroy();
		writeBuffer.destroy();
		userData.ptr = nullptr;
		onUserDataDestory = nullptr;
		sessionId = 0;
		heartBeats = 0;
		canRead = false;
		ioPending = false;
		isConnect = false;
	}
};

#endif /* BUFFER_H_ */
