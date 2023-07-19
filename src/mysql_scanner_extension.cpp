#define DUCKDB_EXTENSION_MAIN

/*
 * because we link twice (once to the rust library, and once to the duckdb library) we need a bridge to export the rust symbols
 * this is that bridge
 */

#include "include/mysql_scanner_extension.hpp"
#include "duckdb.hpp"

extern "C" const char* mysql_scanner_version_rust(void);
extern "C" void mysql_scanner_init_rust(void* db);

namespace duckdb {

void Mysql_scannerExtension::Load(DuckDB &db) {
  void *db_ptr = &db;
	mysql_scanner_init_rust(db_ptr);
}

std::string Mysql_scannerExtension::Name() {
	return "mysql_scanner";
}

} // namespace duckdb

extern "C" {

 DUCKDB_EXTENSION_API const char* mysql_scanner_version() {
     return mysql_scanner_version_rust();
 }

 DUCKDB_EXTENSION_API void mysql_scanner_init(duckdb::DatabaseInstance &db) {
     void *db_ptr = &db;
     	mysql_scanner_init_rust(db_ptr);
 }

}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif