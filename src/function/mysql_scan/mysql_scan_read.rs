use duckdb_extension_framework::DataChunk;
use duckdb_extension_framework::duckly::{duckdb_data_chunk, duckdb_function_info};
use duckdb_extension_framework::table_functions::FunctionInfo;
use crate::function::mysql_scan::mysql_scan_bind::MysqlScanBindData;
use crate::function::mysql_scan::mysql_scan_global_init::MysqlScanGlobalInitData;
use crate::function::mysql_scan::mysql_scan_local_init::MysqlScanLocalInitData;

pub static STANDARD_VECTOR_SIZE: usize = 2048;

/// # Safety
///
/// .
#[no_mangle]
pub unsafe extern "C" fn read_mysql(info: duckdb_function_info, output: duckdb_data_chunk) {
    let info = FunctionInfo::from(info);
    let mut output = DataChunk::from(output);

    let bind_data = info.get_bind_data::<MysqlScanBindData>();
    let init_data = info.get_init_data::<MysqlScanGlobalInitData>();
    let local_init_data = info.get_local_init_data::<MysqlScanLocalInitData>();

    loop {
        if local_init_data.batch_done && !mysql_parallel_state_next(
            &bind_data,
            &local_init_data,
            &init_data) {
            return;
        }

        let mut output_offset = 0;

        while output_offset < STANDARD_VECTOR_SIZE {
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
        }

        local_init_data.current_page += 1;

        if local_init_data.current_page == local_init_data.end_page {
            local_init_data.done = true;
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