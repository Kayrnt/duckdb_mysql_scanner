#include "connection_pool.hpp"

ConnectionPool::ConnectionPool(int poolSize, const std::string &host, const std::string &username, const std::string &password)
{
  std::cout << "Creating connection pool with size " << poolSize << std::endl;
  driver = sql::mysql::get_mysql_driver_instance();

  std::vector<std::thread> threads(poolSize);

  for (int i = 0; i < poolSize; ++i)
  {
    threads[i] = std::thread([this, host, username, password]()
                             {
            sql::Connection* connection = driver->connect(host, username, password);

            // Add a lock to ensure mutual exclusion when accessing the connections vector
            std::lock_guard<std::mutex> lock(connectionsMutex);

            connections.push(connection); });
  }

  // Wait for all threads to finish
  for (std::thread &thread : threads)
  {
    thread.join();
  }
}

sql::Connection *ConnectionPool::getConnection()
{
  // Add a lock to ensure mutual exclusion when accessing the connections vector
  std::lock_guard<std::mutex> lock(connectionsMutex);
  std::cout << "Retrieving connection from pool" << std::endl;
  // Retrieve the next available connection in a round-robin fashion

  // front() returns a reference to the first element in the vector
  sql::Connection *connection = connections.front();
  connections.pop();
  return connection;
}

void ConnectionPool::releaseConnection(sql::Connection *connection)
{
  // Add a lock to ensure mutual exclusion when accessing the connections vector
  std::lock_guard<std::mutex> lock(connectionsMutex);
  std::cout << "Releasing connection back to pool" << std::endl;
  // Add the released connection back to the pool for reuse
  connections.push(connection);
}

void ConnectionPool::close()
{
  // Add a lock to ensure mutual exclusion when accessing the connections vector
  std::lock_guard<std::mutex> lock(connectionsMutex);
  std::cout << "ConnectionPool :: Closing connection pool" << std::endl;
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
  std::cout << "Destroying connection pool" << std::endl;
  close();
}
