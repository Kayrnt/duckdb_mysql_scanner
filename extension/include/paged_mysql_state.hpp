#pragma once
#include "duckdb.hpp"

using namespace duckdb;

class PagedMysqlState
{
public:
	virtual idx_t get_approx_number_of_pages() const { return 0; };
	virtual idx_t get_pages_per_task() const { return 0; };
	virtual void set_approx_number_of_pages(idx_t approx_number_of_pages){};
	virtual void set_pages_per_task(idx_t pages_per_task){};
};