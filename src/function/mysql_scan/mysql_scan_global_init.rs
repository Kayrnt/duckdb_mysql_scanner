use duckdb_extension_framework::duckly::{duckdb_free, duckdb_init_info};
use duckdb_extension_framework::malloc_struct;
use duckdb_extension_framework::table_functions::InitInfo;
use crate::function::mysql_scan::mysql_scan_bind::MysqlScanBindData;
use crate::model::mysql_global_state::MysqlGlobalState;

/// # Safety
///
/// .
#[no_mangle]
pub unsafe extern "C" fn read_mysql_init(info: duckdb_init_info) {
    let info = InitInfo::from(info);
    let bind_info = info.get_bind_data::<MysqlScanBindData>();

    let mut my_init_data = malloc_struct::<MysqlGlobalState>();
    (*my_init_data).done = false;
    (*my_init_data).start_page = 0;
    (*my_init_data).max_threads = 1;
    info.set_init_data(my_init_data.cast(), Some(duckdb_free));

}
