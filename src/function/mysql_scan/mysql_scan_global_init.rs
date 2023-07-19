use duckdb_extension_framework::duckly::{duckdb_free, duckdb_init_info, idx_t};
use duckdb_extension_framework::table_functions::InitInfo;
use std::ffi::{c_char, c_void, CString};
use duckdb_extension_framework::malloc_struct;
use futures::executor::block_on;
use crate::function::mysql_scan::mysql_scan_bind::MysqlScanBindData;
use crate::model::extension_global_state::get_mysql_table_type_infos_for_schema_table;


#[repr(C)]
pub struct MysqlScanGlobalInitData {
    pub done: bool,
    pub current_thread_start_page: u32,
    pub max_threads: u32,
    pub base_sql: *mut c_char,
}

impl MysqlScanGlobalInitData {
    pub fn get_base_sql(&self) -> String {
        unsafe { CString::from_raw(self.base_sql).to_string_lossy().into_owned() }
    }
}

/// Drop the MysqlScanGlobalInitData from C.
///
/// # Safety
unsafe extern "C" fn drop_scan_global_init_data(v: *mut c_void) {
    // let actual = v.cast::<MysqlScanGlobalInitData>();
    /*println!("Dropping ScanBindData: {:?}", actual);
    drop(CString::from_raw((*actual).schema.cast()));
    drop(CString::from_raw((*actual).table.cast()));
    drop(CString::from_raw((*actual).url.cast()));
    println!("Dropped ScanBindData: {:?}", actual);*/
    duckdb_free(v);
}

pub unsafe fn duckdb_to_mysql_request(
    schema: &str,
    table: &str,
    column_ids: &Vec<idx_t>) -> String {
    let type_infos_arc = get_mysql_table_type_infos_for_schema_table(
        &schema,
        &table,
    ).unwrap();

    let binding = type_infos_arc.lock().unwrap();
    let type_infos = binding.borrow();


    let mut col_names = String::new();
    col_names = column_ids.iter().map(|&column_id| {
        let column_id = column_id as usize;
        let name = type_infos.names[column_id].to_owned();
        let needs_cast: bool = type_infos.needs_cast[column_id];
        format!("`{}`{}", name, if needs_cast { "::VARCHAR" } else { "" })
    }).collect::<Vec<_>>().join(", ");

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
        col_names, schema, table, filter_string
    )
}

/// # Safety
///
/// .
#[no_mangle]
pub unsafe extern "C" fn read_mysql_init(info: duckdb_init_info) {
    let global_init_data = InitInfo::from(info);
    let bind_data = global_init_data.get_bind_data::<MysqlScanBindData>();
    let column_ids = global_init_data.get_column_indices();

    //let url_binding = CString::from_raw((*bind_data).url);
    let url = (*bind_data).get_url();

    let schema_binding = CString::from_raw((*bind_data).schema);
    let schema = schema_binding.to_str().unwrap();

    let table_binding = CString::from_raw((*bind_data).table);
    let table = table_binding.to_str().unwrap();

    // Allocate memory for the base_sql
    let sql_str = duckdb_to_mysql_request(
        schema, table, &column_ids,
    );
    //println!("SQL: {}", sql_str);
    let base_sql = CString::new(sql_str).expect("CString::new failed").into_raw();

    let mut my_global_init_data = malloc_struct::<MysqlScanGlobalInitData>();
    (*my_global_init_data).done = false;
    (*my_global_init_data).current_thread_start_page = 0;
    (*my_global_init_data).max_threads = 1;
    (*my_global_init_data).base_sql = base_sql;
    //(*my_global_init_data).pushdown_column_ids = column_ids;

    //create the connection pool to mysql using URL
    block_on(async {
        // let pool = get_connection_pool_for_url(url).await;

        // let query = format!("SELECT COUNT(*) FROM {}.{}", schema, table);
        // println!("Query: {}", query);
        // let count_rows: i64 = sqlx::query(query.as_str())
        //     .map(|row: MySqlRow| {
        //         let count: Option<i64> = row.try_get(0).unwrap();
        //         count.expect("Counted but no count returned")
        //         // map the row into a user-defined domain type
        //     })
        //     .fetch_one(&*(pool.lock().unwrap().borrow())).await.unwrap();

        // print the result
        // println!("Count: {}", count_rows);

        global_init_data.set_init_data(my_global_init_data.cast(), Some(drop_scan_global_init_data));
    });
}

