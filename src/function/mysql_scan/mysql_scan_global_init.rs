use duckdb_extension_framework::duckly::{duckdb_free, duckdb_init_info};
use duckdb_extension_framework::table_functions::InitInfo;
use std::cell::RefCell;
use std::ffi::{c_char, c_void, CString};
use std::sync::{Arc, Mutex};
use duckdb_extension_framework::{LogicalType, LogicalTypeId, malloc_struct};
use duckdb_extension_framework::table_functions::BindInfo;
use futures::StreamExt;
use mysql::{Pool, params, prelude::Queryable};
use mysql::prelude::FromRow;
use crate::function::mysql_scan::mysql_scan_bind::MysqlScanBindData;
use crate::model::duckdb_type::duckdb_type;
use crate::model::extension_global_state::get_connection_pool_for_url;

#[repr(C)]
pub struct MysqlScanGlobalInitData {
    pub scan_done: bool,
    pub current_thread_start_page: u32,
    pub max_threads: u32,
    pub pushdown_column_ids: Vec<u64>,
    pub mysql_type_infos: MysqlTableTypeInfos,
    pub base_sql: String
}

pub fn duckdb_to_mysql_request(bind_data: &MysqlScanBindData,
                               &column_ids: Vec<u64>) -> String {
    let type_infos = bind_data.mysql_type_infos;

    let mut col_names = String::new();
    col_names = column_ids.map(|&column_id| {
        let name = type_infos.names[column_id].to_owned();
        let needs_cast: bool = type_infos.needs_cast[column_id];
        format!("`{}`{}", name, if needs_cast { "::VARCHAR" } else { "" })
    }).collect::<Vec<_>>().join(", ");

    let mut filter_string = String::new();
    /*if let Some(filters) = &lstate.filters {
        if !filters.filters.is_empty() {
            let filter_entries = filters.filters.iter().map(|(column_id, filter)| {
                let column_name = format!("`{}`", bind_data.names[lstate.column_ids[*column_id]]);
                transform_filter(&column_name, filter)
            }).collect::<Vec<_>>();
            filter_string = format!(" WHERE {}", filter_entries.join(" AND "));
        }
    }*/

    format!(
        "SELECT {} FROM `{}`.`{}` {}",
        col_names, bind_data.schema_name, bind_data.table_name, filter_string
    )
}

#[repr(C)]
#[derive(Debug)]
pub struct MysqlTableTypeInfos {
    pub columns: Vec<MysqlColumnInfo>,
    pub names: Vec<String>,
    pub types: Vec<LogicalType>,
    pub needs_cast: Vec<bool>,
}

impl MysqlTableTypeInfos {
    pub fn new(columns: Vec<MysqlColumnInfo>, names: Vec<String>, types: Vec<LogicalType>, needs_cast: Vec<bool>) -> Self {
        Self {
            columns,
            names,
            types,
            needs_cast,
        }
    }
}

#[repr(C)]
#[derive(Debug)]
pub struct MysqlColumnInfo {
    pub column_name: String,
    pub type_info: MysqlTypeInfo,
}

#[repr(C)]
#[derive(Debug)]
pub struct MysqlTypeInfo {
    pub name: String,
    pub char_max_length: Option<i32>,
    pub numeric_precision: Option<u8>,
    pub numeric_scale: Option<u8>,
    pub enum_values: Option<String>,
}

fn get_table_types_infos(
    connection_pool: Arc<Mutex<RefCell<Pool>>>,
    bind_info: &MysqlScanBindData,
    schema_name: &str,
    table_name: &str,
) -> Result<MysqlTableTypeInfos, Box<dyn std::error::Error>> {
    let mut columns = Vec::new();
    let mut names = Vec::new();
    let mut types = Vec::new();
    let mut needs_cast = Vec::new();

    let mut conn = connection_pool.lock().unwrap().borrow().get_conn()?;
    let query = format!(
        "SELECT column_name,
			    DATA_TYPE,
       		    character_maximum_length as char_max_length,
			    numeric_precision,
			    numeric_scale,
                IF(DATA_TYPE = 'enum', SUBSTRING(COLUMN_TYPE,5), NULL) enum_values
        FROM    information_schema.columns
        WHERE   table_schema = '{}'
        AND     table_name = '{}'",
        schema_name, table_name
    );

    let infos = conn.query_map(
        query,
        |(column_name, data_type, char_max_length, numeric_precision, numeric_scale, enum_values)|
            {
                let info1 = MysqlColumnInfo {
                    column_name,
                    type_info: MysqlTypeInfo {
                        name: data_type,
                        char_max_length,
                        numeric_precision,
                        numeric_scale,
                        enum_values,
                    },
                };
                info1
            },
    ).expect("Query failed.");

    // Check if the table has any columns
    if infos.len() == 0 {
        return Err(format!("Table {} does not contain any columns.", table_name).into());
    }

    for info in infos {
        names.push(info.column_name.clone());

        let duckdb_type = duckdb_type(&info);

        let col_needs_cast = duckdb_type.type_id() == LogicalTypeId::Invalid;
        if col_needs_cast {
            types.push(LogicalType::new(LogicalTypeId::Varchar));
        } else {
            types.push(duckdb_type.clone());
        }

        bind_info.add_result_column(info.column_name.clone().as_str(), duckdb_type);
        needs_cast.push(col_needs_cast);
        columns.push(info);
    }

    Ok(MysqlTableTypeInfos::new(columns, names, types, needs_cast))
}


/// # Safety
///
/// .
#[no_mangle]
pub unsafe extern "C" fn read_mysql_init(info: duckdb_init_info) {
    let global_init_data = InitInfo::from(info);
    let bind_data = global_init_data.get_bind_data::<MysqlScanBindData>();
    let column_ids = global_init_data.get_column_indices();
    let url = bind_data.url.to_string().as_str();

    let schema = bind_data.schema.to_string().as_str();
    let table = bind_data.table.to_string().as_str();

    let mut my_global_init_data = malloc_struct::<MysqlScanGlobalInitData>();
    my_global_init_data.scan_done = false;
    my_global_init_data.current_thread_start_page = 0;
    my_global_init_data.max_threads = 1;
    my_global_init_data.pushdown_column_ids = column_ids;

    //create the connection pool to mysql using URL
    let pool: Arc<Mutex<RefCell<Pool>>> = get_connection_pool_for_url(&url);

    let mut connection = pool.lock().unwrap().borrow().get_conn().unwrap();

    let query = format!("SELECT COUNT(*) FROM {}.{}", schema, table);
    let count_rows: Option<u64> = connection.query_first(query).unwrap();

    // print the result
    println!("Count: {}", count_rows.unwrap());

    match get_table_types_infos(pool,
                                &bind_data,
                                schema,
                                table,
    ) {
        Ok(table_type_infos) => {
            println!("Column infos: {:?}", table_type_infos);
            my_global_init_data.mysql_type_infos = table_type_infos;
        }
        Err(err) => {
            println!("Couldn't retrieve column infos for table {}.{}: {}", schema, table, err);
        }
    };

    global_init_data.set_init_data(my_global_init_data.cast(), Some(duckdb_free));
}

