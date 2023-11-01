// found https://gist.github.com/swkwon/7c70585e8787b4000fff looks like doing a minimal connection pool

#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <memory>
#include <queue>
#include <my_global.h>
#include <mysql.h>

class MysqlConnectionCreatetor
{
public:
	static MYSQL* CreateConnection(const char* host, unsigned int port, const char* user, const char* password, const char* database)
	{
		std::lock_guard<std::mutex> lk(lock_connection_);
		MYSQL* conn = mysql_init(nullptr);
		if (mysql_real_connect(conn, host, user, password, database, port, nullptr, 0) == nullptr)
		{
			mysql_close(conn);
			return nullptr;
		}
		return conn;
	}

private:
	static std::mutex lock_connection_;
};
std::mutex MysqlConnectionCreatetor::lock_connection_;

class Driver : public std::enable_shared_from_this < Driver >
{
public:
	typedef std::shared_ptr<Driver> SDriver;
	static Driver::SDriver create_driver(const char* host, unsigned int port, const char* user, const char* password, const char* database)
	{
		return Driver::SDriver(new Driver(host, port, user, password, database));
	}

	~Driver()
	{
		if (conn != nullptr)
			mysql_close(conn);
	}

	bool start()
	{
		if (conn == nullptr)
		{
			conn = MysqlConnectionCreatetor::CreateConnection(server_.c_str(), port_, user_.c_str(), password_.c_str(), database_.c_str());
			if (conn == nullptr)
				return false;
		}

		return true;
	}

	int execute(const char* query)
	{
		if (start() == false)
			return 1;

		int result = mysql_query(conn, query);
		if (result != 0)
		{
			mysql_close(conn);
			conn = nullptr;
			if (start() == false)
				return 1;

			result = mysql_query(conn, query);
		}
		return result;

	}

private:
	Driver(const char* host, unsigned int port, const char* user, const char* password, const char* database)
		: server_(host), port_(port), user_(user), password_(password), database_(database), conn(nullptr)
	{

	}

	std::string server_;
	unsigned int port_;
	std::string user_;
	std::string password_;
	std::string database_;

	MYSQL* conn;
};

class ConnectionManager
{
public:
	ConnectionManager(const char* host, unsigned int port, const char* user, const char* password, const char* database, int pool_count)
		:server_(host), port_(port), user_(user), password_(password), database_(database), pool_count_(pool_count)
	{}
	void start()
	{
		for (int i = 0; i < pool_count_; i++)
		{
			auto driver = create_driver(server_.c_str(), port_, user_.c_str(), password_.c_str(), database_.c_str());
			driver->start();
			conn_queue_.push(driver);
		}
	}

	~ConnectionManager()
	{
		std::lock_guard<std::mutex> lk(lock_queue_);
		while (conn_queue_.size() != 0)
			conn_queue_.pop();
	}

	Driver::SDriver get_driver()
	{
		while (true)
		{
			std::lock_guard<std::mutex> lk(lock_queue_);
			if (conn_queue_.size() == 0)
			{
				continue;
			}
			else
			{
				auto driver = conn_queue_.front();
				conn_queue_.pop();
				return driver;
			}
		}
	}

	void push(Driver::SDriver driver)
	{
		std::lock_guard<std::mutex> lk(lock_queue_);
		conn_queue_.push(driver);
	}

private:
	Driver::SDriver create_driver(const char* host, unsigned int port, const char* user, const char* password, const char* database)
	{
		return Driver::create_driver(host, port, user, password, database);
	}

	std::string server_;
	unsigned int port_;
	std::string user_;
	std::string password_;
	std::string database_;
	int pool_count_;
	std::queue<Driver::SDriver> conn_queue_;
	std::mutex lock_queue_;
};

class MysqlConnection
{
public:
	MysqlConnection(const char* host, unsigned int port, const char* user, const char* password, const char* database, int pool_count = 1)
		: manager(host, port, user, password, database, pool_count)
	{}

	~MysqlConnection()
	{

	}

	void Open()
	{
		manager.start();
	}

	Driver::SDriver get_driver()
	{
		return manager.get_driver();
	}

	void end(Driver::SDriver driver)
	{
		manager.push(driver);
	}

private:
	ConnectionManager manager;
};

/*class MysqlCommand
{
public:
	MysqlCommand(MysqlConnection* conn, const char* query)
		: conn_(conn), query_(query)
	{
		driver_ = conn_->get_driver();
	}

	~MysqlCommand()
	{
		conn_->end(driver_);
		driver_ = nullptr;
	}

	bool Execute()
	{
		if (driver_->execute(query_) == 0)
			return true;
		return false;
	}

private:
	MysqlConnection* conn_;
	const char* query_;
	Driver::SDriver driver_;
};*/

/*
Use example

void insert(MysqlConnection* conn, int loop)
{
	for (int i = 0; i < loop; i++)
	{
		std::ostringstream stringStream;
		stringStream << "insert into tbl_test(first, second) values(" << i << ", 'hoho');";
		std::string str = stringStream.str();

		MysqlCommand cmd(conn, str.c_str());
		cmd.Execute();
	}
}


int main(int argc, char* argv[])
{
	MysqlConnection* c = new MysqlConnection("10.10.30.137", 23306, "root", "1234", "TestDB", 3);
	c->Open();

	std::thread t1(insert, c, 100);
	std::thread t2(insert, c, 100);
	t1.join();
	t2.join();

	return 0;
}
*/