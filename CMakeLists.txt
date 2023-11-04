# Define build variables
cmake_minimum_required(VERSION 3.21)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 11)
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

# Extension sources files
include_directories(extension/include)
add_subdirectory(extension/src)

# Set the path to the MySQL Connector/C++ library
set(MYSQL_CONNECTOR_CPP_PATH ${CMAKE_CURRENT_SOURCE_DIR}/mysql-connector-cpp)

# Add the MySQL Connector/C++ headers
include_directories(${MYSQL_CONNECTOR_CPP_PATH}/build/include)

# Add the spdlog library
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include)

# Create the extension library
add_library(${EXTENSION_NAME} STATIC ${EXTENSION_SOURCES})

# export LDFLAGS="-L/opt/homebrew/opt/mysql-client/lib"
# export CPPFLAGS="-I/opt/homebrew/opt/mysql-client/include"

include_directories(${MYSQL_CONNECTOR_CPP_PATH}/build/include)


set(EXTENSION_DEPENDENCIES
   /opt/homebrew/opt/mysql-client/lib/libmysqlclient.dylib
   ${MYSQL_CONNECTOR_CPP_PATH}/build/jdbc/libmysqlcppconn-static.a
   #${CMAKE_CURRENT_SOURCE_DIR}/mysql/lib64/libmysqlcppconn-static.a
   /opt/homebrew/opt/openssl@1.1/lib/libssl.a
   /opt/homebrew/opt/openssl@1.1/lib/libcrypto.a
   #${CMAKE_CURRENT_SOURCE_DIR}/mysql/lib64/libssl.dylib
   #${CMAKE_CURRENT_SOURCE_DIR}/mysql/lib64/libcrypto.dylib
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