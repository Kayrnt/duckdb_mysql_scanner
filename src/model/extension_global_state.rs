use std::collections::HashMap;
use std::ffi::c_void;
use mysql::Pool;
use duckdb_extension_framework::duckly::duckdb_free;
use std::sync::{Mutex, Arc};
use lazy_static::lazy_static;

/**
The global state of the extension.
It contains references to shared resources, such as connection pools.

It contains a single instance of the global state, which is shared between all threads.
It is initialized once, when the extension is loaded.
It contains a map from connection url string to connection pool.
 */

use std::cell::RefCell;

// Create a type alias for the map
type PoolMap = HashMap<String, Arc<Mutex<RefCell<Pool>>>>;

// Create a mutex-protected global instance of the map
lazy_static! {
    static ref POOL_MAP: Mutex<PoolMap> = Mutex::new(HashMap::new());
}

// Function to get or create a connection pool for a given URL
pub fn get_connection_pool_for_url(url: &str) -> Arc<Mutex<RefCell<Pool>>> {
    let mut pool_map = POOL_MAP.lock().unwrap();
    let key = String::from(url);

    match pool_map.get(&key) {
        Some(pool) => pool.clone(),
        None => {
            let new_pool = Arc::new(Mutex::new(RefCell::new(Pool::new(url).unwrap())));
            pool_map.insert(key, new_pool.clone());
            new_pool
        }
    }
}

/*static CONNECTION_POOLS: Lazy<Mutex<HashMap<String, Box<Pool>>>> = Lazy::new(|| {
    let m = HashMap::new();
    Mutex::new(m)
});

pub fn get_connection_pool_for_url(url: String) -> &'static mut Box<Pool> {
    let mut cp = CONNECTION_POOLS.lock().unwrap();
    let entry = cp.entry(url.clone());

    let result = entry
        .or_insert_with(|| Box::new(Pool::new(url.as_str()).unwrap()));

    result
}*/

/*pub fn empty_connections_pool() {
    let mut cp = CONNECTION_POOLS.lock().unwrap();
    cp.clear();
}*/

