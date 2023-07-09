use duckdb_extension_framework::DataChunk;
use duckdb_extension_framework::duckly::{duckdb_data_chunk, duckdb_function_info};
use duckdb_extension_framework::table_functions::FunctionInfo;
use crate::model::mysql_global_state::MysqlGlobalState;

/// # Safety
///
/// .
#[no_mangle]
pub unsafe extern "C" fn read_mysql(info: duckdb_function_info, output: duckdb_data_chunk) {
    let info = FunctionInfo::from(info);
    let mut output = DataChunk::from(output);

    let init_data = info.get_init_data::<MysqlGlobalState>();

}