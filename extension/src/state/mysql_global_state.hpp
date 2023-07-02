#pragma once

#include "duckdb.hpp"
#include "connection_pool.hpp"

using namespace duckdb;

struct MysqlGlobalState : public GlobalTableFunctionState
{

	~MysqlGlobalState()
	{
		if (pool)
		{
			pool->close();
			pool = nullptr;
		}
	}

	MysqlGlobalState(idx_t max_threads) : start_page(0), max_threads(max_threads)
	{
	}

	mutex lock;
	idx_t start_page;
	idx_t max_threads;
	ConnectionPool *pool = nullptr;

	idx_t MaxThreads() const override
	{
		return max_threads;
	}
};
