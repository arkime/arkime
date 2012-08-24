#!/bin/sh

TDIR=_TDIR_

cd ${TDIR}/elasticsearch-_ES_
ulimit -a
export JAVA_OPTS="-XX:+UseCompressedOops"
export ES_HOSTNAME=`hostname -s`a
ES_HEAP_SIZE=1G bin/elasticsearch -Des.config=${TDIR}/etc/elasticsearch.yml
