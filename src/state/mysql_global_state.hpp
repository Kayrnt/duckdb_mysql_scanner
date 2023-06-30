#include "duckdb.hpp"

using namespace duckdb;

struct MysqlGlobalState : public GlobalTableFunctionState
{
	MysqlGlobalState(idx_t max_threads) : start_page(0), max_threads(max_threads)
	{
	}

	mutex lock;
	idx_t start_page;
	idx_t max_threads;

	idx_t MaxThreads() const override
	{
		return max_threads;
	}
};
