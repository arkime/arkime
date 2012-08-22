#!/bin/sh

TDIR=/data/moloch

cd ${TDIR}/elasticsearch-0.19.8
ulimit -a
export JAVA_OPTS="-XX:+UseCompressedOops"
export ES_HOSTNAME=`hostname -s`a
ES_HEAP_SIZE=1G bin/elasticsearch -Des.config=${TDIR}/etc/elasticsearch.yml
#sleep 2
#export ES_HOSTNAME=`hostname -s`b
#ES_HEAP_SIZE=20G bin/elasticsearch -Des.config=${TDIR}/elasticsearch.yml
