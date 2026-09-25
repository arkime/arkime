#!/bin/sh
# Run the test suite with ClickHouse as the sessions store. config.test.ini
# stays pointed at Elasticsearch; these ARKIME_ environment overrides (honored
# by capture, viewer, and the test framework) switch sessions to ClickHouse.
# Usage: ./tests_ch.sh [tests.pl args], e.g. ./tests_ch.sh --viewer

cd "$(dirname "$0")" || exit 1

export ARKIME_test__sessionsDbUrl="clickhouse://localhost:9100"
export ARKIME_testuser__sessionsDbUrl="clickhouse://localhost:9100"
export ARKIME_all__multiESNodes="127.0.0.1:9200,prefix:tests,name:test,sessionsDbUrl:clickhouse://localhost:9100;127.0.0.1:9200,prefix:tests2_,name:test2"

exec ./tests.pl "$@"
