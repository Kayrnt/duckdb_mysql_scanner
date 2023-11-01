#pragma once

#include <mysql/jdbc.h>
#include <thread>
#include <mutex>
#include <map>
#include <queue>
#include <string>

class ConnectionPool
{
private:
  sql::mysql::MySQL_Driver *driver;
  std::queue<sql::Connection*> connections = std::queue<sql::Connection*>();
  std::mutex connectionsMutex;
  int maxPoolSize;
  int minPoolSize;
  std::string host;
  std::string username;
  std::string password;

public:
  ConnectionPool(int minPoolSize, int maxPoolSize, const std::string& host, const std::string& username, const std::string& password);
  sql::Connection *createConnection(int retryLeftCount);
  sql::Connection *getConnection();
  void releaseConnection(sql::Connection *connection);
  void close();
  ~ConnectionPool();
};
