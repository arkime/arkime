#!/bin/bash

FILE=$1

rm -f $FILE
aws s3 cp --quiet $S3BASE/$FILE $FILE 

if [ -f $FILE ] ; then
    NUM=`cat $FILE`
else
    NUM=0
fi

echo $((++NUM))
exit 0
