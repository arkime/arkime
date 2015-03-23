#!/bin/bash

export TDIR="/data/moloch"
ln -s $TDIR/bin/node $TDIR/bin/node-wise

cp -Rp capture/plugins/wiseService $TDIR/
cp -p capture/plugins/wiseService/wiseService.ini.sample $TDIR/etc/wiseService.ini

echo "Make sure to uncomment plugins= and viewerPlugins= in your config.ini"
echo "and append wise.so to plugins="

cd $TDIR/wiseService/
npm install

cd -
