#define DUCKDB_EXTENSION_MAIN

#include "duckdb.hpp"
#include <iostream>
#include <mysql/jdbc.h>
#include "mysqlscanner_extension.hpp"

#include "model/mysql_bind_data.hpp"
#include "state/mysql_local_state.hpp"
#include "state/mysql_global_state.hpp"
#include "duckdb_function/mysql_scan.cpp"
#include "duckdb_function/mysql_attach.cpp"

#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include "duckdb/planner/table_filter.hpp"
#include "duckdb/parser/parsed_data/create_function_info.hpp"

namespace duckdb
{

	class MysqlScanFunction : public TableFunction
	{
	public:
		MysqlScanFunction()
				: TableFunction("mysql_scan", {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR},
												MysqlScan, MysqlBind, MysqlInitGlobalState, MysqlInitLocalState)
		{
			to_string = MysqlScanToString;
			projection_pushdown = true;
		}
	};

	class MysqlScanFunctionFilterPushdown : public TableFunction
	{
	public:
		MysqlScanFunctionFilterPushdown()
				: TableFunction("mysql_scan_pushdown", {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR},
												MysqlScan, MysqlBind, MysqlInitGlobalState, MysqlInitLocalState)
		{
			to_string = MysqlScanToString;
			projection_pushdown = true;
			filter_pushdown = true;
		}
	};

	class MysqlAttachFunction : public TableFunction
	{
	public:
		MysqlAttachFunction()
				: TableFunction("mysql_attach", {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR}, AttachFunction, AttachBind)
		{
			named_parameters["overwrite"] = LogicalType::BOOLEAN;
			named_parameters["filter_pushdown"] = LogicalType::BOOLEAN;

			named_parameters["source_schema"] = LogicalType::VARCHAR;
			named_parameters["sink_schema"] = LogicalType::VARCHAR;
		}
	};

	static void LoadInternal(DatabaseInstance &instance)
	{
		Connection con(instance);
		con.BeginTransaction();
		auto &context = *con.context;
		auto &catalog = Catalog::GetSystemCatalog(context);

		MysqlScanFunction mysql_fun;
		CreateTableFunctionInfo mysql_info(mysql_fun);
		catalog.CreateTableFunction(context, mysql_info);

		MysqlScanFunctionFilterPushdown mysql_fun_filter_pushdown;
		CreateTableFunctionInfo mysql_filter_pushdown_info(mysql_fun_filter_pushdown);
		catalog.CreateTableFunction(context, mysql_filter_pushdown_info);

		MysqlAttachFunction attach_func;
		CreateTableFunctionInfo attach_info(attach_func);
		catalog.CreateTableFunction(context, attach_info);

		con.Commit();
	}

	void MysqlscannerExtension::Load(DuckDB &db)
	{
		LoadInternal(*db.instance);
	}

	std::string MysqlscannerExtension::Name()
	{
		return "mysqlscanner";
	}

} // namespace duckdb

extern "C"
{
	DUCKDB_EXTENSION_API void mysqlscanner_init(duckdb::DatabaseInstance &db)
	{
		LoadInternal(db);
	}

	DUCKDB_EXTENSION_API const char *mysqlscanner_version()
	{
		return duckdb::DuckDB::LibraryVersion();
	}
}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif