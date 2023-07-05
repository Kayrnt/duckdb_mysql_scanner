//use duckdb::column_t;
//use duckdb::LocalTableFunctionState;
//use duckdb::TableFilterSet;

struct MysqlLocalState {
    done: bool,
    exec: bool,
    base_sql: String,
    start_page: usize,
    current_page: usize,
    end_page: usize,
    pagesize: usize,
    //column_ids: Vec<column_t>,
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
            //column_ids: Vec::new(),
            //filters: TableFilterSet::new()
        }
    }
}
