.PHONY: all clean build release update

all: release

OSX_BUILD_UNIVERSAL_FLAG=
ifeq (${OSX_BUILD_UNIVERSAL}, 1)
	OSX_BUILD_UNIVERSAL_FLAG=-DOSX_BUILD_UNIVERSAL=1
endif

# On Github, if we want to cross-compile, we have to explicitly set the target architecture.
# This is because the corrosion-rs thing that's used doesn't support universal binaries.
# The only way I've seen folks do it is by using the (unmaintained) cargo-lipo or
# by building each target and using lipo manually as done here:
# https://github.com/walles/riff/blob/82f77c82e7306dd69d343640670bdf9d31cc0b0b/release.sh#L132-L136
# For us, we'll create an ARM64 flag we can use as the default target in GitHub mac runners is intel
OSX_BUILD_AARCH64_FLAG=
ifeq (${OSX_BUILD_AARCH64}, 1)
	OSX_BUILD_AARCH64_FLAG=-DOSX_BUILD_AARCH64=1
endif

VCPKG_TOOLCHAIN_PATH?=
ifneq ("${VCPKG_TOOLCHAIN_PATH}", "")
	TOOLCHAIN_FLAGS:=${TOOLCHAIN_FLAGS} -DVCPKG_MANIFEST_DIR='${PROJ_DIR}' -DVCPKG_BUILD=1 -DCMAKE_TOOLCHAIN_FILE='${VCPKG_TOOLCHAIN_PATH}'
endif
ifneq ("${VCPKG_TARGET_TRIPLET}", "")
	TOOLCHAIN_FLAGS:=${TOOLCHAIN_FLAGS} -DVCPKG_TARGET_TRIPLET='${VCPKG_TARGET_TRIPLET}'
endif

ifeq ($(GEN),ninja)
	GENERATOR=-G "Ninja"
	FORCE_COLOR=-DFORCE_COLORED_OUTPUT=1
endif

MKFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
PROJ_DIR := $(dir $(MKFILE_PATH))
$(info $(PROJ_DIR))

BUILD_FLAGS=-DEXTENSION_STATIC_BUILD=1 -DSKIP_EXTENSIONS=parquet -DDISABLE_BUILTIN_EXTENSIONS=1 -DCLANG_TIDY=False ${OSX_BUILD_UNIVERSAL_FLAG} ${OSX_BUILD_AARCH64_FLAG}

# These flags will make DuckDB build the extension
EXTENSION_FLAGS=\
-DDUCKDB_EXTENSION_NAMES="mysql_scanner" \
-DDUCKDB_EXTENSION_MYSQL_SCANNER_PATH="$(PROJ_DIR)" \
-DDUCKDB_EXTENSION_MYSQL_SCANNER_SHOULD_LINK=0 \

clean:
	rm -rf build
	cargo clean

# Debug build
build:
	mkdir -p build/debug && \
	cmake $(GENERATOR) $(FORCE_COLOR) -DCMAKE_BUILD_TYPE=Debug ${BUILD_FLAGS} \
	-S ./duckdb/ ${EXTENSION_FLAGS} -B build/debug && \
	cmake --build build/debug --config Debug

release:
	mkdir -p build/release && \
	cmake $(GENERATOR) $(FORCE_COLOR) -DCMAKE_BUILD_TYPE=Release ${BUILD_FLAGS} \
	-S ./duckdb/ $(EXTENSION_FLAGS) -B build/release && \
	cmake --build build/release --config Release

update:
	git submodule update --remote --merge