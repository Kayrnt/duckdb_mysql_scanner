#![allow(dead_code)]
use std::ffi::{c_char, c_void};

pub mod error;
mod types;
mod model;
mod transformer;
mod function;

use crate::function::mysql_scan::table_function_builder::build_table_function_def;
use duckdb_extension_framework::Database;
use duckdb_extension_framework::duckly::duckdb_library_version;
use error::Result;

/// Init hook for DuckDB, registers all functionality provided by this extension
/// # Safety
/// .
#[no_mangle]
pub unsafe extern "C" fn mysql_scanner_init_rust(db: *mut c_void) {
    init(db).expect("init failed");
}

unsafe fn init(db: *mut c_void) -> Result<()> {
    let db = Database::from_cpp_duckdb(db);
    let mysql_scan_function = build_table_function_def();
    let connection = db.connect()?;
    connection.register_table_function(mysql_scan_function)?;
    Ok(())
}

/// Version hook for DuckDB, indicates which version of DuckDB this extension was compiled against
#[no_mangle]
pub extern "C" fn mysql_scanner_version_rust() -> *const c_char {
    unsafe { duckdb_library_version() }
}
