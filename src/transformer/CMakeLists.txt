set(EXTENSION_SOURCES
    ${EXTENSION_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/duckdb_to_mysql_request.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mysql_to_duckdb_result.cpp
    PARENT_SCOPE
)