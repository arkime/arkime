#!/bin/bash
# This script fills in the values in the etc/*.template files.
# This script is auto run by easybutton-singlehost.sh

TDIR="/data/moloch"

clear

echo -n "Moloch service userid: [daemon] "
read USERNAME
if [ -z $USERNAME ]; then USERNAME="daemon"; fi

echo -n "Moloch service groupid: [daemon] "
read GROUPNAME
if [ -z $GROUPNAME ]; then GROUPNAME="daemon"; fi

echo -n "Moloch INTERNAL encryption phrase: [0mgMolochRules1] "
read PASSWORD
if [ -z $PASSWORD ]; then PASSWORD="0mgMolochRules1"; fi

echo -n "Moloch interface to listen on: [eth0] "
read INTERFACE
if [ -z $INTERFACE ]; then INTERFACE="eth0"; fi



echo 
echo "*** Create self signed SSL Certificate"
echo -n "fully qualified domain name: [moloch.test.com] "
read FQDN
if [ -z ${FQDN} ]; then FQDN="moloch.test.com"; fi

echo -n "Country: [US] "
read COUNTRY
if [ -z ${COUNTRY} ]; then COUNTRY="US"; fi

echo -n "State or Province: [Virginia] "
read STATE
if [ -z ${STATE} ]; then STATE="Virginia"; fi

echo -n "Organization Name: [MolochTester] "
read ORG_NAME
if [ -z ${ORG_NAME} ]; then ORG_NAME="MolochTester"; fi

echo -n "Organizational Unit: [MolochUnit] "
read ORG_UNIT
if [ -z ${ORG_UNIT} ]; then ORG_UNIT="MolochUnit"; fi

echo -n "Locality: [Virginia] "
read LOCALITY
if [ -z ${LOCALITY} ]; then LOCALITY="Sterling"; fi



clear

echo "You are about to attempt a Moloch build with the following configs:  (Proceed?)"
echo
echo "userid: ${USERNAME}"
echo "grpid : ${GROUPNAME}"
echo "hostname: ${FQDN}"

echo
echo "Hit Ctrl-C *now* to stop!   Hit enter to proceed"
read OK

cat ${TDIR}/etc/openssl.cnf.template | sed -e 's/_ORGANIZATION_NAME_/'${ORG_NAME}'/g' -e 's/_COMMON_NAME_/'${FQDN}'/g' -e 's/_COUNTRY_/'${COUNTRY}'/g' -e 's/_STATE_OR_PROVINCE_/'${STATE}'/g' -e 's/_LOCALITY_/'${LOCALITY}'/g' -e 's/_ORGANIZATION_UNIT_/'${ORG_UNIT}'/g'  > ${TDIR}/etc/openssl.cnf

cat ${TDIR}/etc/config.ini.template | sed -e 's/_PASSWORD_/'${PASSWORD}'/g' -e 's/_USERNAME_/'${USERNAME}'/g' -e 's/_GROUPNAME_/'${GROUPNAME}'/g' -e 's/_INTERFACE_/'${INTERFACE}'/g'  -e "s,_TDIR_,${TDIR},g" > ${TDIR}/etc/config.ini

cd ${TDIR}/etc/
openssl req -new -newkey rsa:2048 -nodes -keyout moloch.key -out moloch.csr -config ${TDIR}/etc/openssl.cnf
openssl x509 -req -days 3650 -in moloch.csr -signkey moloch.key -out moloch.crt
