use std::any::Any;
use std::ffi::CString;
use std::slice;
use duckdb_extension_framework::vector::Inserter;
use duckdb_extension_framework::{DataChunk, LogicalType, LogicalTypeId};
use duckdb_extension_framework::duckly::{duckdb_vector_size, idx_t};
use sqlx::mysql::{MySqlRow, MySqlValueRef};
use sqlx::{Error, Row};
use sqlx::error::UnexpectedNullError;
use crate::function::mysql_scan::mysql_scan_bind::{MysqlColumnInfo, MysqlTypeInfo};

fn duckdb_type_2(type_info: &MysqlTypeInfo) -> LogicalType {
    let mysql_type_name = &type_info.name;

    /*
    if mysql_type_name == "enum" {
        let enum_values_as_str = &type_info.enum_values;
        let enum_values = enum_values_as_str[1..enum_values_as_str.len() - 1]
            .split("','")
            .collect::<Vec<&str>>();
        let duckdb_levels = enum_values
            .iter()
            .map(|&value| value)
            .collect::<Vec<&str>>();
        return LogicalTypeId::Enum;
    }*/

    /*if mysql_type_name == "decimal" {
        return
            LogicalType::new_decimal_type(type_info.numeric_precision.unwrap(),
                                          type_info.numeric_scale.unwrap());
    }*/

    let logical_type_id = match mysql_type_name.as_str() {
        //"enum" => LogicalTypeId::Enum,
        "bit" => LogicalTypeId::Bit,
        "tinyint" => LogicalTypeId::Tinyint,
        "smallint" => LogicalTypeId::Smallint,
        "int" => LogicalTypeId::Integer,
        "bigint" => LogicalTypeId::Bigint,
        "float" => LogicalTypeId::Float,
        "double" => LogicalTypeId::Double,
        //TODO native implementation for enum
        "enum" | "char" | "bpchar" | "varchar" | "text" | "longtext" | "jsonb" | "json" => LogicalTypeId::Varchar,
        "date" => LogicalTypeId::Date,
        "bytea" => LogicalTypeId::Blob,
        "time" => LogicalTypeId::Time,
        "timestamp" => LogicalTypeId::Timestamp,
        "interval" => LogicalTypeId::Interval,
        "uuid" => LogicalTypeId::Uuid,
        _ => {
            println!("unknown type: {:?}", mysql_type_name);
            LogicalTypeId::Invalid
        },
    };

    return LogicalType::new(logical_type_id);
}

pub fn info_to_duckdb_type(info: &MysqlColumnInfo) -> LogicalType {
    duckdb_type_2(&info.type_info)
}

pub unsafe fn populate_column(
    row: &MySqlRow,
    col_type: &str,
    output: &DataChunk,
    row_idx: usize,
    col_idx: usize,
) {
    //println!("col_type: {:?} for col_idx {:?}", col_type, col_idx);
    match col_type {
        "VARCHAR" | "TEXT" | "CHAR" => {
            let val: Option<String> = row.try_get(col_idx).unwrap();
            val.map(|val| {
                set_bytes(output, row_idx, col_idx, val.as_bytes());
            }).or_else(||{
                set_invalid(output, row_idx, col_idx)
            });
        },
        "TINYINT" => {
            let val: Option<i8> = row.try_get(col_idx).unwrap();
            val.map(|val| {
                assign(output, row_idx, col_idx, val);
            }).or_else(||{
                set_invalid(output, row_idx, col_idx)
            });
        },
        "INT" => {
            let val: Option<i32> = row.try_get(col_idx).unwrap();
            val.map(|val| {
                assign(output, row_idx, col_idx, val);
            }).or_else(||{
                set_invalid(output, row_idx, col_idx)
            });
        },
        "BIGINT" => {
            let val: Option<i64> = row.try_get(col_idx).unwrap();
            val.map(|val| {
                assign(output, row_idx, col_idx, val);
            }).or_else(||{
                set_invalid(output, row_idx, col_idx)
            });
        },
        "TINYINT UNSIGNED" => {
            let val: Option<u8> = row.try_get(col_idx).unwrap();
            val.map(|val| {
                assign(output, row_idx, col_idx, val);
            }).or_else(||{
                set_invalid(output, row_idx, col_idx)
            });
        },
        "INT UNSIGNED" => {
            let val: Option<u32> = row.try_get(col_idx).unwrap();
            val.map(|val| {
                assign(output, row_idx, col_idx, val);
            }).or_else(||{
                set_invalid(output, row_idx, col_idx)
            });
        },
        "BIGINT UNSIGNED" => {
            let val: Option<u64> = row.try_get(col_idx).unwrap();
            val.map(|val| {
                assign(output, row_idx, col_idx, val);
            }).or_else(||{
                set_invalid(output, row_idx, col_idx)
            });
        },
        _ => {
            println!("Unsupported data type: {:?}", col_type);
        }
    }
}

pub unsafe fn set_invalid<T>(output: &DataChunk, row_idx: usize, col_idx: usize) -> Option<T> {
    let vec = output.get_vector::<T>(col_idx as idx_t);
    vec.ensure_validity_writable();
    vec.set_row_invalid(row_idx as idx_t);
    Option::<T>::None
}

pub unsafe fn assign<T: 'static>(output: &DataChunk, row_idx: usize, col_idx: usize, v: T) {
    get_column_result_vector::<T>(output, col_idx)[row_idx] = v;
}

pub unsafe fn get_column_result_vector<T>(output: &DataChunk, column_index: usize) -> &'static mut [T] {
    let result_vector = output.flat_vector(column_index);
    let ptr = result_vector.as_mut_ptr::<T>();
    slice::from_raw_parts_mut(ptr, duckdb_vector_size() as usize)
}

pub unsafe fn set_bytes(output: &DataChunk, row_idx: usize, col_idx: usize, bytes: &[u8]) {
    let cs = CString::new(bytes).unwrap();
    let result_vector = &mut output.flat_vector(col_idx);
    result_vector.insert(row_idx, cs.to_str().unwrap());
}
