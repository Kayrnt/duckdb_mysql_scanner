.PHONY: all clean format debug release duckdb_debug duckdb_release update
all: release
GEN=ninja

OSX_BUILD_UNIVERSAL_FLAG=
ifeq (${OSX_BUILD_UNIVERSAL}, 1)
	OSX_BUILD_UNIVERSAL_FLAG=-DOSX_BUILD_UNIVERSAL=1
endif

# BUILD_OUT_OF_TREE_EXTENSION
MYSQL_SCANNER_PATH=${PWD}

clean:
	rm -rf build
	rm -rf mysql

pull:
	git submodule init
	git submodule update --recursive --remote


debug:
	mkdir -p build/debug && \
	cd build/debug && \
	cmake -DCMAKE_BUILD_TYPE=Debug ${OSX_BUILD_UNIVERSAL_FLAG} \
				-DEXTENSION_STATIC_BUILD=1 ../../duckdb/CMakeLists.txt \
				-DEXTERNAL_EXTENSION_DIRECTORIES=${MYSQL_SCANNER_PATH} \
				-B. -S ../../duckdb && \
	cmake --build .


release: pull
	mkdir -p build/release && \
	cd build/release && \
	cmake -DCMAKE_BUILD_TYPE=Release ${OSX_BUILD_UNIVERSAL_FLAG} \
				-DEXTENSION_STATIC_BUILD=1 ../../duckdb/CMakeLists.txt \
				-DEXTERNAL_EXTENSION_DIRECTORIES=${MYSQL_SCANNER_PATH} \
				-B. -S ../../duckdb && \
	cmake --build .

test: release
	./build/release/test/unittest --test-dir . "[mysql_scanner]"

format: pull
	cp duckdb/.clang-format .
	clang-format --sort-includes=0 -style=file -i mysql_scanner.cpp
	cmake-format -i CMakeLists.txt
