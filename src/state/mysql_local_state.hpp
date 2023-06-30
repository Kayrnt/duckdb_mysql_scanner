#ifndef MYSQL_LOCAL_STATE_HPP
#define MYSQL_LOCAL_STATE_HPP

#include "duckdb.hpp"
#include "../../mysql/include/mysql/jdbc.h"

using namespace duckdb;

struct MysqlLocalState : public LocalTableFunctionState {
    ~MysqlLocalState() {
        if (conn) {
            conn->close();
            conn = nullptr;
        }
    }

    bool done = false;
    bool exec = false;
    std::string base_sql = "";

    idx_t start_page = 0;
    idx_t current_page = 0;
    idx_t end_page = 0;
    idx_t pagesize = 0;

    std::vector<column_t> column_ids;
    TableFilterSet* filters;
    sql::Connection* conn = nullptr;
};

#endif // MYSQL_LOCAL_STATE_HPP
