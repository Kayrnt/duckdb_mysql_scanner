# Define build variables
cmake_minimum_required(VERSION 3.21)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 11)

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()

# Set extension name here
set(TARGET_NAME mysql_scanner)

set(EXTENSION_NAME ${TARGET_NAME}_extension)
project(${TARGET_NAME})
set(PARAMETERS "-no-warnings")

if (APPLE)
  # POLICY CMP0042
  set(CMAKE_MACOSX_RPATH 1)
endif()

set(BUILD_UNITTESTS FALSE) # Disable unit test build in duckdb

# Setup OpenSSL
set(OPENSSL_USE_STATIC_LIBS ON)
find_package(OpenSSL REQUIRED)

# Extension sources files
include_directories(extension/include)
add_subdirectory(extension/src)

# Add the MySQL Connector/C++ headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/build_mysql_conn/include)

# Add the spdlog library
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include)

# Create the extension library
add_library(${EXTENSION_NAME} STATIC ${EXTENSION_SOURCES})

# Check if MYSQL_CLIENT_DYN_LIB_PATH is set
if(DEFINED ENV{MYSQL_CLIENT_DYN_LIB_PATH})
    message(STATUS "Using MySQL dynamic library path from environment variable: $ENV{MYSQL_CLIENT_DYN_LIB_PATH}")
else()
    # If MYSQL_CLIENT_DYN_LIB_PATH is not set, set a default value
    set(MYSQL_CLIENT_DYN_LIB_PATH /opt/homebrew/opt/mysql-client/lib/libmysqlclient.dylib)
    message(STATUS "Using default MySQL dynamic library path: $ENV{MYSQL_CLIENT_DYN_LIB_PATH}")
endif()

# Check if MYSQL_CPP_STATIC_LIB_PATH is set
if(DEFINED ENV{MYSQL_CPP_STATIC_LIB_PATH})
    message(STATUS "Using MySQL Connector CPP library path from environment variable: $ENV{MYSQL_CPP_STATIC_LIB_PATH}")
else()
    # If MYSQL_CPP_STATIC_LIB_PATH is not set, set a default value
    set(MYSQL_CPP_STATIC_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build_mysql_conn/jdbc/libmysqlcppconn-static.a)
    message(STATUS "Using default MySQL dynamic library path: $ENV{MYSQL_CPP_STATIC_LIB_PATH}")
endif()

set(EXTENSION_DEPENDENCIES
   ${MYSQL_CLIENT_DYN_LIB_PATH}
   ${MYSQL_CPP_STATIC_LIB_PATH}
   resolv
)

# Add dependencies to extension
target_link_libraries(${EXTENSION_NAME} PUBLIC ${EXTENSION_DEPENDENCIES})

# Set linker flags and libraries
set(PARAMETERS "-warnings")

# Build the loadable extension target
build_loadable_extension(${TARGET_NAME} ${PARAMETERS} ${EXTENSION_SOURCES})
target_link_libraries(${TARGET_NAME}_loadable_extension ${EXTENSION_DEPENDENCIES})

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")