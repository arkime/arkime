#!/bin/sh

USER=daemon

ulimit -a
export ES_HOSTNAME=`hostname -s`a
export ES_HEAP_SIZE=_ESMEM_
export ES_NODE_MASTER=true
export ES_DATA_DIR=_TDIR_/data
export ES_LOG_DIR=_TDIR_/logs

chown -R $USER $ES_DATA_DIR $ES_LOG_DIR

exec su -s /bin/sh -c 'exec "$0" "$@" > /dev/null' $USER -- _TDIR_/elasticsearch-_ES_/bin/elasticsearch -Des.default.path.conf=_TDIR_/etc -d
