#!/bin/sh
# This updates the elasticsearch sessions template.
# It does NOT change current sessions indexes, only new ones.

if [ $# == 0 ] ; then
    echo "$0 <elasticsearch host>"
    exit 0;
fi

ESHOST=$1


echo "Updating Sessions Template"
echo curl -XPUT http://$ESHOST:9200/_template/template_1 --data @sessions.json
curl -XPUT http://$ESHOST:9200/_template/template_1 --data @sessions.json
