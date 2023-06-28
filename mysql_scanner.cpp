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
		if (conn)
		{
			conn->close();
			conn = nullptr;
		}
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

	sql::Connection *conn = nullptr;

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

static idx_t MysqlMaxThreads(ClientContext &context, const FunctionData *bind_data_p)
{
	D_ASSERT(bind_data_p);

	auto bind_data = (const MysqlBindData *)bind_data_p;
	return bind_data->approx_number_of_pages / bind_data->pages_per_task;
}

struct MysqlLocalState : public LocalTableFunctionState
{
	~MysqlLocalState()
	{
		if (conn)
		{
			conn->close();
			conn = nullptr;
		}
	}

	bool done = false;
	bool exec = false;
	string base_sql = "";

	idx_t start_page = 0;
	idx_t current_page = 0;
	idx_t end_page = 0;
	idx_t pagesize = 0;

	vector<column_t> column_ids;
	TableFilterSet *filters;
	sql::Connection *conn = nullptr;
};

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

static string TransformFilter(string &column_name, TableFilter &filter);

static string CreateExpression(string &column_name, vector<unique_ptr<TableFilter>> &filters, string op)
{
	vector<string> filter_entries;
	for (auto &filter : filters)
	{
		filter_entries.push_back(TransformFilter(column_name, *filter));
	}
	return "(" + StringUtil::Join(filter_entries, " " + op + " ") + ")";
}

static string TransformComparision(ExpressionType type)
{
	switch (type)
	{
	case ExpressionType::COMPARE_EQUAL:
		return "=";
	case ExpressionType::COMPARE_NOTEQUAL:
		return "!=";
	case ExpressionType::COMPARE_LESSTHAN:
		return "<";
	case ExpressionType::COMPARE_GREATERTHAN:
		return ">";
	case ExpressionType::COMPARE_LESSTHANOREQUALTO:
		return "<=";
	case ExpressionType::COMPARE_GREATERTHANOREQUALTO:
		return ">=";
	default:
		throw NotImplementedException("Unsupported expression type");
	}
}

static string TransformFilter(string &column_name, TableFilter &filter)
{
	switch (filter.filter_type)
	{
	case TableFilterType::IS_NULL:
		return column_name + " IS NULL";
	case TableFilterType::IS_NOT_NULL:
		return column_name + " IS NOT NULL";
	case TableFilterType::CONJUNCTION_AND:
	{
		auto &conjunction_filter = (ConjunctionAndFilter &)filter;
		return CreateExpression(column_name, conjunction_filter.child_filters, "AND");
	}
	case TableFilterType::CONJUNCTION_OR:
	{
		auto &conjunction_filter = (ConjunctionAndFilter &)filter;
		return CreateExpression(column_name, conjunction_filter.child_filters, "OR");
	}
	case TableFilterType::CONSTANT_COMPARISON:
	{
		auto &constant_filter = (ConstantFilter &)filter;
		// TODO properly escape ' in constant value
		auto constant_string = "'" + constant_filter.constant.ToString() + "'";
		auto operator_string = TransformComparision(constant_filter.comparison_type);
		return StringUtil::Format("%s %s %s", column_name, operator_string, constant_string);
	}
	default:
		throw InternalException("Unsupported table filter type");
	}
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

	std::string col_names;
	// if (have_rowid)
	// {
	// 	// We are only counting rows, not interested in the actual values of the columns.
	// 	col_names = "NULL";
	// }
	// else
	// {
	col_names = StringUtil::Join(
			lstate.column_ids.data(),
			lstate.column_ids.size(),
			", ",
			[&](const idx_t column_id)
			{ return StringUtil::Format("`%s`%s",
																	bind_data->names[column_id],
																	bind_data->needs_cast[column_id] ? "::VARCHAR" : ""); });
	// }

	string filter_string;
	if (lstate.filters && !lstate.filters->filters.empty())
	{
		vector<string> filter_entries;
		for (auto &entry : lstate.filters->filters)
		{
			// TODO properly escape " in column names
			auto column_name = "`" + bind_data->names[lstate.column_ids[entry.first]] + "`";
			auto &filter = *entry.second;
			filter_entries.push_back(TransformFilter(column_name, filter));
		}
		filter_string = " WHERE " + StringUtil::Join(filter_entries, " AND ");
	}

	lstate.base_sql = StringUtil::Format(
			R"(
			SELECT %s FROM `%s`.`%s` %s
			)",

			col_names, bind_data->schema_name, bind_data->table_name, filter_string);

	lstate.start_page = page_start_at;
	lstate.end_page = page_start_at + page_to_fetch;
	lstate.pagesize = STANDARD_VECTOR_SIZE;

	// std::cout << "lstate.start_page: " << lstate.start_page << std::endl;
	// std::cout << "lstate.end_page: " << lstate.end_page << std::endl;

	// std::cout << "BASE SQL: " << lstate.base_sql << std::endl;

	lstate.exec = false;
	lstate.done = false;
}

static bool MysqlParallelStateNext(ClientContext &context, const FunctionData *bind_data_p,
																	 MysqlLocalState &lstate, MysqlGlobalState &gstate)
{
	D_ASSERT(bind_data_p);
	auto bind_data = (const MysqlBindData *)bind_data_p;

	// std::cout << "MysqlParallelStateNext: parallel_lock" << std::endl;
	lock_guard<mutex> parallel_lock(gstate.lock);

	// std::cout << "MysqlParallelStateNext: gstate.page_to_fetch: " << bind_data->approx_number_of_pages << std::endl;
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

static void ProcessValue(
		sql::ResultSet *res,
		const LogicalType &type,
		const MysqlTypeInfo *type_info,
		Vector &out_vec,
		idx_t query_col_idx,
		idx_t output_offset)
{
	// std::cout << "Process value: query_col_idx : " << query_col_idx << std::endl;
	// std::cout << "Process value: output_offset : " << output_offset << std::endl;
	//  if(raw_col_idx == 2 || output_offset == 2){
	//  throw InternalException("col_idx = %d, output_offset = %d", raw_col_idx, output_offset);
	//  }
	//  mysql column index starts from 1
	auto col_idx = query_col_idx + 1;
	if (res->isNull(col_idx))
	{
		FlatVector::Validity(out_vec).Set(output_offset, false);
		return;
	}
	switch (type.id())
	{
	case LogicalTypeId::TINYINT:
	{
		auto tinyIntValue = res->getInt(col_idx);
		FlatVector::GetData<int8_t>(out_vec)[output_offset] = static_cast<int8_t>(tinyIntValue);
		break;
	}
	case LogicalTypeId::SMALLINT:
	{
		auto smallIntValue = res->getInt(col_idx);
		FlatVector::GetData<int16_t>(out_vec)[output_offset] = static_cast<int16_t>(smallIntValue);
		break;
	}
	case LogicalTypeId::INTEGER:
	{
		FlatVector::GetData<int32_t>(out_vec)[output_offset] = res->getInt(col_idx);
		break;
	}
	case LogicalTypeId::UINTEGER:
	{
		FlatVector::GetData<uint32_t>(out_vec)[output_offset] = res->getUInt(col_idx);
		break;
	}
	case LogicalTypeId::BIGINT:
	{
		FlatVector::GetData<int64_t>(out_vec)[output_offset] = res->getInt64(col_idx);
		break;
	}
	case LogicalTypeId::FLOAT:
	{
		auto i = res->getDouble(col_idx);
		FlatVector::GetData<float>(out_vec)[output_offset] = *((float *)&i);
		break;
	}

	case LogicalTypeId::DOUBLE:
	{
		auto i = res->getDouble(col_idx);
		FlatVector::GetData<double>(out_vec)[output_offset] = *((double *)&i);
		break;
	}

	case LogicalTypeId::BLOB:
	case LogicalTypeId::VARCHAR:
	{
		if (query_col_idx == 0)
		{
			InternalException("> AVANT VARCHAR query_col_idx == 0 !!!");
		}
		auto mysql_str = res->getString(col_idx);
		auto c_str = mysql_str.c_str();
		auto value_len = mysql_str.length();
		auto duckdb_str = StringVector::AddStringOrBlob(out_vec, c_str, value_len);
		FlatVector::GetData<string_t>(out_vec)[output_offset] = duckdb_str;
		if (query_col_idx == 1)
		{
			InternalException("> APRES VARCHAR query_col_idx == 1 !!!");
		}
		break;
	}
	case LogicalTypeId::BOOLEAN:
		FlatVector::GetData<bool>(out_vec)[output_offset] = res->getBoolean(col_idx);
		break;
	case LogicalTypeId::DECIMAL:
	{
		auto decimal_config = res->getDouble(col_idx);
		switch (type.InternalType())
		{
		case PhysicalType::INT16:
		{
			auto smallIntValue = res->getInt(col_idx);
			FlatVector::GetData<int16_t>(out_vec)[output_offset] = static_cast<int16_t>(smallIntValue);
			break;
		}
		case PhysicalType::INT32:
		{
			FlatVector::GetData<int32_t>(out_vec)[output_offset] = res->getInt(col_idx);
			break;
		}
		case PhysicalType::INT64:
		{
			FlatVector::GetData<int64_t>(out_vec)[output_offset] = res->getInt64(col_idx);
			break;
		}
		// case PhysicalType::INT128:
		// 	break;
		default:
			throw InvalidInputException("Unsupported decimal storage type");
		}
		break;
	}

	// case LogicalTypeId::DATE:
	// {

	// 	auto jd = res->getUInt64(col_idx);
	// 	auto out_ptr = FlatVector::GetData<date_t>(out_vec);
	// 	out_ptr[output_offset].days = jd + MYSQL_EPOCH_JDATE - 2440588; // magic! TODO check as copied from Postgres
	// 	break;
	// }

	// case LogicalTypeId::TIME:
	// {
	// 	D_ASSERT(value_len == sizeof(int64_t));
	// 	D_ASSERT(atttypmod == -1);

	// 	FlatVector::GetData<dtime_t>(out_vec)[output_offset].micros = LoadEndIncrement<uint64_t>(value_ptr);
	// 	break;
	// }

	// case LogicalTypeId::TIME_TZ:
	// {
	// 	D_ASSERT(value_len == sizeof(int64_t) + sizeof(int32_t));
	// 	D_ASSERT(atttypmod == -1);

	// 	auto usec = LoadEndIncrement<uint64_t>(value_ptr);
	// 	auto tzoffset = LoadEndIncrement<int32_t>(value_ptr);
	// 	FlatVector::GetData<dtime_t>(out_vec)[output_offset].micros = usec + tzoffset * Interval::MICROS_PER_SEC;
	// 	break;
	// }

	// case LogicalTypeId::TIMESTAMP_TZ:
	// case LogicalTypeId::TIMESTAMP:
	// {
	// 	D_ASSERT(value_len == sizeof(int64_t));
	// 	D_ASSERT(atttypmod == -1);

	// 	auto usec = ntohll(Load<uint64_t>(value_ptr));
	// 	auto time = usec % Interval::MICROS_PER_DAY;
	// 	// adjust date
	// 	auto date = (usec / Interval::MICROS_PER_DAY) + POSTGRES_EPOCH_JDATE - 2440588;
	// 	// glue it back together
	// 	FlatVector::GetData<timestamp_t>(out_vec)[output_offset].value = date * Interval::MICROS_PER_DAY + time;
	// 	break;
	// }
	case LogicalTypeId::ENUM:
	{
		auto enum_val = (res->getString(col_idx)).c_str();
		auto offset = EnumType::GetPos(type, enum_val);
		if (offset < 0)
		{
			throw IOException("Could not map ENUM value %s", enum_val);
		}
		switch (type.InternalType())
		{
		case PhysicalType::UINT8:
			FlatVector::GetData<uint8_t>(out_vec)[output_offset] = (uint8_t)offset;
			break;
		case PhysicalType::UINT16:
			FlatVector::GetData<uint16_t>(out_vec)[output_offset] = (uint16_t)offset;
			break;

		case PhysicalType::UINT32:
			FlatVector::GetData<uint32_t>(out_vec)[output_offset] = (uint32_t)offset;
			break;

		default:
			throw InternalException("ENUM can only have unsigned integers (except "
															"UINT64) as physical types, got %s",
															TypeIdToString(type.InternalType()));
		}
		break;
	}
		// case LogicalTypeId::INTERVAL:
		// {
		// 	if (atttypmod != -1)
		// 	{
		// 		throw IOException("Interval with unsupported typmod %d", atttypmod);
		// 	}

		// 	interval_t res;

		// 	res.micros = LoadEndIncrement<uint64_t>(value_ptr);
		// 	res.days = LoadEndIncrement<uint32_t>(value_ptr);
		// 	res.months = LoadEndIncrement<uint32_t>(value_ptr);

		// 	FlatVector::GetData<interval_t>(out_vec)[output_offset] = res;
		// 	break;
		// }

		// case LogicalTypeId::UUID:
		// {
		// 	D_ASSERT(value_len == 2 * sizeof(int64_t));
		// 	D_ASSERT(atttypmod == -1);

		// 	hugeint_t res;

		// 	auto upper = LoadEndIncrement<uint64_t>(value_ptr);
		// 	res.upper = upper ^ (int64_t(1) << 63);
		// 	res.lower = LoadEndIncrement<uint64_t>(value_ptr);

		// 	FlatVector::GetData<hugeint_t>(out_vec)[output_offset] = res;
		// 	break;
		// }

	default:
		throw InternalException("Unsupported Type %s", type.ToString());
	}
}

static void MysqlScan(ClientContext &context, TableFunctionInput &data, DataChunk &output)
{
	auto &bind_data = data.bind_data->Cast<MysqlBindData>();
	auto &local_state = data.local_state->Cast<MysqlLocalState>();
	auto &gstate = data.global_state->Cast<MysqlGlobalState>();

	while (true)
	{
		// std::cout << "while true..." << std::endl;
		if (local_state.done && !MysqlParallelStateNext(context, data.bind_data.get(), local_state, gstate))
		{
			// std::cout << "OUTPUT (size):" << output.size() << std::endl;
			// output.Print();
			// std::cout << "Annnnnd we're done" << std::endl;
			return;
		}

		idx_t output_offset = 0;
		auto stmt = local_state.conn->createStatement();

		auto sql = StringUtil::Format(
				R"(
							%s LIMIT %d OFFSET %d;
						)",
				local_state.base_sql, local_state.pagesize, local_state.pagesize * local_state.current_page);

		// std::cout << "running sql: " << sql << std::endl;
		auto res = stmt->executeQuery(sql);
		if (res->rowsCount() == 0)
		{ // done here, lets try to get more
			// std::cout << "done reading, result set empty" << std::endl;
			local_state.done = true;
			continue;
		}

		// std::cout << "reading result set" << std::endl;

		// iterate over the result set and write the result in the output data chunk
		while (res->next())
		{
			// std::cout << "reading row: " << output_offset << std::endl;
			// for each column from the bind data, read the value and write it to the result vector
			for (idx_t query_col_idx = 0; query_col_idx < output.ColumnCount(); query_col_idx++)
			{
				// std::cout << "ITERATE result set/ query_col_idx: " << query_col_idx << " output_offset: " << output_offset << std::endl;
				auto table_col_idx = local_state.column_ids[query_col_idx];
				auto &out_vec = output.data[query_col_idx];
				// std::cout << "bind_data.names[table_col_idx] " << bind_data.names[table_col_idx] << std::endl;
				// std::cout << "bind_data.types[table_col_idx] " << bind_data.types[table_col_idx].ToString() << std::endl;
				// std::cout << "bind_data.columns[table_col_idx].type_info " << bind_data.columns[table_col_idx].type_info.name << std::endl;
				//  print the size of output data
				ProcessValue(res,
										 bind_data.types[table_col_idx],
										 &bind_data.columns[table_col_idx].type_info,
										 out_vec,
										 query_col_idx,
										 output_offset);
			}

			output_offset++;

			// std::cout << "Result set done, final output_offset " << output_offset << std::endl;

			output.SetCardinality(output_offset);
		}

		// std::cout << "output vector: " << output.size() << std::endl;
		// std::cout << "closing statement" << std::endl;

		res->close();
		stmt->close();
		// std::cout << "local_state.current_page: " << local_state.current_page << std::endl;
		local_state.current_page += 1;

		if (local_state.current_page == local_state.end_page)
		{
			// std::cout << "current = end => done" << std::endl;
			local_state.done = true;
		}

		return;
	}
}

static LogicalType DuckDBType2(MysqlTypeInfo *type_info)
{
	auto &mysql_type_name = type_info->name;

	if (mysql_type_name == "enum")
	{ // ENUM
		// read the enum values from result set into a sorted vector
		const auto enum_values_as_str = type_info->enum_values; // e.g "('a','b','c')"
		const auto enum_values = StringUtil::Split(enum_values_as_str.substr(1, enum_values_as_str.length() - 2), "','");
		Vector duckdb_levels(LogicalType::VARCHAR, enum_values.size());
		for (idx_t row = 0; row < enum_values.size(); row++)
		{
			duckdb_levels.SetValue(row, enum_values[row]);
		}
		return LogicalType::ENUM("mysql_enum_" + mysql_type_name, duckdb_levels, enum_values.size());
	}

	if (mysql_type_name == "bit")
	{
		return LogicalType::BIT;
	}
	else if (mysql_type_name == "tinyint")
	{
		return LogicalType::TINYINT;
	}
	else if (mysql_type_name == "smallint")
	{
		return LogicalType::SMALLINT;
	}
	else if (mysql_type_name == "int")
	{
		return LogicalType::INTEGER;
	}
	else if (mysql_type_name == "bigint")
	{
		return LogicalType::BIGINT;
	}
	else if (mysql_type_name == "float")
	{
		return LogicalType::FLOAT;
	}
	else if (mysql_type_name == "double")
	{
		return LogicalType::DOUBLE;
	}
	else if (mysql_type_name == "decimal")
	{
		return LogicalType::DECIMAL(type_info->numeric_precision, type_info->numeric_scale);
	}
	else if (mysql_type_name == "char" || mysql_type_name == "bpchar" || mysql_type_name == "varchar" || mysql_type_name == "text" ||
					 mysql_type_name == "jsonb" || mysql_type_name == "json")
	{
		return LogicalType::VARCHAR;
	}
	else if (mysql_type_name == "date")
	{
		return LogicalType::DATE;
	}
	else if (mysql_type_name == "bytea")
	{
		return LogicalType::BLOB;
	}
	else if (mysql_type_name == "time")
	{
		return LogicalType::TIME;
	}
	else if (mysql_type_name == "timetz")
	{
		return LogicalType::TIME_TZ;
	}
	else if (mysql_type_name == "timestamp")
	{
		return LogicalType::TIMESTAMP;
	}
	else if (mysql_type_name == "timestamptz")
	{
		return LogicalType::TIMESTAMP_TZ;
	}
	else if (mysql_type_name == "interval")
	{
		return LogicalType::INTERVAL;
	}
	else if (mysql_type_name == "uuid")
	{
		return LogicalType::UUID;
	}
	else
	{
		return LogicalType::INVALID;
	}
}

static LogicalType DuckDBType(MysqlColumnInfo &info)
{
	return DuckDBType2(&info.type_info);
}

static unique_ptr<FunctionData> MysqlBind(ClientContext &context, TableFunctionBindInput &input,
																					vector<LogicalType> &return_types, vector<string> &names)
{

	auto bind_data = make_uniq<MysqlBindData>();

	bind_data->host = input.inputs[0].GetValue<string>();
	bind_data->username = input.inputs[1].GetValue<string>();
	bind_data->password = input.inputs[2].GetValue<string>();

	bind_data->schema_name = input.inputs[3].GetValue<string>();
	bind_data->table_name = input.inputs[4].GetValue<string>();

	auto driver = sql::mysql::get_mysql_driver_instance();

	// std::cout << "Connecting to MySQL server " << bind_data->host << std::endl;
	//  Establish a MySQL connection
	bind_data->conn = driver->connect(bind_data->host, bind_data->username, bind_data->password);
	auto stmt1 = bind_data->conn->createStatement();

	auto res1 = stmt1->executeQuery(
			StringUtil::Format(
					R"(SELECT CEIL(COUNT(*) / %d) FROM %s.%s)",
					STANDARD_VECTOR_SIZE, bind_data->schema_name, bind_data->table_name));
	if (res1->rowsCount() != 1)
	{
		throw InvalidInputException("Mysql table \"%s\".\"%s\" not found", bind_data->schema_name,
																bind_data->table_name);
	}
	if (res1->next())
	{
		bind_data->approx_number_of_pages = res1->getInt64(1);
	}
	else
	{
		// Handle the case where no rows were returned
		// or unable to move to the first row
		throw InvalidInputException("Failed to fetch data from result set");
	}

	res1->close();
	stmt1->close();

	auto stmt2 = bind_data->conn->createStatement();
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
			bind_data->schema_name, bind_data->table_name));

	// can't scan a table without columns (yes those exist)
	if (res2->rowsCount() == 0)
	{
		throw InvalidInputException("Table %s does not contain any columns.", bind_data->table_name);
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

		bind_data->names.push_back(info.column_name);

		auto duckdb_type = DuckDBType(info);
		// we cast unsupported types to varchar on read
		auto needs_cast = duckdb_type == LogicalType::INVALID;
		bind_data->needs_cast.push_back(needs_cast);
		if (!needs_cast)
		{
			bind_data->types.push_back(std::move(duckdb_type));
		}
		else
		{
			bind_data->types.push_back(LogicalType::VARCHAR);
		}

		bind_data->columns.push_back(info);
	}
	res2->close();
	stmt2->close();

	return_types = bind_data->types;
	names = bind_data->names;

	return std::move(bind_data);
}

static unique_ptr<GlobalTableFunctionState> MysqlInitGlobalState(ClientContext &context,
																																 TableFunctionInitInput &input)
{
	return make_uniq<MysqlGlobalState>(
			MysqlMaxThreads(context, input.bind_data.get()));
}

static sql::Connection *MysqlScanConnect(string host, string username, string password)
{
	sql::mysql::MySQL_Driver *driver;
	sql::Connection *conn;
	driver = sql::mysql::get_mysql_driver_instance();
	// Establish a MySQL connection
	conn = driver->connect(host, username, password);
	return conn;
}

static unique_ptr<LocalTableFunctionState> MysqlInitLocalState(ExecutionContext &context,
																															 TableFunctionInitInput &input,
																															 GlobalTableFunctionState *global_state)
{
	auto &bind_data = input.bind_data->Cast<MysqlBindData>();
	auto &gstate = global_state->Cast<MysqlGlobalState>();

	auto local_state = make_uniq<MysqlLocalState>();
	local_state->column_ids = input.column_ids;
	local_state->conn = MysqlScanConnect(bind_data.host, bind_data.username, bind_data.password);
	local_state->filters = input.filters.get();

	if (!MysqlParallelStateNext(context.client, input.bind_data.get(), *local_state, gstate))
	{
		// throw InternalException("Failed to initialize MysqlLocalState");
		local_state->done = true;
	}
	return std::move(local_state);
}
struct AttachFunctionData : public TableFunctionData
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

	string host;
	string username;
	string password;
};

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
	if (data.finished)
	{
		return;
	}

	sql::mysql::MySQL_Driver *driver;

	driver = sql::mysql::get_mysql_driver_instance();

	// Establish a MySQL connection
	auto conn = driver->connect(data.host, data.username, data.password);

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
	conn->close();

	data.finished = true;
}

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

extern "C"
{
	DUCKDB_EXTENSION_API void mysql_scanner_init(duckdb::DatabaseInstance &db)
	{
		Connection con(db);
		con.BeginTransaction();
		auto &context = *con.context;
		auto &catalog = Catalog::GetSystemCatalog(context);

		MysqlScanFunction mysql_fun;
		CreateTableFunctionInfo mysql_info(mysql_fun);
		catalog.CreateTableFunction(context, mysql_info);

		MysqlScanFunctionFilterPushdown mysql_fun_filter_pushdown;
		CreateTableFunctionInfo mysql_filter_pushdown_info(mysql_fun_filter_pushdown);
		catalog.CreateTableFunction(context, mysql_filter_pushdown_info);

		TableFunction attach_func("mysql_attach", {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR}, AttachFunction, AttachBind);
		attach_func.named_parameters["overwrite"] = LogicalType::BOOLEAN;
		attach_func.named_parameters["filter_pushdown"] = LogicalType::BOOLEAN;

		attach_func.named_parameters["source_schema"] = LogicalType::VARCHAR;
		attach_func.named_parameters["sink_schema"] = LogicalType::VARCHAR;

		CreateTableFunctionInfo attach_info(attach_func);
		catalog.CreateTableFunction(context, attach_info);

		con.Commit();
	}

	DUCKDB_EXTENSION_API const char *mysql_scanner_version()
	{
		return DuckDB::LibraryVersion();
	}
}