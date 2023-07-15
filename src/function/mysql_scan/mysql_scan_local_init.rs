use duckdb_extension_framework::duckly::duckdb_init_info;
use duckdb_extension_framework::table_functions::InitInfo;
use mysql::{QueryResult, ResultSet};
use crate::function::mysql_scan::mysql_scan_bind::MysqlScanBindData;

#[repr(C)]
pub struct MysqlScanLocalInitData {
    //stream: *mut ResultStream,
    pub result_set: *mut QueryResult,

    pub start_page: u32,
    pub current_pagination_index: u32,
    pub end_page: u32,

    pub batch_done: bool,
    pub query_executed: bool,


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

pub fn prepare_local_state() -> () {
    /*let base_sql = DuckDBToMySqlRequest(&bind_info, lstate);

    lstate.start_page = page_start_at;
    lstate.end_page = page_start_at + page_to_fetch;
    lstate.pagesize = STANDARD_VECTOR_SIZE;

    lstate.exec = false;
    lstate.done = false;

    lstate.stmt = lstate.conn->createStatement();

    auto sql = StringUtil::Format(
        R"(
					%s LIMIT %d OFFSET %d;
				)",
        lstate.base_sql, lstate.pagesize * lstate.end_page, lstate.start_page * lstate.current_page);

    lstate.result_set = lstate.stmt->executeQuery(sql);
    if (lstate.result_set->rowsCount() == 0)
    { // done here, lets try to get more
        lstate.done = true;
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