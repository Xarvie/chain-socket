#ifndef SERVER_TIMER_H
#define SERVER_TIMER_H

#include <vector>
#include <functional>

class TimerManager;


class Timer {
public:
    friend class TimerManager;

    enum TimerType {
        ONCE, CIRCLE
    };

    Timer(TimerManager &manager);

    virtual ~Timer();

    template<typename Fun>
    void Start(Fun fun, unsigned interval, TimerType timeType = CIRCLE);

    void stop();

public:
    void *data;
private:
    void OnTimer(unsigned long long now);

private:

    TimerManager &manager_;
    TimerType timerType_;
    std::function<void(void *)> timerFun_;
    unsigned interval_;
    unsigned long long expires_;
    size_t heapIndex_;
    int pollerIndex;
};

class TimerManager {
public:
    static unsigned long long GetCurrentMillisecs();

    int DetectTimers();

private:
    friend class Timer;

    void AddTimer(Timer *timer);

    void RemoveTimer(Timer *timer);

    void UpHeap(size_t index);

    void DownHeap(size_t index);

    void SwapHeap(size_t, size_t index2);

private:
    struct HeapEntry {
        unsigned long long time;
        Timer *timer;
    };
    std::vector<HeapEntry> heap_;
};

template<typename Fun>
inline void Timer::Start(Fun fun, unsigned interval, TimerType timeType) {
    stop();
    interval_ = interval;
    timerFun_ = fun;
    timerType_ = timeType;
    this->expires_ = this->interval_ + TimerManager::GetCurrentMillisecs();
    manager_.AddTimer(this);
}

#endif //SERVER_TIMER_H
