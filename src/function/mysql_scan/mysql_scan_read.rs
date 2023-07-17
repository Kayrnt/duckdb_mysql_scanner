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
            init_data_p.as_ref().unwrap()
        ).await;
    });

    loop {
        /*if (*local_init_data_p).batch_done && !mysql_parallel_state_next(
            &bind_data_p.as_ref().unwrap(),
            &local_init_data_p.as_mut().unwrap(),
            init_data_p.as_ref().unwrap()) {
            return;
        }*/

        let r = (*local_init_data_p).results;
        let results = unsafe { Box::from_raw(r as *mut Vec<MySqlRow>) };

        println!("(*local_init_data_p).results LEN {}", results.len());

        for res in results.iter() {
            for col in res.columns() {
                let type_info: &MySqlTypeInfo = col.type_info();
                match type_info.name() {
                    "VARCHAR" | "TEXT" => {
                        let val: String = res.try_get(col.ordinal()).unwrap();
                        println!("val: {}", val);
                    },
                    "INT" => {
                        let val: i32 = res.try_get(col.ordinal()).unwrap();
                        println!("val: {}", val);
                    },
                    _ => {
                        println!("unknown type");
                    }
                }
                match res.try_get::<String, usize>(col.ordinal()) {
                    Ok(val) => {
                        println!("val: {}", val);
                    },
                    Err(e) => {
                        println!("error: {}", e);
                    }
                };
                //row.data[col].push(val);
            }
            //output.append(&row);
        }


        let mut output_offset = 0;

        /*while output_offset < STANDARD_VECTOR_SIZE {
            let current_item_opt = local_init_data.result_set.next();
            if current_item_opt.is_none() {
                local_init_data.batch_done = true;
                break;
            }

            for query_col_idx in 0..output.column_count() {
                let table_col_idx = bind_data.mysql_type_infos.columns[query_col_idx];
                let out_vec = &mut output.get_vector(query_col_idx);

                process_value(
                    &mut local_init_data.result_set,
                    bind_data.types[table_col_idx],
                    &mut bind_data.columns[table_col_idx].type_info,
                    out_vec,
                    query_col_idx,
                    output_offset,
                );
            }

            output_offset += 1;
            output.set_cardinality(output_offset);
        }*/

        (*local_init_data_p).current_page += 1;

        if (*local_init_data_p).current_page == (*local_init_data_p).end_page {
            (*local_init_data_p).batch_done = true;
        }

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