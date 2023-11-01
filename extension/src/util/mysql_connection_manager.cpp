#include "mysql_connection_manager.hpp"

std::map<std::tuple<std::string, std::string, std::string>, ConnectionPool *> MySQLConnectionManager::connectionMap;
std::mutex MySQLConnectionManager::mapMutex;

ConnectionPool *MySQLConnectionManager::getConnectionPool(
 int minPoolSize,
 int maxPoolSize,
 const std::string &host,
 const std::string &username,
 const std::string &password
 )
{
  // spdlog::debug("Retrieving connection pool" <<);
  
  std::lock_guard<std::mutex> lock(mapMutex);

  auto key = std::make_tuple(host, username, password);
  auto existing_connection_pool = connectionMap.find(key);

  if (existing_connection_pool != connectionMap.end())
  {
    // spdlog::debug("Connection pool already exists, return existing!" <<);
    // ConnectionPool already exists, return the existing instance
    return existing_connection_pool->second;
  }

  // spdlog::debug("Connection pool doesn't exist, create new!" <<);
  // ConnectionPool doesn't exist, create a new instance and add it to the map
  ConnectionPool *connectionPool = new ConnectionPool(minPoolSize, maxPoolSize, host, username, password);
  connectionMap[key] = connectionPool;
  return connectionPool;
}

void MySQLConnectionManager::close(const std::string &host, const std::string &username, const std::string &password)
{
  // spdlog::debug("MySQLConnectionManager :: Closing connection pool" <<);
  std::lock_guard<std::mutex> lock(mapMutex);

  auto key = std::make_tuple(host, username, password);
  auto connection_key = connectionMap.find(key);

  if (connection_key != connectionMap.end())
  {
    connection_key->second->close();
    delete connection_key->second;
    connectionMap.erase(connection_key);
  }
}

MySQLConnectionManager::~MySQLConnectionManager()
{
  // spdlog::debug("Destroying connection manager" <<);
  std::lock_guard<std::mutex> lock(mapMutex);

  for (auto &connection_key : connectionMap)
  {
    connection_key.second->close();
    delete connection_key.second;
  }
  connectionMap.clear();
}
