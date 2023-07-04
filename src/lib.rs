#![allow(dead_code)]
use duckdb_ext::Database;
use std::ffi::c_char;
use tokio::runtime::Runtime;

pub mod error;
mod table_function;
mod types;

use crate::table_function::build_table_function_def;
use duckdb_ext::ffi::{_duckdb_database, duckdb_library_version};
use error::Result;

lazy_static::lazy_static! {
    static ref RUNTIME: Runtime = tokio::runtime::Runtime::new()
            .expect("Creating Tokio runtime");
}

/// Init hook for DuckDB, registers all functionality provided by this extension
/// # Safety
/// .
#[no_mangle]
pub unsafe extern "C" fn mysql_scanner_init_rust(db: *mut _duckdb_database) {
    init(db).expect("init failed");
}

unsafe fn init(db: *mut _duckdb_database) -> Result<()> {
    let db = Database::from(db);
    let table_function = build_table_function_def();
    let connection = db.connect()?;
    connection.register_table_function(table_function)?;
    Ok(())
}

/// Version hook for DuckDB, indicates which version of DuckDB this extension was compiled against
#[no_mangle]
pub extern "C" fn mysql_scanner_version_rust() -> *const c_char {
    unsafe { duckdb_library_version() }
}
