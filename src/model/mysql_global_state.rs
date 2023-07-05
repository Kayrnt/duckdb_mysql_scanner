use std::sync::Mutex;

struct MysqlGlobalState {
    lock: Mutex<()>,
    start_page: usize,
    max_threads: usize,
}

/*impl GlobalTableFunctionState for MysqlGlobalState {
    fn MaxThreads(&self) -> usize {
        self.max_threads
    }
}*/

impl Drop for MysqlGlobalState {
    fn drop(&mut self) {
        /*if let Some(pool) = self.pool.take() {
            pool.close();
        }*/
    }
}

impl MysqlGlobalState {
    fn new(max_threads: usize) -> Self {
        MysqlGlobalState {
            lock: Mutex::new(()),
            start_page: 0,
            max_threads,
        }
    }
}
