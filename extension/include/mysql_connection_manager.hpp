#pragma once

#include <map>
#include <mutex>
#include <string>
#include "connection_pool.hpp"

class MySQLConnectionManager {
private:
    static std::map<std::tuple<std::string, std::string, std::string>, ConnectionPool*> connectionMap;
    static std::mutex mapMutex;

public:
    static ConnectionPool* getConnectionPool(int poolSize, const std::string& host, const std::string& username, const std::string& password);
    static void close(const std::string& host, const std::string& username, const std::string& password);
    ~MySQLConnectionManager();
};
