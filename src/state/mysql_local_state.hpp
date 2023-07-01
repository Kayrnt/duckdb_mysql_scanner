#ifndef MYSQL_LOCAL_STATE_HPP
#define MYSQL_LOCAL_STATE_HPP

#include "duckdb.hpp"
#include "../../mysql/include/mysql/jdbc.h"

using namespace duckdb;

struct MysqlLocalState : public LocalTableFunctionState {
    ~MysqlLocalState() {
        if (result_set) {
            result_set->close();
            result_set = nullptr;
        }
        if (stmt) {
            // std::cout << "closing statement" << std::endl;
            stmt->close();
            stmt = nullptr;
        }
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
    sql::ResultSet* result_set = nullptr;
    sql::Statement* stmt = nullptr;
};

#endif // MYSQL_LOCAL_STATE_HPP
