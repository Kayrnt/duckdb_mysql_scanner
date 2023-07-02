#include <iostream>
#include <vector>
#include <list>
#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <functional>

#include "synchronous_queue.cpp"

long elapsedNanos(const std::chrono::steady_clock::time_point& start, const std::chrono::steady_clock::time_point& end)
{
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return duration.count();
}


std::chrono::steady_clock::time_point currentTime()
{
    return std::chrono::steady_clock::now();
}

// Forward declaration
class ConcurrentBagEntry;

template <typename T>
class ConcurrentBag
{
public:
  class IConcurrentBagEntry
  {
  public:
    static const int STATE_NOT_IN_USE = 0;
    static const int STATE_IN_USE = 1;
    static const int STATE_REMOVED = -1;
    static const int STATE_RESERVED = -2;

    virtual bool compareAndSet(int expectState, int newState) = 0;
    virtual void setState(int newState) = 0;
    virtual int getState() = 0;
  };

  class IBagStateListener
  {
  public:
    virtual void addBagItem(int waiting) = 0;
  };

  ConcurrentBag(IBagStateListener *listener) : listener(listener),
                                               weakThreadLocals(useWeakThreadLocals()),
                                               closed(false)
  {
    handoffQueue.reset(new SynchronousQueue<T>());
    waiters.reset(new std::atomic_int());
    sharedList.reset(new std::list<T>());
    if (weakThreadLocals)
    {
      threadList.reset(new std::vector<std::unique_ptr<std::list<T>>>());
      threadList->emplace_back(new std::list<T>());
    }
    else
    {
      threadList.reset(new std::unique_ptr<std::list<T>>(new std::list<T>()));
    }
  }

  virtual ~ConcurrentBag()
  {
    close();
  }

  void close()
  {
    closed = true;
    drainSharedList();
  }

  void add(T item)
  {
    sharedList->push_back(item);
  }

  T borrow(long timeout)
  {
    threadList->get().push_back(nullptr);

    T item = borrowFromThreadLocal();
    if (item != nullptr)
    {
      return item;
    }

    const auto start = currentTime();
    do
    {
      item = borrowFromSharedList();
      if (item != nullptr)
      {
        return item;
      }

      waiters->fetch_add(1);
      item = handoffQueue->poll(timeout);

      if (item != nullptr || elapsedNanos(start, currentTime()) > timeout)
      {
        break;
      }
    } while (true);

    if (item == nullptr)
    {
      waiters->fetch_sub(1);
    }

    return item;
  }

  void requite(T item)
  {
    if (item->compareAndSet(IConcurrentBagEntry::STATE_IN_USE, IConcurrentBagEntry::STATE_NOT_IN_USE))
    {
      threadList->get().emplace_back(item);
    }
    else
    {
      sharedList->remove(item);
    }

    handoffQueue->offer(item);
    if (waiters->load() > 0)
    {
      handoffQueue->poll();
    }
  }

private:
  IBagStateListener *listener;
  bool weakThreadLocals;
  bool closed;
  std::unique_ptr<SynchronousQueue<T>> handoffQueue;
  std::unique_ptr<std::atomic_int> waiters;
  std::unique_ptr<std::list<T>> sharedList;
  std::unique_ptr<std::vector<T>> threadList;

  static const bool useWeakThreadLocals()
  {
    // Implementation specific, adapt accordingly
    return true;
  }

  T borrowFromThreadLocal()
  {
    std::vector<void *> &list = threadList->get().back();
    if (!list.empty())
    {
      T item = static_cast<T>(list.back());
      list.pop_back();
      if (item->compareAndSet(IConcurrentBagEntry::STATE_NOT_IN_USE, IConcurrentBagEntry::STATE_IN_USE))
      {
        return item;
      }
      else
      {
        return nullptr;
      }
    }
    return nullptr;
  }

  T borrowFromSharedList()
  {
    for (auto it = sharedList->begin(); it != sharedList->end(); ++it)
    {
      T item = *it;
      if (item->compareAndSet(IConcurrentBagEntry::STATE_NOT_IN_USE, IConcurrentBagEntry::STATE_IN_USE))
      {
        sharedList->erase(it);
        return item;
      }
    }
    return nullptr;
  }

  void drainSharedList()
  {
    for (auto &item : *sharedList)
    {
      item->setState(IConcurrentBagEntry::STATE_REMOVED);
      handoffQueue->offer(item);
    }
    sharedList->clear();
  }
};
