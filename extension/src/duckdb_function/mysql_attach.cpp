#include "duckdb.hpp"

#include "mysql_jdbc.h"
#include "mysql_connection_manager.hpp"
#include "../state/mysql_global_state.hpp"
#include "paged_mysql_state.hpp"
#include "../model/attach_function_data.cpp"

using namespace duckdb;


static unique_ptr<FunctionData> AttachBind(ClientContext &context, TableFunctionBindInput &input,
																					 vector<LogicalType> &return_types, vector<string> &names)
{

	auto result = make_uniq<AttachFunctionData>();
	result->host = input.inputs[0].GetValue<string>();
	result->username = input.inputs[1].GetValue<string>();
	result->password = input.inputs[2].GetValue<string>();

	for (auto &kv : input.named_parameters)
	{
		if (kv.first == "source_schema")
		{
			result->source_schema = StringValue::Get(kv.second);
		}
		else if (kv.first == "sink_schema")
		{
			result->sink_schema = StringValue::Get(kv.second);
		}
		else if (kv.first == "overwrite")
		{
			result->overwrite = BooleanValue::Get(kv.second);
		}
		else if (kv.first == "filter_pushdown")
		{
			result->filter_pushdown = BooleanValue::Get(kv.second);
		}
	}

	return_types.push_back(LogicalType::BOOLEAN);
	names.emplace_back("Success");
	return std::move(result);
}

static void AttachFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output)
{
	auto &data = (AttachFunctionData &)*data_p.bind_data;
	auto &gstate = (MysqlGlobalState &)*data_p.global_state;
	if (data.finished)
	{
		return;
	}

	gstate.pool = MySQLConnectionManager::getConnectionPool(1, 5, data.host, data.username, data.password);
	auto conn = gstate.pool->getConnection();

	auto dconn = Connection(context.db->GetDatabase(context));
	auto stmt = conn->createStatement();
	auto res = stmt->executeQuery(StringUtil::Format(
			R"(
			SELECT table_name
			FROM   information_schema.tables 
			WHERE  table_schema = '%s';
			)",
			data.source_schema));

	while (res->next())
	{
		auto mysql_str = res->getString(1);
		auto table_name = mysql_str.c_str();

		dconn
				.TableFunction(data.filter_pushdown ? "mysql_scan_pushdown" : "mysql_scan",
											 {Value(data.host), Value(data.username), Value(data.password), Value(data.source_schema), Value(table_name)})
				->CreateView(data.sink_schema, table_name, data.overwrite, false);
	}
	res->close();
	stmt->close();
	gstate.pool->releaseConnection(conn);

	data.finished = true;
}