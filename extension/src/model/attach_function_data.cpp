#pragma once
#include "duckdb.hpp"

#include "paged_mysql_state.hpp"

using namespace duckdb;

struct AttachFunctionData : public TableFunctionData, public PagedMysqlState
{
	AttachFunctionData()
	{
	}

	bool finished = false;
	string source_schema = "public";
	string sink_schema = "main";
	string suffix = "";
	bool overwrite = false;
	bool filter_pushdown = true;

	idx_t approx_number_of_pages = 0;
	idx_t pages_per_task = 1000;

	string host;
	string username;
	string password;

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
};