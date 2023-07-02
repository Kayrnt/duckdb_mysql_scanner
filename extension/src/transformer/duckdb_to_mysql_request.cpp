#include "duckdb.hpp"

#include "../model/mysql_bind_data.hpp"
#include "../state/mysql_local_state.hpp"

#include "duckdb/planner/filter/conjunction_filter.hpp"
#include "duckdb/planner/filter/constant_filter.hpp"

using namespace duckdb;

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

static string DuckDBToMySqlRequest(const MysqlBindData *bind_data_p, MysqlLocalState &lstate)
{
	D_ASSERT(bind_data_p);
	auto bind_data = (const MysqlBindData *)bind_data_p;

	std::string col_names;
	
  col_names = StringUtil::Join(
			lstate.column_ids.data(),
			lstate.column_ids.size(),
			", ",
			[&](const idx_t column_id)
			{ return StringUtil::Format("`%s`%s",
																	bind_data->names[column_id],
																	bind_data->needs_cast[column_id] ? "::VARCHAR" : ""); });

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

	return StringUtil::Format(
			R"(
			SELECT %s FROM `%s`.`%s` %s
			)",

			col_names, bind_data->schema_name, bind_data->table_name, filter_string);

}