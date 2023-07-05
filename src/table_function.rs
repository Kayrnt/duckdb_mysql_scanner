use anyhow::{anyhow, Result};
use futures::{executor::block_on, Stream};
use std::os::fd::IntoRawFd;
use std::ptr::null;
use std::{
    ffi::{c_void, CStr, CString},
    os::raw::c_char,
    pin::Pin,
    task::{Context, Poll},
    thread,
};

use duckdb_ext::table_function::{BindInfo, InitInfo, TableFunction};
use duckdb_ext::{
    ffi::{
        duckdb_bind_info, duckdb_data_chunk, duckdb_free, duckdb_function_info, duckdb_init_info,
    },
};
use duckdb_ext::{DataChunk, FunctionInfo, LogicalType, LogicalTypeId};

use tokio::{runtime::Runtime, time::Duration};

use crate::types::{map_type, populate_column};

#[repr(C)]
struct ScanBindData {
    tablename: *mut c_char,
    output_location: *mut c_char,
    limit: i32,
}

const DEFAULT_LIMIT: i32 = 10000;

impl ScanBindData {
    fn new(tablename: &str, output_location: &str) -> Self {
        Self {
            tablename: CString::new(tablename).expect("Table name").into_raw(),
            output_location: CString::new(output_location)
                .expect("S3 output location")
                .into_raw(),
            limit: DEFAULT_LIMIT as i32,
        }
    }
}

/// Drop the ScanBindData from C.
///
/// # Safety
unsafe extern "C" fn drop_scan_bind_data_c(v: *mut c_void) {
    let actual = v.cast::<ScanBindData>();
    drop(CString::from_raw((*actual).tablename.cast()));
    drop(CString::from_raw((*actual).output_location.cast()));
    drop((*actual).limit);
    duckdb_free(v);
}

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
unsafe extern "C" fn read_mysql(info: duckdb_function_info, output: duckdb_data_chunk) {
    let info = FunctionInfo::from(info);
    let mut output = DataChunk::from(output);

    let init_data = info.init_data::<ScanInitData>();

}

/// # Safety
///
/// .
#[no_mangle]
unsafe extern "C" fn read_mysql_bind(bind_info: duckdb_bind_info) {
    let bind_info = BindInfo::from(bind_info);
    assert!(bind_info.num_parameters()>= 2);

    let tablename = bind_info.parameter(0);
    let output_location = bind_info.parameter(1);
    let maxrows = bind_info.named_parameter("maxrows");
}

/// # Safety
///
/// .
#[no_mangle]
unsafe extern "C" fn read_mysql_init(info: duckdb_init_info) {
    let info = InitInfo::from(info);
    let bind_info = info.bind_data::<ScanBindData>();
    // assert_eq!(bind_info.num_parameters(), 2);

    // Extract the table name and output location from
    let tablename = CStr::from_ptr((*bind_info).tablename).to_str().unwrap();
    let output_location = CStr::from_ptr((*bind_info).output_location)
        .to_str()
        .unwrap();
    let maxrows = (*bind_info).limit;

}

/// # Safety
///
/// .
#[no_mangle]
unsafe extern "C" fn read_mysql_local_init(info: duckdb_init_info) {
    let info = InitInfo::from(info);
    let bind_info = info.bind_data::<ScanBindData>();

}

pub fn build_table_function_def() -> TableFunction {
    let table_function = TableFunction::new("mysql_scan");
    let varchar_type = LogicalType::new(LogicalTypeId::Varchar);
    let int_type = LogicalType::new(LogicalTypeId::Integer);
    table_function.add_parameter(&varchar_type); // url
    table_function.add_parameter(&varchar_type); // schema
    table_function.add_parameter(&varchar_type); // table
    table_function.add_named_parameter("maxrows", &int_type);

    table_function.set_function(Some(read_mysql));
    table_function.set_init(Some(read_mysql_init));
    table_function.set_local_init(Some(read_mysql_init));
    table_function.set_bind(Some(read_mysql_bind));
    table_function
}

lazy_static::lazy_static! {
    static ref RUNTIME: Runtime = tokio::runtime::Builder::new_current_thread()
            .build()
            .expect("runtime");
}
