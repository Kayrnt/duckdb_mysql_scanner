//use duckdb::column_t;
//use duckdb_ext::LocalTableFunctionState;
//use duckdb::TableFilterSet;

pub struct MysqlLocalState {
    pub done: bool,
    pub exec: bool,
    pub base_sql: String,
    pub start_page: usize,
    pub current_page: usize,
    pub end_page: usize,
    pub pagesize: usize,
    pub column_ids: Vec<usize>,
    //filters: TableFilterSet
}

impl Drop for MysqlLocalState {
    fn drop(&mut self) {
    }
}

//impl LocalTableFunctionState for MysqlLocalState {}

impl MysqlLocalState {
    fn new() -> Self {
        MysqlLocalState {
            done: false,
            exec: false,
            base_sql: String::new(),
            start_page: 0,
            current_page: 0,
            end_page: 0,
            pagesize: 0,
            column_ids: Vec::new(),
            //filters: TableFilterSet::new()
        }
    }
}
