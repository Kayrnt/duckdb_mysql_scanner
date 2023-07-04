# Define build variables
cmake_minimum_required(VERSION 3.22)

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()

SET(TARGET_NAME mysql_scanner)
project(c_${TARGET_NAME} VERSION 0.1)
set(EXTENSION_NAME mysql_scanner)

if (APPLE)
  # POLICY CMP0042
  set(CMAKE_MACOSX_RPATH 1)
endif()

include(FetchContent)

if(UNIX AND NOT APPLE)
        find_package(OpenSSL REQUIRED)
endif()

FetchContent_Declare(
        Corrosion
        GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
        GIT_TAG v0.3.2 # Optionally specify a commit hash, version tag or branch here
)
set(BUILD_UNITTESTS FALSE) # Disable unit test build in duckdb

# Setup OpenSSL
set(OPENSSL_USE_STATIC_LIBS ON)
find_package(OpenSSL REQUIRED)

FetchContent_MakeAvailable(Corrosion)

corrosion_import_crate(MANIFEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/duckdb/src/include)

set(ALL_SOURCES src/extension.c src/extension.h)

SET(EXTENSION_STATIC_BUILD 1)
set(PARAMETERS "-no-warnings")
build_loadable_extension(${EXTENSION_NAME} ${PARAMETERS} ${ALL_SOURCES})

set(LIB_NAME ${EXTENSION_NAME}_loadable_extension)

set_target_properties(${LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${LIB_NAME}
        "${CMAKE_CURRENT_BINARY_DIR}/librs_duckdb_mysql.a"
        duckdb_static
	${OPENSSL_LIBRARIES}
)

if (APPLE)
        target_link_libraries(${LIB_NAME}
                "-framework CoreFoundation"
                "-framework Security"
                "-framework Accelerate")
endif()
