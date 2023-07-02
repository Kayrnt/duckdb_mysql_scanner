#include "mysql_connection_manager.hpp"

std::map<std::tuple<std::string, std::string, std::string>, ConnectionPool*> MySQLConnectionManager::connectionMap;
std::mutex MySQLConnectionManager::mapMutex;

ConnectionPool* MySQLConnectionManager::getConnectionPool(int poolSize, const std::string& host, const std::string& username, const std::string& password) {
    // std::cout << "Retrieving connection pool" << std::endl;
    std::lock_guard<std::mutex> lock(mapMutex);

    auto key = std::make_tuple(host, username, password);
    auto it = connectionMap.find(key);

    if (it != connectionMap.end()) {
        // std::cout << "Connection pool already exists, return existing!" << std::endl;
        // ConnectionPool already exists, return the existing instance
        return it->second;
    }

    // std::cout << "Connection pool doesn't exist, create new!" << std::endl;
    // ConnectionPool doesn't exist, create a new instance and add it to the map
    ConnectionPool* connectionPool = new ConnectionPool(poolSize, host, username, password);
    connectionMap[key] = connectionPool;
    return connectionPool;
}

void MySQLConnectionManager::close(const std::string& host, const std::string& username, const std::string& password) {
    // std::cout << "MySQLConnectionManager :: Closing connection pool" << std::endl;
    std::lock_guard<std::mutex> lock(mapMutex);

    auto key = std::make_tuple(host, username, password);
    auto conn = connectionMap.find(key);

    if (conn != connectionMap.end()) {
        conn->second->close();
        delete conn->second;
        connectionMap.erase(conn);
    }
}

MySQLConnectionManager::~MySQLConnectionManager() {
    // std::cout << "Destroying connection manager" << std::endl;
    std::lock_guard<std::mutex> lock(mapMutex);

    for (auto& conn : connectionMap) {
        conn.second->close();
        delete conn.second;
    }
    connectionMap.clear();
}
