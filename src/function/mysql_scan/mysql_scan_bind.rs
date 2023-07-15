use std::cell::RefCell;
use std::ffi::{c_char, c_void, CString};
use std::sync::{Arc, Mutex};
use duckdb_extension_framework::duckly::{duckdb_bind_info, duckdb_free, duckdb_type};
use duckdb_extension_framework::{LogicalType, LogicalTypeId, malloc_struct};
use duckdb_extension_framework::table_functions::BindInfo;
use futures::StreamExt;
use mysql::{Pool, params, prelude::Queryable};
use mysql::prelude::FromRow;
use crate::model::duckdb_type::duckdb_type;
use crate::model::extension_global_state::get_connection_pool_for_url;

#[repr(C)]
pub struct MysqlScanBindData {
    pub url: *mut c_char,
    pub schema: *mut c_char,
    pub table: *mut c_char,
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

    // Create the bind data
    let my_bind_data = malloc_struct::<MysqlScanBindData>();
    (*my_bind_data).url = CString::new(url).expect("MySQL url").into_raw();
    (*my_bind_data).schema = CString::new(schema).expect("Schema name").into_raw();
    (*my_bind_data).table = CString::new(table).expect("Table name").into_raw();

    // Set the bind data
    bind_info.set_bind_data(my_bind_data.cast(), Some(drop_scan_bind_data_c));
}

