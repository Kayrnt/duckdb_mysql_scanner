/*use duckdb_ext::LogicalType;
use duckdb_ext::ffi::idx_t;

pub struct MysqlTypeInfo {
    pub name: String,
    pub char_max_length: i64,
    pub numeric_precision: i64,
    pub numeric_scale: i64,
    pub enum_values: String,
}

pub struct MysqlColumnInfo {
    pub column_name: String,
    pub type_info: MysqlTypeInfo,
}

pub struct MysqlBindData {
    pub host: String,
    pub username: String,
    pub password: String,
    pub schema_name: String,
    pub table_name: String,
    pub approx_number_of_pages: idx_t,
    pub pages_per_task: idx_t,
    pub columns: Vec<MysqlColumnInfo>,
    pub names: Vec<String>,
    pub types: Vec<LogicalType>,
    pub needs_cast: Vec<bool>,
    pub snapshot: String,
    pub in_recovery: bool,
}

impl FunctionData for MysqlBindData {
    fn get_approx_number_of_pages(&self) -> idx_t {
        self.approx_number_of_pages
    }

    fn get_pages_per_task(&self) -> idx_t {
        self.pages_per_task
    }

    fn set_approx_number_of_pages(&mut self, approx_number_of_pages: idx_t) {
        self.approx_number_of_pages = approx_number_of_pages;
    }

    fn set_pages_per_task(&mut self, pages_per_task: idx_t) {
        self.pages_per_task = pages_per_task;
    }

    fn copy(&self) -> Box<dyn FunctionData> {
        unimplemented!(); // Not implemented for MysqlBindData
    }

    fn equals(&self, other: &dyn FunctionData) -> bool {
        unimplemented!(); // Not implemented for MysqlBindData
    }
}

impl MysqlBindData {
    fn new() -> Self {
        MysqlBindData {
            host: String::new(),
            username: String::new(),
            password: String::new(),
            schema_name: String::new(),
            table_name: String::new(),
            approx_number_of_pages: 0,
            pages_per_task: 1000,
            columns: Vec::new(),
            names: Vec::new(),
            types: Vec::new(),
            needs_cast: Vec::new(),
            snapshot: String::new(),
            in_recovery: false,
        }
    }
}
*/