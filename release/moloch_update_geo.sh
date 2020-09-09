#!/bin/bash

# Variables
DEST_DIR="${MOLOCH_DIR:-BUILD_MOLOCH_INSTALL_DIR}/etc"
TIMEOUT="${WGET_TIMEOUT:-30}"

# Work on temp dir to not affect current working files
cd /tmp

mkdir -p "${DEST_DIR}"

wget -N -nv --timeout="${TIMEOUT}" -O "${DEST_DIR}/ipv4-address-space.csv" --no-check-certificate https://www.iana.org/assignments/ipv4-address-space/ipv4-address-space.csv
if ! [ $? = 0 ]; then
  echo "Failed to download ipv4-address-space.csv"
fi

wget -nv --timeout="${TIMEOUT}" -O "${DEST_DIR}/oui.txt" https://raw.githubusercontent.com/wireshark/wireshark/master/manuf
if ! [ $? = 0 ]; then
  echo "Failed to download oui.txt"
fi



# Call the maxind geoipupdate program if available. See
# https://blog.maxmind.com/2019/12/18/significant-changes-to-accessing-and-using-geolite2-databases/
# https://dev.maxmind.com/geoip/geoipupdate/#For_Free_GeoLite2_Databases
if [ -x "/usr/bin/geoipupdate" ]; then
    /usr/bin/geoipupdate
fi
