# Define build variables
cmake_minimum_required(VERSION 3.22)

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

include(FetchContent)

FetchContent_Declare(
        Corrosion
        GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
        GIT_TAG v0.3.2 # Optionally specify a commit hash, version tag or branch here
)
set(BUILD_UNITTESTS FALSE) # Disable unit test build in duckdb

# Setup OpenSSL
set(OPENSSL_USE_STATIC_LIBS ON)
find_package(OpenSSL REQUIRED)

# Setup ZLIB
find_package(ZLIB REQUIRED)

FetchContent_MakeAvailable(Corrosion)
corrosion_import_crate(MANIFEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/duckdb-extension-framework/duckdb/src/include)
include_directories(src/include)

set(ALL_SOURCES src/mysql_scanner_extension.cpp)
build_loadable_extension(${TARGET_NAME} ${PARAMETERS} ${ALL_SOURCES})

set(LIB_NAME ${TARGET_NAME}_loadable_extension)

target_link_libraries(${LIB_NAME}
                      "${CMAKE_CURRENT_BINARY_DIR}/librs_duckdb_mysql_scanner.a"
                      ZLIB::ZLIB
                      duckdb_static
	                    ${OPENSSL_LIBRARIES}
)

if (APPLE)
        target_link_libraries(${LIB_NAME}
                "-framework CoreFoundation"
                "-framework Security"
                "-framework Accelerate")
endif()