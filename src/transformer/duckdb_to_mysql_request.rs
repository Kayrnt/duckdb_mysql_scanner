use duckdb_extension_framework::table_functions::BindInfo;
use crate::function::mysql_scan::mysql_scan_bind::MysqlScanBindData;
use crate::model::mysql_local_state::MysqlLocalState;
// use duckdb_ext::ExpressionType;

/*fn transform_filter(column_name: &str, filter: &TableFilter) -> String {
    // Implementation of TransformFilter goes here
    unimplemented!()
}

struct TransformFilter {
    column_name: String,
    filter: Box<TableFilter>,
}*/

/*fn create_expression(column_name: &str, filters: &Vec<Box<TableFilter>>, op: &str) -> String {
    let mut filter_entries = Vec::new();
    for filter in filters {
        filter_entries.push(transform_filter(column_name, &filter));
    }
    format!("({})", filter_entries.join(&format!(" {} ", op)))
}

fn transform_comparison(expr_type: ExpressionType) -> String {
    match expr_type {
        ExpressionType::COMPARE_EQUAL => "=".to_string(),
        ExpressionType::COMPARE_NOTEQUAL => "!=".to_string(),
        ExpressionType::COMPARE_LESSTHAN => "<".to_string(),
        ExpressionType::COMPARE_GREATERTHAN => ">".to_string(),
        ExpressionType::COMPARE_LESSTHANOREQUALTO => "<=".to_string(),
        ExpressionType::COMPARE_GREATERTHANOREQUALTO => ">=".to_string(),
        _ => panic!("Unsupported expression type"),
    }
}

fn transform_filter(column_name: &str, filter: &TableFilter) -> String {
    match filter.filter_type {
        TableFilterType::IS_NULL => format!("{} IS NULL", column_name),
        TableFilterType::IS_NOT_NULL => format!("{} IS NOT NULL", column_name),
        TableFilterType::CONJUNCTION_AND => {
            let conjunction_filter = filter.as_any().downcast_ref::<ConjunctionAndFilter>().unwrap();
            create_expression(column_name, &conjunction_filter.child_filters, "AND")
        }
        TableFilterType::CONJUNCTION_OR => {
            let conjunction_filter = filter.as_any().downcast_ref::<ConjunctionOrFilter>().unwrap();
            create_expression(column_name, &conjunction_filter.child_filters, "OR")
        }
        TableFilterType::CONSTANT_COMPARISON => {
            let constant_filter = filter.as_any().downcast_ref::<ConstantFilter>().unwrap();
            let constant_string = format!("'{}'", constant_filter.constant.to_string());
            let operator_string = transform_comparison(constant_filter.comparison_type);
            format!("{} {} {}", column_name, operator_string, constant_string)
        }
        _ => panic!("Unsupported table filter type"),
    }
}*/

/*fn duckdb_to_mysql_request(bind_data_p: *const MysqlScanBindData, lstate: &mut MysqlLocalState) -> String {
    assert!(!bind_data_p.is_null());
    let bind_data: &MysqlScanBindData = unsafe { &*bind_data_p };

    let col_names = String::new();
    /*let col_names = lstate.column_ids.iter().map(|&column_id| {
        let name = bind_data.names[column_id].to_owned();
        let needs_cast = bind_data.needs_cast[column_id];
        format!("`{}`{}", name, if needs_cast { "::VARCHAR" } else { "" })
    }).collect::<Vec<_>>().join(", ");*/

    let mut filter_string = String::new();
    /*if let Some(filters) = &lstate.filters {
        if !filters.filters.is_empty() {
            let filter_entries = filters.filters.iter().map(|(column_id, filter)| {
                let column_name = format!("`{}`", bind_data.names[lstate.column_ids[*column_id]]);
                transform_filter(&column_name, filter)
            }).collect::<Vec<_>>();
            filter_string = format!(" WHERE {}", filter_entries.join(" AND "));
        }
    }*/

    format!(
        "SELECT {} FROM `{}`.`{}` {}",
        col_names, bind_data.schema_name, bind_data.table_name, filter_string
    )
}*/

