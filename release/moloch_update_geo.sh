#!/bin/bash

# Variables
DEST_DIR="${MOLOCH_DIR:-/data/moloch}/etc"
TIMEOUT=${WGET_TIMEOUT:-30}

# Work on temp dir to not affect current working files
cd /tmp

wget -N -nv --timeout=${TIMEOUT} --no-check-certificate https://www.iana.org/assignments/ipv4-address-space/ipv4-address-space.csv
if (( $? == 0 ))
then
  cp ipv4-address-space.csv "${DEST_DIR}"
fi

wget -N -nv --timeout=${TIMEOUT} -O GeoLite2-Country.mmdb.gz "https://download.maxmind.com/app/geoip_download?edition_id=GeoLite2-Country&license_key=$1&suffix=tar.gz"
if (( $? == 0 ))
then
  /bin/rm -f "${DEST_DIR}/GeoLite2-Country.mmdb"
  tar xvzf GeoLite2-Country.mmdb.gz
  mv GeoLite2-Country*/GeoLite2-Country.mmdb ${DEST_DIR}/
  rm -rf GeoLite2-Country*
fi


wget -N -nv --timeout=${TIMEOUT} -O GeoLite2-ASN.mmdb.gz "https://download.maxmind.com/app/geoip_download?edition_id=GeoLite2-ASN&license_key=$1&suffix=tar.gz"
if (( $? == 0 ))
then
  /bin/rm -f "${DEST_DIR}/GeoLite2-ASN.mmdb"
  tar xvzf GeoLite2-ASN.mmdb.gz
  mv GeoLite2-ASN*/GeoLite2-ASN.mmdb ${DEST_DIR}/
  rm -rf GeoLite2-ASN*
fi


wget -nv --timeout=${TIMEOUT} -O oui.txt https://raw.githubusercontent.com/wireshark/wireshark/master/manuf
if (( $? == 0 ))
then
  cp oui.txt "${DEST_DIR}/oui.txt"
fi
