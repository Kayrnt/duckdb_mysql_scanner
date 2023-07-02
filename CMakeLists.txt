# Define build variables
cmake_minimum_required(VERSION 3.2)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(TARGET_NAME mysql_scanner)
set(CMAKE_CXX_STANDARD 11)
set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(FULL_PATH_TO_MYSQL_CONNECTOR_CPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mysql)
project(${TARGET_NAME})


# Setup OpenSSL
set(OPENSSL_USE_STATIC_LIBS ON)
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(
  ${FULL_PATH_TO_MYSQL_CONNECTOR_CPP_DIR}/include
)

# Link directories
link_directories(${FULL_PATH_TO_MYSQL_CONNECTOR_CPP_DIR}/lib64)

# Add subdirectory for source files
add_subdirectory(src)

# Create the extension library
add_library(${EXTENSION_NAME} STATIC ${EXTENSION_SOURCES})

# Set linker flags and libraries
set(CMAKE_VERBOSE_MAKEFILE ON)
set(PARAMETERS "-warnings")

# Build the loadable extension target
build_loadable_extension(${TARGET_NAME} ${PARAMETERS} ${EXTENSION_SOURCES})

# Link the loadable extension target with libraries
target_link_libraries(${TARGET_NAME}_loadable_extension OpenSSL::SSL OpenSSL::Crypto -lmysqlcppconn)

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")