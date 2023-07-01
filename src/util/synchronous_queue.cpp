#include <iostream>
#include <vector>
#include <list>
#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <functional>

template <typename T>
class SynchronousQueue
{
public:
    SynchronousQueue(bool fair = false) :
        fair(fair)
    {}

    void offer(T item)
    {
        std::unique_lock<std::mutex> lock(mutex);
        queue.push_back(item);
        conditionVariable.notify_all();
    }

    T poll(long timeout)
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (queue.empty())
        {
            conditionVariable.wait_for(lock, std::chrono::milliseconds(timeout));
        }

        if (!queue.empty())
        {
            T item = queue.front();
            queue.pop_front();
            return item;
        }

        return nullptr;
    }

private:
    std::mutex mutex;
    std::condition_variable conditionVariable;
    std::list<T> queue;
    bool fair;
};