# Use an official Ubuntu base image
FROM ubuntu:22.04 as base
FROM base as builder

# Set environment variables
# ENV VCPKG_TARGET_TRIPLET=arm64-linux
# ENV VCPKG_TOOLCHAIN_PATH=/vcpkg/scripts/buildsystems/vcpkg.cmake

# Install required packages
RUN --mount=type=cache,target=/var/cache/apt apt-get update -y -qq \
    && apt-get install -y -qq software-properties-common \
    && add-apt-repository ppa:git-core/ppa \
    && apt-get update -y -qq \
    && apt-get install -y -qq ninja-build make libssl-dev \
                          wget openjdk-8-jdk zip maven unixodbc-dev \
                          libssl-dev libcurl4-gnutls-dev libexpat1-dev \
                          gettext unzip build-essential \
                          checkinstall libffi-dev curl libz-dev \
                          openssh-client libboost-all-dev \
                          libmysqlclient-dev libssl-dev cmake \
    && apt-get install -y -qq tar pkg-config \
    && apt-get install -y -qq git

# Install and configure vcpkg
#RUN git clone https://github.com/microsoft/vcpkg.git /vcpkg \
#    && cd /vcpkg \
#    && git checkout 501db0f17ef6df184fcdbfbe0f87cde2313b6ab1

# Copy the current directory contents into the container at /app
# COPY . /app

WORKDIR /usr/app

# Clone the repos
RUN git clone --depth 1 --branch v0.9.1 https://github.com/duckdb/duckdb.git
RUN git clone --depth 1 https://github.com/gabime/spdlog.git
RUN git clone --depth 1 https://github.com/mysql/mysql-connector-cpp

# Build mysql connector cpp
RUN mkdir -p build_mysql_conn \
    && cd build_mysql_conn \
    && cmake ../mysql-connector-cpp -DBUILD_STATIC=ON -DWITH_JDBC=ON \
    -DMYSQL_LIB_DIR=/usr/lib/aarch64-linux-gnu/ -D MYSQL_INCLUDE_DIR=/usr/include/mysql/  \
    && cmake --build . --config Release

WORKDIR /usr/app

# Copy extension sources
COPY ./extension ./extension
COPY ./CMakeLists.txt ./CMakeLists.txt
COPY ./Makefile ./Makefile

ENV MYSQL_CLIENT_DYN_LIB_PATH /usr/lib/aarch64-linux-gnu/libmysqlclient.so
ENV MYSQL_CPP_STATIC_LIB_PATH /usr/app/build_mysql_conn/jdbc/libmysqlcppconn-static.a

# Build the extension
RUN GEN=ninja make

FROM base as artifacts_container


CMD ["/bin/bash"]
