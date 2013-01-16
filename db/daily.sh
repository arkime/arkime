#!/bin/sh
# This script drops the old index and optimizes yesterdays index.
# It should be run once a day during non peak time

# CONFIG
ESHOSTPORT=CHANGEMEHOST:CHANGEMEPORT
RETAINNUMDAYS=7


YESTERDAY=`date +sessions-%y%m%d -d yesterday`
curl -XPOST "http://$ESHOSTPORT/$YESTERDAY/_optimize?max_num_segments=4"

EXPIRE=`date +sessions-%y%m%d -d "$RETAINNUMDAYS days ago"`
curl -XDELETE "http://$ESHOSTPORT/$EXPIRE/"
