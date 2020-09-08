#!/bin/bash

# Variables
DEST_DIR="${MOLOCH_DIR:-BUILD_MOLOCH_INSTALL_DIR}/etc"
TIMEOUT="${WGET_TIMEOUT:-30}"

# Use an appropriately permissions place to download files
mkdir -p /opt


# Remove /opt/ipv4-address-space.csv if it exists, and then download the latest
if [ -e /opt/ipv4-address-space.csv ]; then
    rm -rf /opt/ipv4-address-space.csv
fi

wget -nv --timeout="${TIMEOUT}" -O /opt/ipv4-address-space.csv --no-check-certificate "https://www.iana.org/assignments/ipv4-address-space/ipv4-address-space.csv" \
    && mv /opt/ipv4-address-space.csv "${DEST_DIR}/ipv4-address-space.csv"

# Remove /opt/oui.txt if it exists, and then download the latest
if [ -e /opt/oui.txt ]; then
    rm -rf /opt/oui.txt
fi

wget -nv --timeout="${TIMEOUT}" -O /opt/oui.txt --no-check-certificate "https://raw.githubusercontent.com/wireshark/wireshark/master/manuf" \
    && mv /opt/oui.txt "${DEST_DIR}/oui.txt"

# Call the maxind geoipupdate program if available. See
# https://blog.maxmind.com/2019/12/18/significant-changes-to-accessing-and-using-geolite2-databases/
# https://dev.maxmind.com/geoip/geoipupdate/#For_Free_GeoLite2_Databases
if [ -x "/usr/bin/geoipupdate" ]; then
    /usr/bin/geoipupdate
fi
