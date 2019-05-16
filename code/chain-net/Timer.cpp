#define _CRT_SECURE_NO_WARNINGS
#include "Timer.h"
#ifdef _WIN32
# include <sys/timeb.h>
#else
# include <sys/time.h>
#endif

Timer::Timer(TimerManager& manager)
        : manager_(manager)
        , heapIndex_(-1)
{
}

Timer::~Timer()
{
    Stop();
}

void Timer::Stop()
{
    if (heapIndex_ != -1)
    {
        manager_.RemoveTimer(this);
        heapIndex_ = -1;
    }
}


void Timer::OnTimer(unsigned long long now)
{
    if (timerType_ == Timer::CIRCLE)
    {
        expires_ = interval_ + now;
        manager_.AddTimer(this);
    }
    else
    {
        heapIndex_ = -1;
    }

    timerFun_(data);
}

void TimerManager::AddTimer(Timer* timer)
{
    timer->heapIndex_ = heap_.size();
    HeapEntry entry = { timer->expires_, timer };
    heap_.push_back(entry);
    UpHeap(heap_.size() - 1);
}

void TimerManager::RemoveTimer(Timer* timer)
{
    size_t index = timer->heapIndex_;
    if (!heap_.empty() && index < heap_.size())
    {
        if (index == heap_.size() - 1)
        {
            heap_.pop_back();
        }
        else
        {
            SwapHeap(index, heap_.size() - 1);
            heap_.pop_back();
            size_t parent = (index - 1) / 2;
            if (index > 0 && heap_[index].time < heap_[parent].time)
                UpHeap(index);
            else
                DownHeap(index);
        }
    }
}

void TimerManager::DetectTimers()
{
    unsigned long long now = GetCurrentMillisecs();

    while (!heap_.empty() && heap_[0].time <= now)
    {
        Timer* timer = heap_[0].timer;
        RemoveTimer(timer);
        timer->OnTimer(now);
    }
}

void TimerManager::UpHeap(size_t index)
{
    size_t parent = (index - 1) / 2;
    while (index > 0 && heap_[index].time < heap_[parent].time)
    {
        SwapHeap(index, parent);
        index = parent;
        parent = (index - 1) / 2;
    }
}

void TimerManager::DownHeap(size_t index)
{
    size_t child = index * 2 + 1;
    while (child < heap_.size())
    {
        size_t minChild = (child + 1 == heap_.size() || heap_[child].time < heap_[child + 1].time)
                          ? child : child + 1;
        if (heap_[index].time < heap_[minChild].time)
            break;
        SwapHeap(index, minChild);
        index = minChild;
        child = index * 2 + 1;
    }
}

void TimerManager::SwapHeap(size_t index1, size_t index2)
{
    HeapEntry tmp = heap_[index1];
    heap_[index1] = heap_[index2];
    heap_[index2] = tmp;
    heap_[index1].timer->heapIndex_ = index1;
    heap_[index2].timer->heapIndex_ = index2;
}

unsigned long long TimerManager::GetCurrentMillisecs()
{
#ifdef _WIN32
    _timeb timebuffer;
    _ftime(&timebuffer);
    unsigned long long ret = timebuffer.time;
    ret = ret * 1000 + timebuffer.millitm;
    return ret;
#else
    timeval tv;
    ::gettimeofday(&tv, 0);
    unsigned long long ret = tv.tv_sec;
    return ret * 1000 + tv.tv_usec / 1000;
#endif
}