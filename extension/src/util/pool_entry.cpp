#include <atomic>
#include <vector>
#include <chrono>
#include <future>
#include <stdexcept>

#include <mysql/jdbc.h>
#include "concurrent_bag.cpp"

class Connection;
class Statement;

class PoolBase
{
public:
  virtual bool recycle(PoolEntry *entry) = 0;
  // implementation for PoolBase
};

class HikariPool : public PoolBase
{
public:
  bool recycle(PoolEntry *entry)
  {
    // implementation for recycle
    return true;
  }
  bool closeConnection(PoolEntry *entry, std::string closureReason)
  {
    // implementation for closeConnection
  }

  std::string toString()
  {
    // implementation for toString
    return "";
  }

  void resetConnectionState(sql::Connection *connection, sql::Connection *proxyConnection, int dirtyBits)
  {
    // implementation for resetConnectionState
  }
  // implementation for HikariPool
};

class ProxyLeakTask
{
  // implementation for ProxyLeakTask
};

class ProxyConnection
{
  // implementation for ProxyConnection
};

class ProxyFactory
{
public:
  sql::Connection getProxyConnection(PoolEntry *entry, sql::Connection *connection, std::vector<sql::Statement *> openStatements, ProxyLeakTask *leakTask, bool isReadOnly, bool isAutoCommit)
  {
    // throw a non implemented exception
    throw std::runtime_error("Not implemented");
  }
  // implementation for ProxyFactory
};

class PoolEntry
{
private:
  static std::atomic<int> stateUpdater;

  sql::Connection *connection;
  long lastAccessed;
  long lastBorrowed;

  volatile int state;
  volatile bool evict;

  volatile std::future<void> endOfLife;
  volatile std::future<void> keepalive;

  std::vector<sql::Statement *> openStatements;
  HikariPool *hikariPool;

  bool isReadOnly;
  bool isAutoCommit;

  static std::atomic<int> stateUpdater;

  long currentTime()
  {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  }

  long elapsedMillis(long startTime)
  {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto currentMillis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    return currentMillis - startTime;
  }

public:
  PoolEntry(sql::Connection *connection, PoolBase *pool, bool isReadOnly, bool isAutoCommit)
      : connection(connection), hikariPool(dynamic_cast<HikariPool *>(pool)), isReadOnly(isReadOnly), isAutoCommit(isAutoCommit)
  {
    lastAccessed = currentTime();
    openStatements = std::vector<sql::Statement *>();
  }

  void recycle()
  {
    if (connection != nullptr)
    {
      lastAccessed = currentTime();
      hikariPool->recycle(this);
    }
  }

  // void setFutureEol(std::future<void> endOfLife)
  // {
  //   this->endOfLife = std::move(endOfLife);
  // }

  // void setKeepalive(std::future<void> keepalive)
  // {
  //   this->keepalive = std::move(keepalive);
  // }

  sql::Connection *createProxyConnection(ProxyLeakTask *leakTask)
  {
    throw std::runtime_error("Not implemented");
    // return ProxyFactory::getProxyConnection(this, connection, openStatements, leakTask, isReadOnly, isAutoCommit);
  }

  void resetConnectionState(sql::Connection *connection, int dirtyBits)
  {
    hikariPool->resetConnectionState(connection, connection, dirtyBits);
  }

  std::string getPoolName()
  {
    return hikariPool->toString();
  }

  bool isMarkedEvicted()
  {
    return evict;
  }

  void markEvicted()
  {
    evict = true;
  }

  void evict(std::string closureReason)
  {
    hikariPool->closeConnection(this, closureReason);
  }

  long getMillisSinceBorrowed()
  {
    return elapsedMillis(lastBorrowed);
  }

  PoolBase *getPoolBase()
  {
    return hikariPool;
  }

  int getState()
  {
    return stateUpdater.load(std::memory_order_acquire);
  }

  bool compareAndSet(int expect, int update)
  {
    return stateUpdater.compare_exchange_strong(expect, update, std::memory_order_acq_rel);
  }

  void setState(int update)
  {
    stateUpdater.store(update, std::memory_order_release);
  }

  sql::Connection *close()
  {
    // auto eol = endOfLife;
    // if (eol.valid() && eol.wait_for(std::chrono::seconds(0)) != std::future_status::ready && !eol.cancel())
    // {
    //   // handle cancellation failure
    // }

    // auto ka = keepalive;
    // if (ka.valid() && ka.wait_for(std::chrono::seconds(0)) != std::future_status::ready && !ka.cancel())
    // {
    //   // handle cancellation failure
    // }

    sql::Connection *con = connection;
    connection = nullptr;
    // endOfLife = std::future<void>();
    // keepalive = std::future<void>();
    return con;
  }

  static const int STATE_NOT_IN_USE = 0;
  static const int STATE_IN_USE = 1;
  static const int STATE_REMOVED = -1;
  static const int STATE_RESERVED = -2;

  std::string stateToString()
  {
    switch (state)
    {
    case STATE_IN_USE:
      return "IN_USE";
    case STATE_NOT_IN_USE:
      return "NOT_IN_USE";
    case STATE_REMOVED:
      return "REMOVED";
    case STATE_RESERVED:
      return "RESERVED";
    default:
      return "Invalid";
    }
  }
};

// Definitions for static members
std::atomic<int> PoolEntry::stateUpdater;
