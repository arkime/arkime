# mysqldb

Lets MySQL hold the Arkime users db, cont3xt's db, parliament, the notifiers and the cluster db.
The cluster db (`dbUrl`) is the stats, dstats, files, fields, sequence, hunts, queries and history
tables, and featherprint's devices, history and alerts (featherprint, featherprint_history and
featherprint_alerts). Sessions stay in OpenSearch/Elasticsearch or ClickHouse through `sessionsDbUrl`.

It has two halves, both needed for a cluster db:

* `mysqldb.js`: a `dbPlugins` module for the viewer, cont3xt, parliament, wise and the tools
* `mysqldb.so`: a capture `rootPlugins` writer for the tables capture fills

Only the users db or cont3xt's db only needs `mysqldb.js`.

MySQL 26.10 or newer is required.

## Setup

```ini
[default]
dbUrl=mysql://arkime:password@db.example.com/arkime
sessionsDbUrl=https://es.example.com:9200
usersDbUrl=mysql://arkime:password@db.example.com/arkime
dbPlugins=mysqldb/mysqldb.js
rootPlugins=mysqldb.so
```

Create the database and a user that can create tables in it. Every app creates the tables it
uses the first time it starts and adds columns and indexes newer versions need.

The url is `mysql://user:password@host[:port]/database` with optional query settings:

| Setting | |
| --- | --- |
| `socket=/path` | Connect through a unix socket instead of host and port |
| `ssl-mode=` | `DISABLED`, `REQUIRED`, `VERIFY_CA` or `VERIFY_IDENTITY` |
| `ssl-ca=`, `ssl-cert=`, `ssl-key=` | PEM files for the server CA and a client certificate |
| `connectionLimit=10` | Pool size for each node app |

Without ssl settings a remote server must use TLS and its certificate is verified with
`caTrustFile` or the system CAs, like the OpenSearch/Elasticsearch connections. `--insecure`
still requires TLS but skips verification. Loopback and unix socket servers are left alone.

`mysqlPatternTimeout` (seconds, default 60) bounds a `$regex` or `$wildcard` search from the UI,
and only two of them run at once for each pool.

## Tables

Each collection is a table of `(id VARBINARY(3072), doc JSON, version INT)` with an
`arkime_meta` table per prefix. stats and dstats keep each field in its own column. Fields a
collection marks indexed are virtual generated columns of their type with an index: keywords
`VARCHAR(768)`, numbers `DOUBLE`, ips `VARBINARY(16)` and booleans, and a sorted one also gets an
index for ascending sorts with missing values last. The doc stays the source of truth, so a keyword
longer than 768 characters or a number past a double still matches exactly.

Strings compare by their bytes with `utf8mb4_0900_bin`, the same order OpenSearch/Elasticsearch and
the other backends use. `$regex` runs in MySQL's ICU engine and refuses the syntax ICU and RE2
read differently, like `(?i)` and nested classes.

## Building

The plugin isn't in configure.ac yet, so generate its Makefile from the top of the tree:

```bash
./config.status --file=capture/plugins/mysqldb/Makefile
make -C capture/plugins/mysqldb
(cd capture/plugins/mysqldb && npm ci)
```

The build needs the MySQL client library, `pkg-config mysqlclient` or `mysql_config`.

## Testing

```bash
ARKIME_TEST_DB_URL=mysql://arkime:arkime@127.0.0.1/arkimetest node --test capture/plugins/mysqldb/mysqldb.unit.mjs
ARKIME_TEST_DB_PLUGIN=capture/plugins/mysqldb/mysqldb.js ARKIME_TEST_DB_URL=mysql://arkime:arkime@127.0.0.1/arkimetest npm run test:unit
cd tests && ARKIME_default__dbUrl=mysql://arkime:arkime@127.0.0.1/arkimecap ARKIME_default__dbPlugins=mysqldb/mysqldb.js ARKIME_default__rootPlugins=mysqldb.so ./tests.pl --viewer
```
