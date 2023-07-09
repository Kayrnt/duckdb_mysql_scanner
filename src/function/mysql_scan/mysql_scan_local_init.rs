use duckdb_extension_framework::duckly::duckdb_init_info;
use duckdb_extension_framework::table_functions::InitInfo;
use crate::function::mysql_scan::mysql_scan_bind::MysqlScanBindData;

#[repr(C)]
struct ScanInitData {
    //stream: *mut ResultStream,
    pagination_index: u32,
    done: bool,
}

impl ScanInitData {
    /*fn new(stream: Box<ResultStream>) -> Self {
        Self {
            stream: Box::into_raw(stream),
            done: false,
            pagination_index: 0,
        }
    }*/
}

/// # Safety
///
/// .
#[no_mangle]
pub unsafe extern "C" fn read_mysql_local_init(info: duckdb_init_info) {
    let info = InitInfo::from(info);
    let bind_info = info.get_bind_data::<MysqlScanBindData>();
}