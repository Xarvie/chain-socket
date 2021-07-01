#ifndef CHAIN_SOCKET_LOG_H
#define CHAIN_SOCKET_LOG_H


class Log {
public:
    Log();

	virtual ~Log();

    virtual void init(const char *fileName, int logLevel);

    virtual void output(const char *context, int logLevel);

protected:
    int level;
    const char *pwd;
};


#endif //CHAIN_SOCKET_LOG_H
