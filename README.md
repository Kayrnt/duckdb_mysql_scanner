# DuckDB mysql_scanner extension

> DuckDB Labs team is working its own MySQL scanner extension, see [duckdb_mysql](https://github.com/duckdb/duckdb_mysql). I'll like freeze the development here since developing it twice isn't worth it.

The mysql_scanner extension allows DuckDB to directly read data from a running MySQL instance. The data can be queried directly from the underlying MySQL tables, or read into DuckDB tables.

## Usage

First load the extension:

```SQL
LOAD 'mysql_scanner.duckdb_extension';
```


### Attach a single table (:white_check_mark: working)

If you prefer to not attach all tables, but just query a single table, that is possible using the `MYSQL_SCAN` table-producing function, e.g.


```SQL
SELECT * FROM MYSQL_SCAN('localhost', 'root', '', 'public', 'mytable');
```

`MYSQL_SCAN` takes 5 string parameters:

- the host (ip or name)
- the user name
- the password for that user
- the schema name in MySQL
- the table name in MySQL

### Attach a single table with pushdown (:white_check_mark: working)

Same as `MYSQL_SCAN` but with pushdown.

```SQL
SELECT * FROM MYSQL_SCAN_PUSHDOWN('localhost', 'root', '', 'public', 'mytable')
WHERE id > 1000;
```

### Attach a MySQL database (:warning: :red_circle: not yet working)

To make a MYSQL database accessible to DuckDB, use the `MYSQL_ATTACH` command:

```SQL
CALL MYSQL_ATTACH('mysql://user:password@host:port/database');
```

`MYSQL_ATTACH` takes a one required string parameter:
- MySQL url string (i.e. `mysql://user:password@host:port/database`, some parameters can be omitted)

There are few additional named parameters:

- `source_schema` the name of a non-standard schema name in mysql to get tables from. Default is `public`.
- `sink_schema` the schema name in DuckDB to create views. Default is `main`.
- `overwrite` whether we should overwrite existing views in the target schema, default is `false`.
- `filter_pushdown` whether filter predicates that DuckDB derives from the query should be forwarded to MySQL, defaults to `true`.

#### `sink_schema` usage

attach MYSQL schema to another DuckDB schema.

```sql
-- create a new schema in DuckDB first
CREATE SCHEMA abc;
CALL mysql_attach('localhost', 'root', '', source_schema='information_schema', sink_schema='abc');
SELECT table_schema,table_name,table_type FROM tables;
```

The tables in the database are registered as views in DuckDB, you can list them with

```SQL
PRAGMA show_tables;
```

Then you can query those views normally using SQL.

## Building & Loading the Extension

### Build

#### Requirements

Download the C++ extension for MySQL on [MySQL dev connector C++](https://dev.mysql.com/downloads/connector/cpp/).
Extract the sources to `mysql` directory in the project root (you should have a mysql/include and lib or lib64 directories).

OpenSSL library is required to build the extension. For instance, on MacOS, you can install it with Homebrew:

```sh
brew install openssl
```

You'll need to install spdlog at the root of the project:

```sh
$git clone https://github.com/gabime/spdlog.git
$ cd spdlog && mkdir build && cd build
$ cmake .. && make -j
```

#### build commands

As release is the default target, to build:
from the project root directory, type:

```sh
make
```

if possible, use ninja for faster build:

```sh
$ GEN=ninja make
```

Add `debug` target to build debug version.

### Run

To run, run the bundled `duckdb` shell:

```sh
 ./build/release/duckdb -unsigned  # allow unsigned extensions
```

### Use

Then, load the MYSQL extension like so:

```SQL
LOAD 'build/release/extension/mysql_scanner/mysql_scanner.duckdb_extension';
```

## License

Copyright 2023 [Kayrnt](kayrnt@gmail.com).

This project is licensed under the GNU General Public License v3 (LICENSE-GPLv3).

## Acknowledgments

- Inspired by:
  - [Postgres scanner extension](https://github.com/duckdblabs/postgres_scanner)
  - [Lance duckDB](https://github.com/lancedb/lance/blob/main/integration/duckdb_lance)
  - [DuckDB extension framework](https://github.com/Mause/duckdb-extension-framework)

- Rely on [DuckDB](https://github.com/duckdb/duckdb)
