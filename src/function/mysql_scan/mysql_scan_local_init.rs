use std::ffi::c_void;
use duckdb_extension_framework::duckly::{duckdb_free, duckdb_init_info};
use duckdb_extension_framework::malloc_struct;
use duckdb_extension_framework::table_functions::InitInfo;
use sqlx::Executor;
use sqlx::mysql::MySqlRow;
use crate::function::mysql_scan::mysql_scan_bind::MysqlScanBindData;
use crate::function::mysql_scan::mysql_scan_global_init::MysqlScanGlobalInitData;
use crate::function::mysql_scan::mysql_scan_read::STANDARD_VECTOR_SIZE;
use crate::model::extension_global_state::get_connection_pool_for_url;

#[repr(C)]
pub struct MysqlScanLocalInitData {
    pub results: *mut c_void,

    pub start_page: usize,
    pub current_page: usize,
    pub end_page: usize,

    pub batch_done: bool,
    pub local_state_initialized: bool,
}

impl MysqlScanLocalInitData {
    /*fn new(stream: Box<ResultStream>) -> Self {
        Self {
            stream: Box::into_raw(stream),
            done: false,
            pagination_index: 0,
        }
    }*/
}

/// Drop the MysqlScanLocalInitData from C.
///
/// # Safety
unsafe extern "C" fn drop_scan_local_init_data(v: *mut c_void) {
    let actual = v.cast::<MysqlScanLocalInitData>();
    let boxed_data = unsafe { Box::from_raw((*actual).results as *mut Vec<MySqlRow>) };
    duckdb_free(v);
}

/// # Safety
///
/// .
#[no_mangle]
pub unsafe extern "C" fn read_mysql_local_init(info: duckdb_init_info) {
    let info = InitInfo::from(info);
    let bind_data_p = info.get_bind_data::<MysqlScanBindData>();


    //malloc MysqlScanLocalInitData for duckDB
    let mut my_local_init_data = malloc_struct::<MysqlScanLocalInitData>();
    //TODO update the value on first scan read based on the global state to update those values
    (*my_local_init_data).start_page = 0;
    (*my_local_init_data).current_page = 0;
    (*my_local_init_data).end_page = 1000;
    //my_local_init_data.pagesize = STANDARD_VECTOR_SIZE;

    (*my_local_init_data).batch_done = false;
    (*my_local_init_data).local_state_initialized = false;

    info.set_init_data(my_local_init_data.cast(), Some(drop_scan_local_init_data));
}

pub async fn initialize_local_init_data(
    bind_data_p: &MysqlScanBindData,
    local_init_data_p: &mut MysqlScanLocalInitData,
    init_data_p: &MysqlScanGlobalInitData) {
    if !local_init_data_p.local_state_initialized {
        let sql = format!(
            "{} LIMIT {} OFFSET {};",
            init_data_p.get_base_sql(),
            STANDARD_VECTOR_SIZE * (*local_init_data_p).end_page,
            STANDARD_VECTOR_SIZE * (*local_init_data_p).start_page
        );

        let url = (*bind_data_p).get_url();
        let pool = get_connection_pool_for_url(url).await;

        let res: Vec<MySqlRow> = pool.lock().unwrap().borrow().fetch_all(sql.as_str()).await.unwrap();

        let boxed_data = Box::new(res);
        (*local_init_data_p).results = Box::into_raw(boxed_data) as *mut c_void;

        (*local_init_data_p).local_state_initialized = true;
    }
}