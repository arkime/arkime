#!/bin/sh
# This script fills in the values in the etc/*.template files.
# This script is auto run by easybutton-singlehost.sh

TDIR="/data/moloch"
if [ "$#" -gt 0 ]; then
    TDIR="$1"
fi

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

echo "You are about to attempt a Moloch build (Proceed?)"
echo
echo "Hit Ctrl-C *now* to stop!   Hit enter to proceed"
read OK

cat ${TDIR}/etc/config.ini.template | sed -e 's/_PASSWORD_/'${PASSWORD}'/g' -e 's/_USERNAME_/'${USERNAME}'/g' -e 's/_GROUPNAME_/'${GROUPNAME}'/g' -e 's/_INTERFACE_/'${INTERFACE}'/g'  -e "s,_TDIR_,${TDIR},g" > ${TDIR}/etc/config.ini

cd ${TDIR}/etc/
echo "MOLOCH: creating self signed certificate"

# Start Certificate
cd ${TDIR}/etc/
openssl req -new -newkey rsa:2048 -nodes -keyout moloch.key -out moloch.csr << EOF 2>/dev/null
US
California
Anytown
Moloch
Moloch
moloch.test.com



EOF
openssl x509 -in moloch.csr -out moloch.crt -req -signkey moloch.key -days 3650 2>/dev/null
## End Certificate creation
