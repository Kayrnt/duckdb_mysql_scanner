/*
 * because we link twice (once to the rust library, and once to the duckdb library) we need a bridge to export the rust symbols
 * this is that bridge
 */

#include "extension.h"

const char* mysql_scanner_version_rust(void);
void mysql_scanner_init_rust(void* db);

DUCKDB_EXTENSION_API const char* mysql_scanner_version() {
    return mysql_scanner_version_rust();
}

DUCKDB_EXTENSION_API void mysql_scanner_init(void* db) {
    mysql_scanner_init_rust(db);
}
