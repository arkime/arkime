#!/bin/sh
# Use this script to create a single instance host for Moloch for testing, 
# you probably don't want to use this for a real deployment.  This script 
# uses easybutton-build.sh to build everything and easybutton-config.sh
# to configure moloch using the sample config files from single-host directory.
# At the end you'll have a full deployment in /data/moloch.


# This script will 
# * Use easybutton-build.sh to
# ** download known working versions of moloch dependancies, 
# ** build them statically 
# ** configure moloch-capture to use them
# ** build moloch-capture
# * build nodejs
# * build moloch-viewer
# * build ElasticSearch



ES=0.19.9
NODEJS=0.8.8
# If you change TDIR you need to change TDIR in the single-host/*/* files
TDIR=/data/moloch
INSTALL_DIR=$PWD

if [ "$(id -u)" != "0" ]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

which java
JAVA_VAL=$?

if [ $JAVA_VAL -ne 0 ]; then
 echo
 echo "*** Please install Oracle Java before proceeding, OpenJDK doesn't seem to work.  Visit http://java.sun.com"
 echo
 exit 0
fi

umask 022


# Building thirdparty libraries and moloch
./easybutton-build.sh

# Increase limits
grep -q "hard.*nofile.*128000" /etc/security/limits.conf
LIMIT_VAL=$?
if [ $LIMIT_VAL -ne 0 ]; then
  echo "MOLOCH: Adding entries to /etc/security/limits.conf"
  echo "* hard nofile 128000" >> /etc/security/limits.conf
  echo "* soft nofile 128000" >> /etc/security/limits.conf
fi


# Install area
echo "MOLOCH: Creating install area"
mkdir -p ${TDIR}/data
mkdir -p ${TDIR}/logs
mkdir -p ${TDIR}/raw
mkdir -p ${TDIR}/etc
mkdir -p ${TDIR}/bin



# ElasticSearch
echo "MOLOCH: Downloading and installing elastic search"
cd ${INSTALL_DIR}/thirdparty
if [ ! -f "elasticsearch-${ES}.tar.gz" ]; then
  wget https://github.com/downloads/elasticsearch/elasticsearch/elasticsearch-${ES}.tar.gz
fi

cd ${TDIR}
tar xvfz ${INSTALL_DIR}/thirdparty/elasticsearch-${ES}.tar.gz
cd elasticsearch-${ES}
./bin/plugin -install mobz/elasticsearch-head
./bin/plugin -install lukas-vlcek/bigdesk


# NodeJS
echo "MOLOCH: Downloading and installing node"
cd ${INSTALL_DIR}/thirdparty
if [ ! -f "node-v${NODEJS}.tar.gz" ]; then
  wget http://nodejs.org/dist/v${NODEJS}/node-v${NODEJS}.tar.gz
fi

tar xvfz node-v${NODEJS}.tar.gz
cd node-v${NODEJS}
./configure 
make
make install
./configure --prefix=${TDIR}
make install


cd ${TDIR}/etc/
if [ ! -f "GeoIP.dat" ]; then
  wget http://geolite.maxmind.com/download/geoip/database/GeoLiteCountry/GeoIP.dat.gz
  gunzip GeoIP.dat.gz
fi

wget http://www.maxmind.com/download/geoip/database/asnum/GeoIPASNum.dat.gz
gunzip GeoIPASNum.dat.gz



echo "MOLOCH: Copying single-host config files"
cp ${INSTALL_DIR}/capture/moloch-capture ${TDIR}/bin/
cp ${INSTALL_DIR}/single-host/etc/* ${TDIR}/etc
cat ${INSTALL_DIR}/single-host/etc/elasticsearch.yml | sed -e "s,_TDIR_,${TDIR},g" > ${TDIR}/etc/elasticsearch.yml

cat ${INSTALL_DIR}/single-host/bin/run_es.sh | sed -e "s,_TDIR_,${TDIR},g" -e "s/_ES_/${ES}/g" > ${TDIR}/bin/run_es.sh
cat ${INSTALL_DIR}/single-host/bin/run_capture.sh | sed -e "s,_TDIR_,${TDIR},g" > ${TDIR}/bin/run_capture.sh
cat ${INSTALL_DIR}/single-host/bin/run_viewer.sh | sed -e "s,_TDIR_,${TDIR},g" > ${TDIR}/bin/run_viewer.sh
chmod 755 ${TDIR}/bin/run*.sh


cp -Rp ${INSTALL_DIR}/viewer ${TDIR}/
cp -Rp ${INSTALL_DIR}/db ${TDIR}/


echo "MOLOCH: Running npm install"
cd ${TDIR}/viewer
${TDIR}/bin/npm install
chmod a+w public

chown daemon:daemon ${TDIR}/raw

echo "MOLOCH: Running config script"


${INSTALL_DIR}/easybutton-config.sh


echo "MOLOCH: Starting ElasticSearch"

${TDIR}/bin/run_es.sh

sleep 10

echo "MOLOCH: Building database"
cat ${INSTALL_DIR}/db/sessions.json | sed -e 's/_CHANGE_ME_TO_NUMBER_OF_NODES_/1/g'  > ${TDIR}/db/sessions.json
cd ${TDIR}/db
./init.sh localhost

sleep 1


echo "MOLOCH: Adding user admin/admin"
cd ${TDIR}/viewer
../bin/node addUser.js -c ../etc/config.ini admin "Admin" admin -admin

echo "MOLOCH: Starting viewer and capture"
cd ${TDIR}/bin
nohup ./run_viewer.sh &
nohup ./run_capture.sh &


HOSTNAME=`hostname`
echo "MOLOCH: Complete use https://$HOSTNAME:8005 to access.  You should also make the run_* scripts in ${TDIR}/bin run on start up and look at the config files in ${TDIR}/etc"
