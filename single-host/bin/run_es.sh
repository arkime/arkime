#!/bin/sh

TDIR=_TDIR_

cd ${TDIR}/elasticsearch-_ES_
ulimit -a
# Uncomment if using Sun Java for better memory utilization
# export JAVA_OPTS="-XX:+UseCompressedOops"
export ES_HOSTNAME=`hostname -s`a

# Increase memory
ES_HEAP_SIZE=_ESMEM_ bin/elasticsearch -Des.config=${TDIR}/etc/elasticsearch.yml -d
