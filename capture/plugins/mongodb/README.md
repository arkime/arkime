# mongodb

Lets MongoDB hold everything Arkime keeps in a database: the sessions, the users db, cont3xt's db,
parliament, the notifiers and the cluster db. The cluster db (`dbUrl`) is the stats, dstats, files,
fields, sequence, hunts, queries and history collections, and featherprint's devices, history and
alerts (featherprint, featherprint_history and featherprint_alerts).

It has two halves:

* `mongodb.js`: a `dbPlugins` module for the viewer, cont3xt, parliament, wise and the tools
* `mongodb.so`: a capture `rootPlugins` writer for the sessions and the collections capture fills

Only the users db or cont3xt's db only needs `mongodb.js`.

MongoDB 9.0 or newer is required.

## Setup

```ini
[default]
sessionsDbUrl=mongodb://arkime:password@db.example.com/arkime
dbUrl=mongodb://arkime:password@db.example.com/arkime
usersDbUrl=mongodb://arkime:password@db.example.com/arkime
dbPlugins=mongodb/mongodb.js
rootPlugins=mongodb.so
```

Any of the three can point somewhere else, such as sessions in MongoDB and the cluster db in
OpenSearch/Elasticsearch. Each app creates the collections and indexes it uses the first time it starts.

The url is the standard MongoDB connection string, `mongodb://user:password@host[:port][,host2...]/database[?options]`
or `mongodb+srv://`, and it must name a database. Without `tls` settings in the url a remote server
must use TLS and its certificate is verified with `caTrustFile` or the system CAs, like the
OpenSearch/Elasticsearch connections. `--insecure` still requires TLS but skips verification.
A url with one loopback or unix socket server is left alone and connects to only that server, set
`tls=false` in the url for a local replica set or a local member of one.

| Setting | Default | |
| --- | --- | --- |
| `mongodbPatternTimeout` | 60 | Seconds a `$regex` or `$wildcard` search from the UI can run, only two run at once for each connection |
| `mongodbSessionsTimeout` | 300 | Seconds a sessions search or aggregation can run |

## Sessions

Capture writes each session into the collection named after the index OpenSearch/Elasticsearch
would use, `<prefix>sessions3-<period>`, with the session's fields at the top level. Each session
collection has a wildcard index over every field plus `firstPacket` and `lastPacket` indexes, and
`_ip` lists every ip in the session with its field and bytes so ip, cidr and partial ip searches
use an index. An ipv4 is stored as `::ffff:a.b.c.d`, so ipv4 and ipv6 compare in the order ES uses.

The viewer's queries are compiled into MongoDB filters and a search over several periods is one
`$unionWith` pipeline. Expressions, aggregations (terms, histograms, filters and the sum, min, max,
avg, cardinality, value_count and percentiles metrics), tagging, hunts, scrolls and deleting work like
OpenSearch/Elasticsearch. The viewer copies the shortcuts into a `<prefix>lookups` collection for the
searches that use them. Nothing expires sessions yet, drop the old period collections to.

## Collections

Each other collection holds `{ _id, d: the doc, v: its version }` with an `arkime_meta` collection
per prefix. Fields a collection marks indexed get an index on `d.<field>`, and a sorted one also gets
one with `_n.<field>` first so an ascending sort puts missing values last off the index. stats and
dstats only take their fields and store each as its type.

`$regex` runs in MongoDB's PCRE engine and refuses the syntax PCRE and RE2 read differently, like
`(?i)` and named groups. A `$wildcard` is compiled to atomic groups so it runs in linear time.

## Building

The plugin isn't part of the default build, after `./configure` build it from the top of the tree:

```bash
make -C capture/plugins/mongodb
(cd capture/plugins/mongodb && npm ci)
```

The build needs the MongoDB C driver 2.x, `pkg-config mongoc2`.

## Testing

```bash
ARKIME_TEST_DB_URL=mongodb://127.0.0.1/arkimetest node --test capture/plugins/mongodb/mongodb.unit.mjs
ARKIME_TEST_DB_PLUGIN=capture/plugins/mongodb/mongodb.js ARKIME_TEST_DB_URL=mongodb://127.0.0.1/arkimetest npm run test:unit
cd tests && ARKIME_default__dbUrl=mongodb://127.0.0.1/arkimecap ARKIME_default__dbPlugins=mongodb/mongodb.js ARKIME_default__rootPlugins=mongodb.so ./tests.pl --viewer
```

For MongoDB sessions also set `ARKIME_test__sessionsDbUrl`, `ARKIME_testuser__sessionsDbUrl`,
the `sessionsDbUrl` of the first `ARKIME_all__multiESNodes` node, and
`ARKIME_TEST_SESSIONS_HELPER=../capture/plugins/mongodb/testSessions.mjs`, which the tests use to
wipe, post and delete sessions.
