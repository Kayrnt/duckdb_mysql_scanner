# DuckDB mysql_scanner extension

> The project is in progress and not yet on par with postgres scanner or other integrations. There is no releases yet so you have to build it yourself.

The mysql_scanner extension allows DuckDB to directly read data from a running MySQL instance. The data can be queried directly from the underlying MySQL tables, or read into DuckDB tables.

## Usage

### Attach a single table (:white_check_mark: working)

If you prefer to not attach all tables, but just query a single table, that is possible using the `MYSQL_SCAN` table-producing function, e.g.

```SQL
SELECT * FROM MYSQL_SCAN('', 'public', 'mytable');
```

`MYSQL_SCAN` takes 5 string parameters:

- the host (ip or name)
- the user name
- the password for that user
- the schema name in MySQL
- the table name in MySQL

#### `sink_schema` usage

attach MYSQL schema to another DuckDB schema.

```sql
-- create a new schema in DuckDB first
CREATE SCHEMA abc;
CALL mysql_attach('localhost', 'root', '', source_schema='information_schema', sink_schema='abc');
SELECT table_schema,table_name,table_type FROM tables;
```

### Attach a MySQL database (:warning: :red_circle: not yet working)

First load the extension:

```SQL
LOAD 'mysql_scanner.duckdb_extension';
```

To make a MYSQL database accessible to DuckDB, use the `MYSQL_ATTACH` command:

```SQL
CALL MYSQL_ATTACH('127.0.0.1', 'username', 'password');
```

`MYSQL_ATTACH` takes a three required string parameters:

- the host (ip or name)
- the user name
- the password for that user

There are few additional named parameters:

- `source_schema` the name of a non-standard schema name in Postgres to get tables from. Default is `public`.
- `sink_schema` the schema name in DuckDB to create views. Default is `main`.
- `overwrite` whether we should overwrite existing views in the target schema, default is `false`.
- `filter_pushdown` whether filter predicates that DuckDB derives from the query should be forwarded to MySQL, defaults to `true`.

The tables in the database are registered as views in DuckDB, you can list them with

```SQL
PRAGMA show_tables;
```

Then you can query those views normally using SQL.

## Building & Loading the Extension

### Build

Download the C++ extension for MySQL on [MySQL dev connector C++](https://dev.mysql.com/downloads/connector/cpp/).
Extract the sources to `mysql` directory in the project root (you should have a mysql/include and lib or lib64 directories).

OpenSSL library is required to build the extension. For instance, on MacOS, you can install it with Homebrew:

```sh
brew install openssl
```

To build, from the project root directory, type:

```sh
make
```

Add `debug` to build debug version.

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

This project is licensed under the GNU General Public License (LICENSE-GPL).

## Acknowledgments

- Inspired by:
  - [Postgres scanner extension](https://github.com/duckdblabs/postgres_scanner)
  - [Lance duckDB](https://github.com/lancedb/lance/blob/main/integration/duckdb_lance)
  - [DuckDB extension framework](https://github.com/Mause/duckdb-extension-framework)

- Rely on [DuckDB](https://github.com/duckdb/duckdb)
