use std::any::Any;
use duckdb_extension_framework::DataChunk;
use duckdb_extension_framework::duckly::{duckdb_data_chunk, duckdb_function_info};
use duckdb_extension_framework::table_functions::FunctionInfo;
use sqlx::mysql::{MySqlColumn, MySqlRow, MySqlTypeInfo};
use sqlx::{Column, Row, TypeInfo};
use std::ffi::c_void;
use std::ptr;
use futures::executor::block_on;
use crate::function::mysql_scan::mysql_scan_bind::MysqlScanBindData;
use crate::function::mysql_scan::mysql_scan_global_init::MysqlScanGlobalInitData;
use crate::function::mysql_scan::mysql_scan_local_init::{initialize_local_init_data, MysqlScanLocalInitData};
use crate::model::duckdb_type::populate_column;

pub static STANDARD_VECTOR_SIZE: usize = 2048;

/// # Safety
///
/// .
#[no_mangle]
pub unsafe extern "C" fn read_mysql(info: duckdb_function_info, output: duckdb_data_chunk) {
    let info = FunctionInfo::from(info);
    let mut output = DataChunk::from(output);

    let bind_data_p = info.get_bind_data::<MysqlScanBindData>();
    let init_data_p = info.get_init_data::<MysqlScanGlobalInitData>();
    let local_init_data_p: *mut MysqlScanLocalInitData = info.get_local_init_data::<MysqlScanLocalInitData>();

    block_on(async {
        initialize_local_init_data(
            &bind_data_p.as_ref().unwrap(),
            &mut local_init_data_p.as_mut().unwrap(),
            init_data_p.as_ref().unwrap(),
        ).await;
    });

    let results = Box::from_raw((*local_init_data_p).results as *mut Vec<MySqlRow>);

    loop {
        /*if (*local_init_data_p).batch_done && !mysql_parallel_state_next(
            &bind_data_p.as_ref().unwrap(),
            &local_init_data_p.as_mut().unwrap(),
            init_data_p.as_ref().unwrap()) {
            return;
        }*/

        let mut output_offset: usize = 0;
        //println!("(*local_init_data_p).results LEN {}", results.len());
        //println!("(*local_init_data_p).current_idx_in_results {}", (*local_init_data_p).current_idx_in_results);

        while (*local_init_data_p).current_idx_in_results < results.len() &&
            output_offset < STANDARD_VECTOR_SIZE {

            let res = &results[(*local_init_data_p).current_idx_in_results];

            for col in res.columns() {
                let type_info: &MySqlTypeInfo = col.type_info();
                populate_column(res, type_info.name(),
                                &mut output, output_offset, col.ordinal());
            }
            (*local_init_data_p).current_idx_in_results += 1;
            output_offset += 1;
            output.set_len(output_offset);
        }

        //println!("read output_offset {}", output_offset);

        (*local_init_data_p).current_page += 1;

        if (*local_init_data_p).current_page == (*local_init_data_p).end_page {
            //println!("thread done");
            (*init_data_p).done = true; //TODO remove and should check done global
            (*local_init_data_p).done = true;
            return;
        }

        //println!("batch done");
        Box::into_raw(results);
        return;
    }

}

fn mysql_parallel_state_next(
    bind_data: &MysqlScanBindData,
    local_init_data: &MysqlScanLocalInitData,
    global_init_data: &MysqlScanGlobalInitData) -> bool {
    //todo!()
    true
}