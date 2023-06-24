# The project is in progress, little is working so far

# DuckDB mysql_scanner extension

The mysql_scanner extension allows DuckDB to directly read data from a running MySQL instance. The data can be queried directly from the underlying MySQL tables, or read into DuckDB tables.

## Usage (TODO)

First load the extension:
```SQL
LOAD 'mysql_scanner.duckdb_extension';
```

**So far only `SELECT mysql_version();` is working to connect to a localhost mysql with `root` and no password**

To make a MYSQL database accessible to DuckDB, use the `MYSQL_ATTACH` command:
```SQL
CALL MYSQL_ATTACH('127.0.0.1', 'username', 'password');
```
`MYSQL_ATTACH` takes a three required string parameters:

- the host (ip or name)
- the user name
- the password for that user

## Building & Loading the Extension

### Build

Download the C++ extension for MySQL on [MySQL dev connector C++](https://dev.mysql.com/downloads/connector/cpp/).
Extract the sources to `mysql` directory in the project root (you should have a mysql/include and lib or lib64 directories).

OpenSSL library is required to build the extension. For instance, on MacOS, you can install it with Homebrew:
```
brew install openssl
```

To build, from the project root directory, type 
```
make
```

### Run

To run, run the bundled `duckdb` shell:
```
 ./build/release/duckdb -unsigned  # allow unsigned extensions
```

### Use

Then, load the MYSQL extension like so:
```SQL
LOAD 'build/release/extension/mysql_scanner/mysql_scanner.duckdb_extension';
```



## License

Copyright 2023 Kayrnt [kayrnt@gmail.com].

This project is licensed under the GNU General Public License (LICENSE-GPL).
