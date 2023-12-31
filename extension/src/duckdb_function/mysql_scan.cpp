#include "duckdb.hpp"
#include <thread>
#include <future>
#include "mysql_connection_manager.hpp"

#include "../model/mysql_bind_data.hpp"
#include "../state/mysql_local_state.hpp"
#include "../state/mysql_global_state.hpp"
#include "../transformer/duckdb_to_mysql_request.cpp"
#include "../transformer/mysql_to_duckdb_result.cpp"
#include "../model/attach_function_data.cpp"
#include <spdlog/spdlog.h>


using namespace duckdb;

static idx_t MysqlMaxThreads(ClientContext &context, const FunctionData *bind_data_p)
{
	D_ASSERT(bind_data_p);

	const PagedMysqlState* bind_data = nullptr;

	if (auto attach_data = dynamic_cast<const AttachFunctionData*>(bind_data_p)) {
			bind_data = attach_data;
	} else if (auto mysql_data = dynamic_cast<const MysqlBindData*>(bind_data_p)) {
			bind_data = mysql_data;
	} else {
			//TODO handle the case when bind_data_p doesn't point to AttachFunctionData or MysqlBindData
	}

	return bind_data->get_approx_number_of_pages() / bind_data->get_pages_per_task();
}

static void MysqlInitPerTaskInternal(ClientContext &context, const MysqlBindData *bind_data_p,
																		 MysqlLocalState &lstate, idx_t page_start_at, idx_t page_to_fetch)
{
	D_ASSERT(bind_data_p);

	auto bind_data = (const MysqlBindData *)bind_data_p;

	// Queries like SELECT count(*) do not require actually returning the columns from the
	// Mysql table, but only counting the row. In this case, we will be asked to return
	// the 'rowid' special column here. It must be the only selected column. The corresponding
	// deparsed query will be 'SELECT NULL'..Note that the user is not allowed to explicitly
	// request the 'rowid' special column from a Mysql table in a SQL query.
	// TODO check that it works on MySQL too
	// bool have_rowid = false;
	// for (idx_t i = 0; i < lstate.column_ids.size(); i++)
	// {
	// 	if (lstate.column_ids[i] == (column_t)-1)
	// 	{
	// 		have_rowid = true;
	// 		break;
	// 	}
	// }

	// if (have_rowid && lstate.column_ids.size() > 1)
	// {
	// 	throw InternalException("Cannot return ROW_ID from Mysql table");
	// }

	// if (have_rowid)
	// {
	// 	// We are only counting rows, not interested in the actual values of the columns.
	// 	col_names = "NULL";
	// }
	// else
	// {

	lstate.base_sql = DuckDBToMySqlRequest(bind_data_p, lstate);

	lstate.start_page = page_start_at;
	lstate.end_page = page_start_at + page_to_fetch;
	lstate.pagesize = STANDARD_VECTOR_SIZE;

	// spdlog::debug("lstate.start_page: " << lstate.start_page <<);
	// spdlog::debug("lstate.end_page: " << lstate.end_page <<);

	// spdlog::debug("BASE SQL: " << lstate.base_sql <<);

	lstate.exec = false;
	lstate.done = false;

	lstate.stmt = lstate.conn->createStatement();

	auto sql = StringUtil::Format(
			R"(
					%s LIMIT %d OFFSET %d;
				)",
			lstate.base_sql, lstate.pagesize * lstate.end_page, lstate.start_page * lstate.current_page);

	spdlog::debug("running sql: {}", sql);
	lstate.result_set = lstate.stmt->executeQuery(sql);
	if (lstate.result_set->rowsCount() == 0)
	{ // done here, lets try to get more
		spdlog::debug("done reading, result set empty");
		lstate.done = true;
	}
}

static bool MysqlParallelStateNext(ClientContext &context, const FunctionData *bind_data_p,
																	 MysqlLocalState &lstate, MysqlGlobalState &gstate)
{
	D_ASSERT(bind_data_p);
	auto bind_data = (const MysqlBindData *)bind_data_p;

	// spdlog::debug("MysqlParallelStateNext: parallel_lock" <<);
	lock_guard<mutex> parallel_lock(gstate.lock);

	// spdlog::debug("MysqlParallelStateNext: gstate.page_to_fetch: " << bind_data->approx_number_of_pages <<);
	if (gstate.start_page < bind_data->approx_number_of_pages)
	{
		MysqlInitPerTaskInternal(context, bind_data, lstate, gstate.start_page, bind_data->approx_number_of_pages);
		gstate.start_page += bind_data->pages_per_task;
		return true;
	}
	else
	{
		lstate.done = true;
		return false;
	}
}

static string MysqlScanToString(const FunctionData *bind_data_p)
{
	D_ASSERT(bind_data_p);

	auto bind_data = (const MysqlBindData *)bind_data_p;
	return bind_data->table_name;
}

#define MYSQL_EPOCH_JDATE 2451545 /* == date2j(2000, 1, 1) */

static void MysqlScan(ClientContext &context, TableFunctionInput &data, DataChunk &output)
{
	auto &bind_data = data.bind_data->Cast<MysqlBindData>();
	auto &local_state = data.local_state->Cast<MysqlLocalState>();
	auto &gstate = data.global_state->Cast<MysqlGlobalState>();

	while (true)
	{
		// spdlog::debug("while true..." <<);
		if (local_state.done && !MysqlParallelStateNext(context, data.bind_data.get(), local_state, gstate))
		{
			// spdlog::debug("OUTPUT (size):" << output.size() <<);
			// output.Print();
			// spdlog::debug("Annnnnd we're done" <<);
			return;
		}

		idx_t output_offset = 0;

		// spdlog::debug("reading result set" <<);

		// iterate over the result set and write the result in the output data chunk
		while (local_state.result_set->next() && output_offset < STANDARD_VECTOR_SIZE)
		{
			// spdlog::debug("reading row: " << output_offset <<);
			// for each column from the bind data, read the value and write it to the result vector
			for (idx_t query_col_idx = 0; query_col_idx < output.ColumnCount(); query_col_idx++)
			{
				//spdlog::debug("ITERATE result set/ query_col_idx: " << query_col_idx << " output_offset: " << output_offset <<);
				auto table_col_idx = local_state.column_ids[query_col_idx];
				auto &out_vec = output.data[query_col_idx];
				//spdlog::debug("bind_data.names[table_col_idx] " << bind_data.names[table_col_idx] <<);
				//spdlog::debug("bind_data.types[table_col_idx] " << bind_data.types[table_col_idx].ToString() <<);
				//spdlog::debug("bind_data.columns[table_col_idx].type_info " << bind_data.columns[table_col_idx].type_info.name <<);
				//  print the size of output data
				ProcessValue(local_state.result_set,
										 bind_data.types[table_col_idx],
										 &bind_data.columns[table_col_idx].type_info,
										 out_vec,
										 query_col_idx,
										 output_offset);
			}

			output_offset++;

			// spdlog::debug("Result set done, final output_offset " << output_offset <<);

			output.SetCardinality(output_offset);
		}

		// spdlog::debug("output vector: " << output.size() <<);

		//  spdlog::debug("local_state.current_page: " << local_state.current_page <<);
		local_state.current_page += 1;

		if (local_state.current_page == local_state.end_page)
		{
			// spdlog::debug("current = end => done" <<);
			local_state.done = true;
		}

		return;
	}
}

static unique_ptr<GlobalTableFunctionState> MysqlInitGlobalState(ClientContext &context,
																																 TableFunctionInitInput &input)
{
	return make_uniq<MysqlGlobalState>(
			MysqlMaxThreads(context, input.bind_data.get()));
}

static unique_ptr<LocalTableFunctionState> MysqlInitLocalState(ExecutionContext &context,
																															 TableFunctionInitInput &input,
																															 GlobalTableFunctionState *global_state)
{
	auto &bind_data = input.bind_data->Cast<MysqlBindData>();
	auto &gstate = global_state->Cast<MysqlGlobalState>();

	auto local_state = make_uniq<MysqlLocalState>();
	local_state->column_ids = input.column_ids;
	local_state->pool = MySQLConnectionManager::getConnectionPool(1, 5, bind_data.host, bind_data.username, bind_data.password);
	local_state->conn = (local_state->pool)->getConnection();
	local_state->filters = input.filters.get();

	if (!MysqlParallelStateNext(context.client, input.bind_data.get(), *local_state, gstate))
	{
		local_state->done = true;
	}
	return std::move(local_state);
}

static int64_t GetApproxNumberOfPageForTable(ConnectionPool* connection_pool, std::string schema_name, std::string table_name){
	auto conn = connection_pool->getConnection();
	auto stmt1 = conn->createStatement();
	int64_t approx_number_of_pages = 0;

	auto res1 = stmt1->executeQuery(
			StringUtil::Format(
					R"(SELECT CEIL(COUNT(*) / %d) FROM %s.%s)",
					STANDARD_VECTOR_SIZE, schema_name, table_name));
	if (res1->rowsCount() != 1)
	{
		throw InvalidInputException("Mysql table \"%s\".\"%s\" not found", schema_name,
																table_name);
	}
	if (res1->next())
	{
		approx_number_of_pages = res1->getInt64(1);
	}
	else
	{
		// Handle the case where no rows were returned
		// or unable to move to the first row
		throw InvalidInputException("Failed to fetch data from result set");
	}

	res1->close();
	stmt1->close();
	connection_pool->releaseConnection(conn);
	return approx_number_of_pages;
}

static std::tuple<vector<MysqlColumnInfo>, vector<string>, vector<LogicalType>, vector<bool>> GetTableTypesInfos(ConnectionPool* connection_pool, std::string schema_name, std::string table_name){

	vector<MysqlColumnInfo> columns;
	vector<string> names;
	vector<LogicalType> types;
	vector<bool> needs_cast;

	auto conn = connection_pool->getConnection();
	auto stmt2 = conn->createStatement();
	auto res2 = stmt2->executeQuery(StringUtil::Format(
			R"(
			SELECT column_name,
						 DATA_TYPE,
       			 character_maximum_length,
						 numeric_precision,
						 numeric_scale,
						 IF(DATA_TYPE = 'enum', SUBSTRING(COLUMN_TYPE,5), NULL) enum_values
			FROM   information_schema.columns 
			WHERE  table_schema = '%s'
			AND 	 table_name = '%s';
			)",
			schema_name, table_name));

	// can't scan a table without columns (yes those exist)
	if (res2->rowsCount() == 0)
	{
		throw InvalidInputException("Table %s.%s does not contain any columns OR does not exist in the DB.", schema_name, table_name);
	}

	// set the column types in a MysqlColumnInfo struct by iterating over the result set
	while (res2->next())
	{
		MysqlColumnInfo info;
		info.column_name = res2->getString(1);
		info.type_info.name = res2->getString(2);
		info.type_info.char_max_length = res2->getInt(3);
		info.type_info.numeric_precision = res2->getInt(4);
		info.type_info.numeric_scale = res2->getInt(5);
		info.type_info.enum_values = res2->getString(6);

		names.push_back(info.column_name);

		auto duckdb_type = DuckDBType(info);
		// we cast unsupported types to varchar on read
		auto col_needs_cast = duckdb_type == LogicalType::INVALID;
		needs_cast.push_back(col_needs_cast);
		if (!col_needs_cast)
		{
			types.push_back(std::move(duckdb_type));
		}
		else
		{
			types.push_back(LogicalType::VARCHAR);
		}

		columns.push_back(info);
	}
	res2->close();
	stmt2->close();

	connection_pool->releaseConnection(conn);

	return std::make_tuple(columns, names, types, needs_cast);

}

static unique_ptr<FunctionData> MysqlBind(ClientContext &context, TableFunctionBindInput &input,
																					vector<LogicalType> &return_types, vector<string> &names)
{

  spdlog::debug("MysqlBind");
	auto bind_data = make_uniq<MysqlBindData>();

	bind_data->host = input.inputs[0].GetValue<string>();
	bind_data->username = input.inputs[1].GetValue<string>();
	bind_data->password = input.inputs[2].GetValue<string>();

	bind_data->schema_name = input.inputs[3].GetValue<string>();
	bind_data->table_name = input.inputs[4].GetValue<string>();

	auto connection_pool = MySQLConnectionManager::getConnectionPool(1, 5, bind_data->host, bind_data->username, bind_data->password);

	// // Create threads for concurrent execution
  //   std::thread t1(GetNumberOfShard, connection_pool, bind_data.get());
  //   std::thread t2(GetTableTypesInfos, connection_pool, bind_data.get());

  //   // Wait for both threads to finish
  //   t1.join();
  //   t2.join();
	
	// print current time with milliseconds and append GetApproxNumberOfPageForTable
	spdlog::debug("GetApproxNumberOfPageForTable");

	//spdlog::debug("Current time: " << std::ctime(&result) << " GetApproxNumberOfPageForTable " << bind_data->table_name <<);

	std::future<int64_t> fut = std::async (GetApproxNumberOfPageForTable, connection_pool, bind_data->schema_name, bind_data->table_name);
	spdlog::debug("GetTableTypesInfos");
	auto columns_tuple = GetTableTypesInfos(connection_pool, bind_data->schema_name, bind_data->table_name);
	spdlog::debug("GetTableTypesInfos DONE");
	bind_data->columns = std::get<0>(columns_tuple);
	bind_data->names = std::get<1>(columns_tuple);
	bind_data->types = std::get<2>(columns_tuple);
	bind_data->needs_cast = std::get<3>(columns_tuple);
	auto nb_of_pages = fut.wait_until(std::chrono::system_clock::now() + std::chrono::seconds(30));
	if (nb_of_pages == std::future_status::timeout)
	{
		throw std::runtime_error("Timeout while fetching number of pages");
	} else {
		bind_data->approx_number_of_pages = fut.get();
	}
	spdlog::debug("GetApproxNumberOfPageForTable DONE");

	return_types = bind_data->types;
	names = bind_data->names;

	return std::move(bind_data);
}