#include "connection_pool.hpp"

ConnectionPool::ConnectionPool(int poolSize, const std::string &host, const std::string &username, const std::string &password)
{
  // std::cout << "Creating connection pool with size " << poolSize << std::endl;

  std::vector<std::thread> threads(poolSize);
  driver = sql::mysql::get_mysql_driver_instance();

  for (int i = 0; i < poolSize; ++i)
  {
    threads[i] = std::thread([this, host, username, password]()
           {
            // std::cout << "Creating connection host " << host << " username " << username << " password " << password << std::endl;
            try {
               sql::Connection* connection = driver->connect(host, username, password);
               // std::cout << "Connection created" << std::endl;
               // Add a lock to ensure mutual exclusion when accessing the connections vector
               std::lock_guard<std::mutex> lock(connectionsMutex);
               connections.push(connection);
            } catch (sql::SQLException &e) {
              std::cerr << "Error connecting to database" << std::endl;
              std::cerr << e.what() << std::endl;
              std::cerr << e.getErrorCode() << std::endl;
              std::cerr << e.getSQLState() << std::endl;
            } catch (...) {
              std::cerr <<  "Unknown Error connecting to database" << std::endl;
            }

         });
  }



  // std::cout << "Waiting for threads to finish" << std::endl;
  // Wait for all threads to finish
  for (std::thread &thread : threads)
  {
    thread.join();
    //std::cout << "Thread finished" << std::endl;
  }
  //std::cout << "Threads finished" << std::endl;
}

sql::Connection *ConnectionPool::getConnection()
{
  // Add a lock to ensure mutual exclusion when accessing the connections vector
  std::lock_guard<std::mutex> lock(connectionsMutex);
  // std::cout << "Retrieving connection from pool" << std::endl;
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
  // std::cout << "Releasing connection back to pool" << std::endl;
  // Add the released connection back to the pool for reuse
  connections.push(connection);
}

void ConnectionPool::close()
{
  // Add a lock to ensure mutual exclusion when accessing the connections vector
  std::lock_guard<std::mutex> lock(connectionsMutex);
  // std::cout << "ConnectionPool :: Closing connection pool" << std::endl;
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
  // std::cout << "Destroying connection pool" << std::endl;
  close();
}
