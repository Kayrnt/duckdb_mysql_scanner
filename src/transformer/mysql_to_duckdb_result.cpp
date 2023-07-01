#include "duckdb.hpp"

#include "../model/mysql_bind_data.hpp"
#include "../state/mysql_local_state.hpp"

using namespace duckdb;

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
		auto mysql_str = res->getString(col_idx);
		auto enum_val = mysql_str.c_str();
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