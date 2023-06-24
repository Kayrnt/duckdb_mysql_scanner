#define DUCKDB_BUILD_LOADABLE_EXTENSION
#include "duckdb.hpp"
#include "mysql/include/mysql/jdbc.h"

#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include "duckdb/planner/table_filter.hpp"
#include "duckdb/planner/filter/conjunction_filter.hpp"
#include "duckdb/planner/filter/constant_filter.hpp"
#include "duckdb/parser/parsed_data/create_function_info.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/function/scalar_function.hpp"

using namespace duckdb;

static void getMySQLVersion(DataChunk &args, ExpressionState &state, Vector &result)
{
	sql::mysql::MySQL_Driver *driver;
	sql::Connection *con;
	sql::Statement *stmt;
	sql::ResultSet *res;
	std::string version;

	// Create a MySQL driver instance
	driver = sql::mysql::get_mysql_driver_instance();

	try
	{
		// Establish a MySQL connection
		con = driver->connect("tcp://localhost:3306", "root", "");

		// Create a statement object
		stmt = con->createStatement();

		// Execute the query to retrieve the MySQL version
		res = stmt->executeQuery("SELECT VERSION()");

		// Retrieve the result
		if (res->next())
		{
			version = res->getString(1);
			result.SetValue(0, Value(version));
		}
	}
	catch (sql::SQLException &e)
	{
		// Handle any errors that occur during the connection or query execution
		std::cout << "SQL Exception: " << e.what() << std::endl;
	}
	if (con != NULL)
	{
		con->close();
	}

	// Clean up
	delete res;
	delete stmt;
	delete con;
}

extern "C"
{
	DUCKDB_EXTENSION_API void mysql_scanner_init(duckdb::DatabaseInstance &db)
	{
		Connection con(db);
		con.BeginTransaction();
		auto &context = *con.context;
		auto &catalog = Catalog::GetSystemCatalog(context);

		duckdb::CreateScalarFunctionInfo mysql_version(
				ScalarFunction(
						"mysql_version",
						{},
						LogicalType::VARCHAR,
						getMySQLVersion));

		catalog.CreateFunction(context, mysql_version);

		con.Commit();
	}

	DUCKDB_EXTENSION_API const char *mysql_scanner_version()
	{
		return DuckDB::LibraryVersion();
	}
}