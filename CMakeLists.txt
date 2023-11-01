# Define build variables
cmake_minimum_required(VERSION 3.21)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_CXX_STANDARD 20)

#set(CMAKE_C_COMPILER /opt/homebrew/bin/gcc-13)
#set(CMAKE_CXX_COMPILER /opt/homebrew/bin/g++-13)

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

# Setup SuperiorMySqlpp
#find_package(SuperiorMySqlpp REQUIRED)

# Extension sources files
include_directories(extension/include)
add_subdirectory(extension/src)

# Set the path to the MySQL Connector/C++ library
set(MYSQL_CONNECTOR_CPP_PATH ${CMAKE_CURRENT_SOURCE_DIR}/mysql)
# Set the path to the MySQL Connector/C++ headers
set(MYSQL_CONNECTOR_CPP_INCLUDE ${MYSQL_CONNECTOR_CPP_PATH}/include)
# Set the path to the MySQL Connector/C++ library files
set(MYSQL_CONNECTOR_CPP_LIB ${MYSQL_CONNECTOR_CPP_PATH}/lib64)
# Add the MySQL Connector/C++ headers
include_directories(${MYSQL_CONNECTOR_CPP_INCLUDE})
# Add the MySQL Connector/C++ library
link_directories(${MYSQL_CONNECTOR_CPP_LIB})

# Set the path to the MySQL Connector/C library
set(MYSQL_CONNECTOR_C_PATH ${CMAKE_CURRENT_SOURCE_DIR}/mysql-client)
# Set the path to the MySQL Connector/C headers
set(MYSQL_CONNECTOR_C_INCLUDE ${MYSQL_CONNECTOR_C_PATH}/include)
# Set the path to the MySQL Connector/C library files
set(MYSQL_CONNECTOR_C_LIB ${MYSQL_CONNECTOR_C_PATH}/lib64)
# Add the MySQL Connector/C headers
#include_directories(${MYSQL_CONNECTOR_C_INCLUDE})
# Add the MySQL Connector/C library
#link_directories(${MYSQL_CONNECTOR_C_LIB})

# Create the extension library
add_library(${EXTENSION_NAME} STATIC ${EXTENSION_SOURCES})

set(EXTENSION_DEPENDENCIES
   ${MYSQL_CONNECTOR_CPP_LIB}/libmysqlcppconn-static.a
   ${MYSQL_CONNECTOR_CPP_LIB}/libssl.dylib
   ${MYSQL_CONNECTOR_CPP_LIB}/libcrypto.dylib
   resolv
)
   #mysqlclient
   #SuperiorMySqlpp::SuperiorMySqlpp

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