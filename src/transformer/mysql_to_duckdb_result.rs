/*pub fn process_value(
    res: &ResultSetStream<Any>,
    type_: &LogicalType,
    type_info: &MysqlTypeInfo,
    out_vec: &mut Vector,
    query_col_idx: usize,
    output_offset: usize,
) {
    let col_idx = query_col_idx + 1;
    if res.is_null(col_idx) {
        out_vec.validity_mut().set(output_offset, false);
        return;
    }

    match type_.id() {
        LogicalTypeId::TinyInt => {
            let tiny_int_value = res.get_int(col_idx) as i8;
            out_vec.data_mut::<i8>()[output_offset] = tiny_int_value;
        }
        LogicalTypeId::SmallInt => {
            let small_int_value = res.get_int(col_idx) as i16;
            out_vec.data_mut::<i16>()[output_offset] = small_int_value;
        }
        LogicalTypeId::Integer => {
            let int_value = res.get_int(col_idx) as i32;
            out_vec.data_mut::<i32>()[output_offset] = int_value;
        }
        LogicalTypeId::UInteger => {
            let uint_value = res.get_uint(col_idx) as u32;
            out_vec.data_mut::<u32>()[output_offset] = uint_value;
        }
        LogicalTypeId::BigInt => {
            let big_int_value = res.get_int64(col_idx) as i64;
            out_vec.data_mut::<i64>()[output_offset] = big_int_value;
        }
        LogicalTypeId::Float => {
            let float_value = res.get_double(col_idx) as f32;
            out_vec.data_mut::<f32>()[output_offset] = float_value;
        }
        LogicalTypeId::Double => {
            let double_value = res.get_double(col_idx);
            out_vec.data_mut::<f64>()[output_offset] = double_value;
        }
        LogicalTypeId::VarChar | LogicalTypeId::Blob => {
            let mysql_str = res.get_string(col_idx).unwrap();
            let c_str = mysql_str.as_ptr();
            let value_len = mysql_str.len();
            let duckdb_str = StringVector::add_string_or_blob(out_vec, c_str, value_len);
            out_vec.data_mut::<&str>()[output_offset] = duckdb_str;
        }
        LogicalTypeId::Boolean => {
            let bool_value = res.get_boolean(col_idx);
            out_vec.data_mut::<bool>()[output_offset] = bool_value;
        }
        LogicalTypeId::Decimal => {
            let decimal_config = res.get_double(col_idx);
            match type_info.internal_type.as_str() {
                "int16" => {
                    let small_int_value = res.get_int(col_idx) as i16;
                    out_vec.data_mut::<i16>()[output_offset] = small_int_value;
                }
                "int32" => {
                    let int_value = res.get_int(col_idx) as i32;
                    out_vec.data_mut::<i32>()[output_offset] = int_value;
                }
                "int64" => {
                    let big_int_value = res.get_int64(col_idx) as i64;
                    out_vec.data_mut::<i64>()[output_offset] = big_int_value;
                }
                _ => panic!("Unsupported decimal storage type"),
            }
        }
        LogicalTypeId::Enum => {
            let mysql_str = res.get_string(col_idx).unwrap();
            let enum_val = mysql_str.as_ptr();
            let offset = EnumType::get_pos(type_.clone(), enum_val);
            if offset < 0 {
                panic!("Could not map ENUM value {}", enum_val);
            }
            match type_info.internal_type.as_str() {
                "uint8" => {
                    out_vec.data_mut::<u8>()[output_offset] = offset as u8;
                }
                "uint16" => {
                    out_vec.data_mut::<u16>()[output_offset] = offset as u16;
                }
                "uint32" => {
                    out_vec.data_mut::<u32>()[output_offset] = offset as u32;
                }
                _ => panic!("ENUM can only have unsigned integers as physical types"),
            }
        }
        _ => panic!("Unsupported Type"),
    }
}*/
