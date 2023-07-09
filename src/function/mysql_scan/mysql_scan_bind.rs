use std::ffi::{c_char, c_void, CString};
use duckdb_extension_framework::duckly::{duckdb_bind_info, duckdb_free};
use duckdb_extension_framework::malloc_struct;
use duckdb_extension_framework::table_functions::BindInfo;
use mysql::Pool;
use mysql::prelude::Queryable;
use crate::model::extension_global_state::get_connection_pool_for_url;

#[repr(C)]
pub struct MysqlScanBindData {
    schema: *mut c_char,
    table: *mut c_char,
}

const DEFAULT_LIMIT: i32 = 10000;

/// Drop the ScanBindData from C.
///
/// # Safety
unsafe extern "C" fn drop_scan_bind_data_c(v: *mut c_void) {
    let actual = v.cast::<MysqlScanBindData>();
    drop(CString::from_raw((*actual).schema.cast()));
    drop(CString::from_raw((*actual).table.cast()));
    duckdb_free(v);
}

/// # Safety
///
/// .
#[no_mangle]
pub unsafe extern "C" fn read_mysql_bind(bind_info: duckdb_bind_info) {
    let bind_info = BindInfo::from(bind_info);
    assert!(bind_info.get_parameter_count() >= 3);

    let url_var = bind_info.get_parameter(0).get_varchar();
    let url = url_var.to_str().unwrap().to_string();

    let schema_var = bind_info.get_parameter(1).get_varchar();
    let schema = schema_var.to_str().unwrap();

    let table_var = bind_info.get_parameter(2).get_varchar();
    let table = table_var.to_str().unwrap();
    //let maxrows = bind_info.named_parameter("maxrows");

    //create the connection pool to mysql using URL
    let pool = get_connection_pool_for_url(&url);

    let mut connection = pool.lock().unwrap().borrow().get_conn().unwrap();

    let query = format!("SELECT COUNT(*) FROM {}.{}", schema, table);
    let count_rows: Option<u64> = connection.query_first(query).unwrap();

    // print the result
    println!("Count: {}", count_rows.unwrap());

    // Create the bind data
    let my_bind_data = malloc_struct::<MysqlScanBindData>();
    (*my_bind_data).schema = CString::new(schema).expect("Schema name").into_raw();
    (*my_bind_data).table = CString::new(table).expect("Table name").into_raw();

    // Set the bind data
    bind_info.set_bind_data(my_bind_data.cast(), Some(drop_scan_bind_data_c));
}