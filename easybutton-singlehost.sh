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

which java
JAVA_VAL=$?

if [ $JAVA_VAL -ne 0 ]; then
 echo
 echo "*** Please install Java before proceeding.  Visit http://java.sun.com"
 echo
 exit 0
fi


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
wget http://geolite.maxmind.com/download/geoip/database/GeoLiteCountry/GeoIP.dat.gz
gunzip GeoIP.dat.gz



echo "MOLOCH: Copying single-host config files"
cp ${INSTALL_DIR}/capture/moloch-capture ${TDIR}/bin/
cp ${INSTALL_DIR}/single-host/etc/* ${TDIR}/etc
cp ${INSTALL_DIR}/single-host/bin/* ${TDIR}/bin
chmod 755 ${TDIR}/bin/run*.sh
cp -Rp ${INSTALL_DIR}/viewer ${TDIR}/



#cd ${TDIR}/etc/
#openssl req -new -newkey rsa:2048 -nodes -keyout moloch.key -out moloch.csr -config ${TDIR}/etc/openssl.cnf
#openssl x509 -req -days 3650 -in moloch.csr -signkey moloch.key -out moloch.crt


echo "MOLOCH: Running npm install"
cd ${TDIR}/viewer
${TDIR}/bin/npm install
chmod a+w public

chown daemon:daemon ${TDIR}/raw

echo "MOLOCH: Running config script"


${INSTALL_DIR}/easybutton-config.sh

echo "MOLOCH: Complete, now look in ${TDIR} and use the run scripts in ${TDIR}/bin"

exit

#${TDIR}/bin/node addUser.js -c ${TDIR}/etc/config.ini admin "Admin" admin -admin

