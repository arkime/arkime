#!/bin/bash

# Variables
DEST_DIR="${MOLOCH_DIR:-BUILD_MOLOCH_INSTALL_DIR}/etc"
TIMEOUT="${WGET_TIMEOUT:-30}"

cd "${DEST_DIR}"

# Remove ipv4-address-space.csv.tmp if it exists from a failed download, and then download the latest
if [ -e ipv4-address-space.csv.tmp ]; then
    rm -rf ipv4-address-space.csv.tmp
fi

wget -nv --timeout="${TIMEOUT}" -O ipv4-address-space.csv.tmp --no-check-certificate "https://www.iana.org/assignments/ipv4-address-space/ipv4-address-space.csv" \
    && mv -f ipv4-address-space.csv.tmp ipv4-address-space.csv

# Remove oui.txt.tmp if it exists from a failed download, and then download the latest
if [ -e oui.txt.tmp ]; then
    rm -rf oui.txt.tmp
fi

wget -nv --timeout="${TIMEOUT}" -O oui.txt.tmp --no-check-certificate "https://raw.githubusercontent.com/wireshark/wireshark/master/manuf" \
    && mv -f oui.txt.tmp oui.txt

# Call the maxind geoipupdate program if available. See
# https://blog.maxmind.com/2019/12/18/significant-changes-to-accessing-and-using-geolite2-databases/
# https://dev.maxmind.com/geoip/geoipupdate/#For_Free_GeoLite2_Databases
if [ -x "/usr/bin/geoipupdate" ]; then
    /usr/bin/geoipupdate
fi
