#pragma once

#include "duckdb.hpp"
#include "mysql_jdbc.h"
#include "paged_mysql_state.hpp"

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

struct MysqlBindData : public FunctionData, public PagedMysqlState
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
	idx_t pages_per_task = 1000;

	vector<MysqlColumnInfo> columns;
	vector<string> names;
	vector<LogicalType> types;
	vector<bool> needs_cast;

	string snapshot;
	bool in_recovery;

public:
	idx_t get_approx_number_of_pages() const override
	{
		return approx_number_of_pages;
	}
	idx_t get_pages_per_task() const override
	{
		return pages_per_task;
	}
	void set_approx_number_of_pages(idx_t approx_number_of_pages) override
	{
		this->approx_number_of_pages = approx_number_of_pages;
	}
	void set_pages_per_task(idx_t pages_per_task) override
	{
		this->pages_per_task = pages_per_task;
	}
	
	unique_ptr<FunctionData> Copy() const override
	{
		throw NotImplementedException("");
	}
	bool Equals(const FunctionData &other) const override
	{
		throw NotImplementedException("");
	}
};
