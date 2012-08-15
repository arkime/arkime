#!/bin/sh
# This initializes the elasticsearch database.  Make sure you edit sessions.json first

if [ $# == 0 ] ; then
    echo "$0 <elasticsearch host>"
    exit 0;
fi

ESHOST=$1


echo "Deleting"
echo curl -XDELETE http://$ESHOST:9200/tags_v1
curl -XDELETE http://$ESHOST:9200/tags_v1
echo

echo curl -XDELETE http://$ESHOST:9200/sequence
curl -XDELETE http://$ESHOST:9200/sequence
echo

echo curl -XDELETE http://$ESHOST:9200/files_v1
curl -XDELETE http://$ESHOST:9200/files_v1
echo

echo curl -XDELETE http://$ESHOST:9200/stats
curl -XDELETE http://$ESHOST:9200/stats
echo

echo curl -XDELETE http://$ESHOST:9200/dstats
curl -XDELETE http://$ESHOST:9200/dstats
echo

echo curl -XDELETE "http://$ESHOST:9200/sessions"
curl -XDELETE "http://$ESHOST:9200/sessions"
echo

sleep 1
echo "Create Sessions"
echo curl -XPUT http://$ESHOST:9200/_template/template_1 --data @sessions.json
curl -XPUT http://$ESHOST:9200/_template/template_1 --data @sessions.json

echo "Create Stats"
echo curl -XPUT http://$ESHOST:9200/stats/?pretty=1 --data @stats.json
curl -XPUT http://$ESHOST:9200/stats --data @stats.json
echo 

echo "Create Detail Stats"
echo curl -XPUT http://$ESHOST:9200/dstats/?pretty=1 --data @dstats.json
curl -XPUT http://$ESHOST:9200/dstats --data @dstats.json
echo 

echo "Create Files"
curl -XPUT "http://$ESHOST:9200/files_v1/?pretty=1" -d '
{
  "mappings": {
    "file": {
      "_all" : {"enabled" : false},
      "_source" : {"enabled" : true},
      "properties": {
        "num": {
          "type": "long",
          "index": "not_analyzed"
        },
        "node": {
          "type": "string",
          "index": "not_analyzed"
        },
        "first": {
          "type": "long",
          "index": "not_analyzed"
        },
        "name": {
          "type": "string",
          "index": "no"
        },
        "size": {
          "type": "long",
          "index": "no"
        },
        "last": {
          "type": "long",
          "index": "not_analyzed"
        }
      }
    }
  }
}
'

curl -XPOST "http://$ESHOST:9200/_aliases" -d '
{
    "actions" : [
        { "add" : { "index" : "files_v1", "alias" : "files" } }
    ]
}'
echo 

echo "Create Users"
curl -XPUT "http://$ESHOST:9200/users_v1/?pretty=1" -d '
{
  "mappings": {
    "user": {
      "_all" : {"enabled" : false},
      "_source" : {"enabled" : true},
      "properties": {
        "userId": {
          "type": "string",
          "index": "not_analyzed"
        },
        "userName": {
          "type": "string"
        },
        "enabled": {
          "type": "boolean"
        },
        "createEnabled": {
          "type": "boolean"
        },
        "webEnabled": {
          "type": "boolean"
        },
        "passHash": {
          "type": "string",
          "index": "no"
        },
        "expression": {
          "type": "string",
          "index": "no"
        }
      }
    }
  }
}
'
curl -XPOST "http://$ESHOST:9200/_aliases" -d '
{
    "actions" : [
        { "add" : { "index" : "users_v1", "alias" : "users" } }
    ]
}'
echo 

echo "Create Sequence"
curl -XPUT "http://$ESHOST:9200/sequence/?pretty=1"  -d '
{
   "settings" : {
      "number_of_shards"     : 1,           
      "auto_expand_replicas" : "0-all"  
   },
   "mappings" : {
      "sequence" : {
         "_source" : { "enabled" : 0 },
         "_all"    : { "enabled" : 0 },
         "_type"   : { "index" : "no" },
         "enabled" : 0
      }
   }
}
'

echo 'Create tags'
curl -XPUT "http://$ESHOST:9200/tags_v1/?pretty=1"  -d '
{
  "settings" : {
    "number_of_shards"     : 1,           
    "auto_expand_replicas" : "0-all"  
  },
  "mappings" : {
    "tag" : {
      "_type"   : { "index" : "no" },
      "_id"     : { "index": "not_analyzed"},
      "enabled" : 0,
      "properties": {
        "n": {
          "type": "integer"
        }
      }
    }
  }
}'

curl -XPOST "http://$ESHOST:9200/_aliases" -d '
{
    "actions" : [
        { "add" : { "index" : "tags_v1", "alias" : "tags" } }
    ]
}'
