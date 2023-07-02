#pragma once

#include <mysql/jdbc.h>
#include <thread>
#include <mutex>
#include <map>
#include <vector>
#include <string>

class ConnectionPool
{
private:
  sql::mysql::MySQL_Driver *driver;
  std::vector<sql::Connection *> connections;
  std::mutex connectionsMutex;

public:
  ConnectionPool(int poolSize, const std::string& host, const std::string& username, const std::string& password);
  sql::Connection *getConnection();
  void releaseConnection(sql::Connection *connection);
  void close();
  ~ConnectionPool();
};
