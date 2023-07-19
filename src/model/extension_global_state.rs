use std::collections::HashMap;
use std::sync::{Mutex, Arc};
use lazy_static::lazy_static;
use std::cell::RefCell;
use sqlx::{MySql, Pool};
use sqlx::mysql::MySqlPoolOptions;
use crate::function::mysql_scan::mysql_scan_bind::MysqlTableTypeInfos;

/**
The global state of the extension.
It contains references to shared resources, such as connection pools.

It contains a single instance of the global state, which is shared between all threads.
It is initialized once, when the extension is loaded.
It contains a map from connection url string to connection pool.
 */

// Create a type alias for the map
type PoolMap = HashMap<String, Arc<Mutex<RefCell<Pool<MySql>>>>>;
type MysqlTableTypeInfosMap = HashMap<(String, String), Arc<Mutex<RefCell<MysqlTableTypeInfos>>>>;

// Create a mutex-protected global instance of the map
lazy_static! {
    static ref POOL_MAP: Mutex<PoolMap> = Mutex::new(HashMap::new());
}
lazy_static! {
    static ref MYSQL_TABLE_TYPE_INFOS_MAP: Mutex<MysqlTableTypeInfosMap> = Mutex::new(HashMap::new());
}

// Function to get or create a connection pool for a given URL
pub async fn get_connection_pool_for_url(url: &str) -> Arc<Mutex<RefCell<Pool<MySql>>>> {
    let mut pool_map = POOL_MAP.lock().unwrap();
    let key = String::from(url);

    match pool_map.get(&key) {
        Some(pool) => pool.clone(),
        None => {
            //print connection url to console
            println!("Connecting to {}", url);
            let options = MySqlPoolOptions::new().max_connections(5);
            let p: Pool<MySql> = options.connect(url).await.expect("Failed to connect to the database");
            let new_pool = Arc::new(Mutex::new(RefCell::new(p)));
            pool_map.insert(key, new_pool.clone());
            new_pool
        }
    }
}

pub fn get_mysql_table_type_infos_for_schema_table(schema: &str, table: &str) -> Option<Arc<Mutex<RefCell<MysqlTableTypeInfos>>>> {
    let mut mysql_table_type_infos_map = MYSQL_TABLE_TYPE_INFOS_MAP.lock().unwrap();
    let key = (String::from(schema), String::from(table));

    match mysql_table_type_infos_map.get(&key) {
        Some(mysql_table_type_infos) => Some(mysql_table_type_infos.clone()),
        None => None
    }
}

pub fn insert_mysql_table_type_infos_for_schema_table(
    schema: &str,
    table: &str,
    mysql_table_type_infos: MysqlTableTypeInfos) {
    let mut mysql_table_type_infos_map = MYSQL_TABLE_TYPE_INFOS_MAP.lock().unwrap();
    let key = (String::from(schema), String::from(table));

    mysql_table_type_infos_map.insert(key, Arc::new(Mutex::new(RefCell::new(mysql_table_type_infos))));
}
