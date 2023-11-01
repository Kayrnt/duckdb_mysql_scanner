#include "duckdb.hpp"
#include "connection_pool.hpp"
#include <spdlog/spdlog.h>

ConnectionPool::ConnectionPool(int minPoolSize, int maxPoolSize, const std::string& host, const std::string& username, const std::string& password):
minPoolSize(minPoolSize), maxPoolSize(maxPoolSize), host(host), username(username), password(password)
{
  // spdlog::debug("Creating connection pool with size " << poolSize <<);

  std::vector<std::thread> threads(minPoolSize);
  driver = sql::mysql::get_mysql_driver_instance();

  for (int i = 0; i < minPoolSize; ++i)
  {
    threads[i] = std::thread([this, host, username, password]()
           {
            // spdlog::debug("Creating connection host " << host << " username " << username << " password " << password <<);
            try {
               sql::Connection* connection = driver->connect(host, username, password);
               spdlog::info("Connection created");
               // Add a lock to ensure mutual exclusion when accessing the connections vector
               std::lock_guard<std::mutex> lock(connectionsMutex);
               connections.push(connection);
            } catch (sql::SQLException &e) {
              spdlog::error("Error connecting to database");
              spdlog::error(e.what());
              spdlog::error(e.getErrorCode());
              spdlog::error(e.getSQLState());
            } catch (...) {
              spdlog::error("Unknown Error connecting to database");
            }

         });
  }

  // spdlog::debug("Waiting for threads to finish" <<);
  // Wait for all threads to finish
  for (std::thread &thread : threads)
  {
    thread.join();
    //spdlog::debug("Thread finished" <<);
  }
  //spdlog::debug("Threads finished" <<);
}

sql::Connection *ConnectionPool::createConnection(int retryLeftCount) {
 if(retryLeftCount == 0){
  throw duckdb::InvalidInputException("Unable to create connection to the host %s with username %s", this->host, this->username);
 } else {
  try {
    return this->driver->connect(this->host, this->username, this->password);
  } catch (...) {
    return createConnection(retryLeftCount - 1);
  }
 }
}

sql::Connection *ConnectionPool::getConnection()
{
  // Add a lock to ensure mutual exclusion when accessing the connections vector
  std::lock_guard<std::mutex> lock(connectionsMutex);
  spdlog::info("Retrieving connection from pool");
  // Retrieve the next available connection in a round-robin fashion

  sql::Connection *connection = nullptr;

  if(connections.empty()) {
    spdlog::info("Connection pool is empty");
    connection = createConnection(3);
  } else {
  // front() returns a reference to the first element in the vector
    connection = connections.front();
    connections.pop();
  }

  // check that the connection is still valid
  if (connection->isValid()) {
    // spdlog::debug("Connection is valid" <<);
  } else {
    // spdlog::debug("Connection is invalid" <<);
    // if the connection is invalid, create a new one
    delete connection;
    connection = createConnection(3);
  }
  return connection;
}

void ConnectionPool::releaseConnection(sql::Connection *connection)
{
  // Add a lock to ensure mutual exclusion when accessing the connections vector
  std::lock_guard<std::mutex> lock(connectionsMutex);
  spdlog::info("Releasing connection back to pool");
  // Add the released connection back to the pool for reuse
  connections.push(connection);
}

void ConnectionPool::close()
{
  // Add a lock to ensure mutual exclusion when accessing the connections vector
  std::lock_guard<std::mutex> lock(connectionsMutex);
  // spdlog::debug("ConnectionPool :: Closing connection pool" <<);
  // Close all connections in the pool
  while (!connections.empty())
  {
    sql::Connection *connection = connections.front();
    connections.pop();
    connection->close();
    delete connection;
  }
}

ConnectionPool::~ConnectionPool()
{
  // spdlog::debug("Destroying connection pool" <<);
  close();
}
