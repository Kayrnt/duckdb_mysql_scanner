#ifndef MYSQL_GLOBAL_STATE_HPP
#define MYSQL_GLOBAL_STATE_HPP

#include "duckdb.hpp"
#include "../../mysql/include/mysql/jdbc.h"

using namespace duckdb;

struct MysqlTypeInfo
{
	string name;
	int64_t char_max_length;
	int64_t numeric_precision;
	int64_t numeric_scale;
	string enum_values;
};

struct MysqlColumnInfo
{
	string column_name;
	MysqlTypeInfo type_info;
};

struct MysqlBindData : public FunctionData
{
	~MysqlBindData()
	{
	}

	string host;
	string username;
	string password;

	string schema_name;
	string table_name;
	idx_t approx_number_of_pages = 0;

	vector<MysqlColumnInfo> columns;
	vector<string> names;
	vector<LogicalType> types;
	vector<bool> needs_cast;

	idx_t pages_per_task = 1000;

	string snapshot;
	bool in_recovery;

public:
	unique_ptr<FunctionData> Copy() const override
	{
		throw NotImplementedException("");
	}
	bool Equals(const FunctionData &other) const override
	{
		throw NotImplementedException("");
	}
};

#endif // MYSQL_GLOBAL_STATE_HPP