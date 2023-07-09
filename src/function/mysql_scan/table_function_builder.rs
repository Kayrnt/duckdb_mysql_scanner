use duckdb_extension_framework::{LogicalType, LogicalTypeId};
use duckdb_extension_framework::table_functions::TableFunction;
use crate::function::mysql_scan::mysql_scan_bind::read_mysql_bind;
use crate::function::mysql_scan::mysql_scan_global_init::read_mysql_init;
use crate::function::mysql_scan::mysql_scan_local_init::read_mysql_local_init;
use crate::function::mysql_scan::mysql_scan_read::read_mysql;

pub fn build_table_function_def() -> TableFunction {
    let table_function = TableFunction::new();
    table_function.set_name("mysql_scan");
    let varchar_type = LogicalType::new(LogicalTypeId::Varchar);
    //let int_type = LogicalType::new(LogicalTypeId::Integer);
    table_function.add_parameter(&varchar_type); // url
    table_function.add_parameter(&varchar_type); // schema
    table_function.add_parameter(&varchar_type); // table
    //table_function.add_named_parameter("maxrows", &int_type);

    table_function.set_function(Some(read_mysql));
    table_function.set_init(Some(read_mysql_init));
    table_function.set_local_init(Some(read_mysql_local_init));
    table_function.set_bind(Some(read_mysql_bind));
    table_function
}
