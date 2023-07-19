use duckdb_ext::ffi::idx_t;

struct AttachFunctionData {
    finished: bool,
    source_schema: String,
    sink_schema: String,
    suffix: String,
    overwrite: bool,
    filter_pushdown: bool,
    approx_number_of_pages: idx_t,
    pages_per_task: idx_t,
    host: String,
    username: String,
    password: String,
}

impl TableFunctionData for AttachFunctionData {
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
}

impl AttachFunctionData {
    fn new() -> Self {
        AttachFunctionData {
            finished: false,
            source_schema: "public".to_string(),
            sink_schema: "main".to_string(),
            suffix: "".to_string(),
            overwrite: false,
            filter_pushdown: true,
            approx_number_of_pages: 0,
            pages_per_task: 1000,
            host: String::new(),
            username: String::new(),
            password: String::new(),
        }
    }
}
