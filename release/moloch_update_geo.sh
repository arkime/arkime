#!/bin/bash

# Variables
DEST_DIR="${MOLOCH_DIR:-BUILD_MOLOCH_INSTALL_DIR}/etc"
TIMEOUT=${WGET_TIMEOUT:-30}

# Work on temp dir to not affect current working files
cd /tmp

wget -N -nv --timeout=${TIMEOUT} --no-check-certificate https://www.iana.org/assignments/ipv4-address-space/ipv4-address-space.csv
if (( $? == 0 ))
then
  cp ipv4-address-space.csv "${DEST_DIR}"
fi

wget -N -nv --timeout=${TIMEOUT} -O GeoLite2-Country.mmdb.gz 'https://updates.maxmind.com/app/update_secure?edition_id=GeoLite2-Country'
if (( $? == 0 ))
then
  /bin/rm -f "${DEST_DIR}/GeoLite2-Country.mmdb"
  zcat GeoLite2-Country.mmdb.gz > "${DEST_DIR}/GeoLite2-Country.mmdb"
fi


wget -N -nv --timeout=${TIMEOUT} -O GeoLite2-ASN.mmdb.gz 'https://updates.maxmind.com/app/update_secure?edition_id=GeoLite2-ASN'
if (( $? == 0 ))
then
  /bin/rm -f "${DEST_DIR}/GeoLite2-ASN.mmdb"
  zcat GeoLite2-ASN.mmdb.gz > "${DEST_DIR}/GeoLite2-ASN.mmdb"
fi


wget -nv --timeout=${TIMEOUT} -O oui.txt https://raw.githubusercontent.com/wireshark/wireshark/master/manuf
if (( $? == 0 ))
then
  cp oui.txt "${DEST_DIR}/oui.txt"
fi


