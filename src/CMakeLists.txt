add_subdirectory(model)
add_subdirectory(state)
add_subdirectory(transformer)
add_subdirectory(util)

set(EXTENSION_SOURCES
    ${EXTENSION_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/mysql_scanner_extension.cpp
    PARENT_SCOPE)