use std::cell::RefCell;
use std::ffi::{c_char, c_void, CStr, CString};
use std::sync::{Arc, Mutex};
use duckdb_extension_framework::duckly::{duckdb_bind_info, duckdb_free};
use duckdb_extension_framework::{LogicalType, LogicalTypeId, malloc_struct};
use duckdb_extension_framework::table_functions::BindInfo;
use futures::executor::block_on;
use sqlx::{MySql, Pool, Row};
use sqlx::mysql::MySqlRow;
use crate::model::duckdb_type::info_to_duckdb_type;
use crate::model::extension_global_state::{get_connection_pool_for_url, insert_mysql_table_type_infos_for_schema_table};

#[repr(C)]
#[derive(Debug, Clone)]
pub struct MysqlScanBindData {
    pub url: *mut c_char,
    pub schema: *mut c_char,
    pub table: *mut c_char,
}

impl MysqlScanBindData {
    pub fn get_url(&self) -> &str {
        //unsafe { CString::from_raw(self.url).to_string_lossy().into_owned() }
        unsafe {
            let cstr = CStr::from_ptr(self.url);
            cstr.to_str().unwrap()
        }
    }

    pub fn get_schema(&self) -> &str {
        unsafe {
            let cstr = CStr::from_ptr(self.schema);
            cstr.to_str().unwrap()
        }
    }

    pub fn get_table(&self) -> &str {
        unsafe {
            let cstr = CStr::from_ptr(self.table);
            cstr.to_str().unwrap()
        }
    }
}

const DEFAULT_LIMIT: i32 = 10000;

#[repr(C)]
#[derive(Debug, Clone)]
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
#[derive(Debug, Clone)]
pub struct MysqlColumnInfo {
    pub column_name: String,
    pub type_info: MysqlTypeInfo,
}

#[repr(C)]
#[derive(Debug, Clone)]
pub struct MysqlTypeInfo {
    pub name: String,
    pub char_max_length: Option<i64>,
    pub numeric_precision: Option<u8>,
    pub numeric_scale: Option<u8>,
    pub enum_values: Option<String>,
}

/// Drop the ScanBindData from C.
///
/// # Safety
unsafe extern "C" fn drop_scan_bind_data_c(v: *mut c_void) {
    //let actual = v.cast::<MysqlScanBindData>();
    //println!("Dropping ScanBindData: {:?}", actual);
    // drop(CString::from_raw((*actual).schema.cast()));
    // drop(CString::from_raw((*actual).table.cast()));
    // drop(CString::from_raw((*actual).url.cast()));
    //println!("Dropped ScanBindData: {:?}", actual);
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
    (*my_bind_data).url = CString::new(url.clone()).expect("MySQL url").into_raw();
    (*my_bind_data).schema = CString::new(schema.clone()).expect("Schema name").into_raw();
    (*my_bind_data).table = CString::new(table.clone()).expect("Table name").into_raw();

    block_on(async {
        //create the connection pool to mysql using URL
        let pool: Arc<Mutex<RefCell<Pool<MySql>>>> = get_connection_pool_for_url(&url).await;
        //println!("Retrieve column infos...");
        match get_table_types_infos(pool,
                                    &bind_info,
                                    schema,
                                    table,
        ).await {
            Ok(table_type_infos) => {
                //println!("Column infos: {:?}", table_type_infos);
                //(*my_bind_data).mysql_type_infos = *table_type_infos;
                insert_mysql_table_type_infos_for_schema_table(schema, table, table_type_infos);
                //println!("Malloc'ed column infos");
            }
            Err(err) => {
                println!("Couldn't retrieve column infos for table {}.{}: {}", schema, table, err);
            }
        };

        //println!("Bind data: {:?}", my_bind_data);
        // Set the bind data
        bind_info.set_bind_data(my_bind_data.cast(), Some(drop_scan_bind_data_c));
    });
}

async fn get_table_types_infos(
    connection_pool: Arc<Mutex<RefCell<Pool<MySql>>>>,
    bind_info: &BindInfo,
    schema_name: &str,
    table_name: &str,
) -> Result<MysqlTableTypeInfos, Box<dyn std::error::Error>> {
    let mut columns = Vec::new();
    let mut names = Vec::new();
    let mut types = Vec::new();
    let mut needs_cast = Vec::new();

    let pool = connection_pool.lock().unwrap();

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

    let infos = sqlx::query(query.as_str())
        .map(|row: MySqlRow| {
            let column_name: String = row.get(0);
            let data_type: String = row.get(1);
            let char_max_length: Option<i64> = row.get(2);
            let numeric_precision: Option<u8> = row.get(3);
            let numeric_scale: Option<u8> = row.get(4);
            let enum_values: Option<String> = row.get(5);

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
        }
        ).fetch_all(&*(pool.borrow())).await.unwrap();

    // Check if the table has any columns
    if infos.len() == 0 {
        return Err(format!("Table {} does not contain any columns.", table_name).into());
    }

    for info in infos {
        names.push(info.column_name.clone());

        let duckdb_type = info_to_duckdb_type(&info);

        let col_needs_cast = duckdb_type.type_id() == LogicalTypeId::Invalid;
        if col_needs_cast {
            types.push(LogicalType::new(LogicalTypeId::Varchar));
        } else {
            types.push(duckdb_type.clone());
        }

        (*bind_info).add_result_column(info.column_name.clone().as_str(), duckdb_type);
        needs_cast.push(col_needs_cast);
        columns.push(info);
    }

    Ok(MysqlTableTypeInfos::new(columns, names, types, needs_cast))
}
