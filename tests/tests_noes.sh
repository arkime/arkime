#!/bin/sh
# Copyright Andy Wick
# SPDX-License-Identifier: Apache-2.0
#
# Run the test suite with no OpenSearch/Elasticsearch at all: sessions in ClickHouse, the cluster db,
# users, cont3xt and parliament in Postgres. Nothing may answer on 127.0.0.1:9200.
# Usage: CH=clickhouse://localhost:9100 PG=postgres://arkime@localhost/arkime ./tests_noes.sh --viewer
# PG must be an empty database, the run doesn't wipe it.

cd "$(dirname "$0")" || exit 1

CH=${CH:-clickhouse://localhost:9100}
PG=${PG:-postgres://arkime@localhost/arkime}

export ARKIME_TEST_NOES=1
export ARKIME_default__elasticsearch=
export ARKIME_default__sessionsDbUrl="$CH"
export ARKIME_test__sessionsDbUrl="$CH"
export ARKIME_testuser__sessionsDbUrl="$CH"
export ARKIME_default__dbUrl="$PG"
export ARKIME_cont3xt__dbUrl="$PG"
export USERSELASTICSEARCH="$PG"
# The multi viewer reads everything through multies, which sends each cluster to these
export ARKIME_all__sessionsDbUrl=http://127.0.0.1:8200
export ARKIME_all__dbUrl=http://127.0.0.1:8200
for section in test2 test3 test4 test5 all testmulties; do
  export "ARKIME_${section}__usersElasticsearch=$PG"
done

exec ./tests.pl "$@"
