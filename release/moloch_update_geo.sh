#!/bin/bash

# Variables
DEST_DIR="${MOLOCH_DIR:-BUILD_MOLOCH_INSTALL_DIR}/etc"
TIMEOUT=${WGET_TIMEOUT:-30}

# Check we have a number for timeout
if ! [[ $TIMEOUT =~ ^[0-9]+$ ]] ; then
    echo "WGET_TIMEOUT isn't a number '$TIMEOUT'"
    exit 1;
fi

# Work on temp dir to not affect current working files
cd /tmp

# Try and download ipv4-address-space.csv, only copy if it works
wget -N -nv --timeout=${TIMEOUT} --no-check-certificate https://www.iana.org/assignments/ipv4-address-space/ipv4-address-space.csv
if (( $? == 0 )) ; then
  cp ipv4-address-space.csv "${DEST_DIR}"
fi

# Try and download manuf, only copy if it works
wget -N -nv --timeout=${TIMEOUT} https://raw.githubusercontent.com/wireshark/wireshark/master/manuf
if (( $? == 0 )) ; then
  cp manuf "${DEST_DIR}/oui.txt"
fi

# Call the maxind geoipupdate program if available. See
# https://blog.maxmind.com/2019/12/18/significant-changes-to-accessing-and-using-geolite2-databases/
# https://dev.maxmind.com/geoip/geoipupdate/#For_Free_GeoLite2_Databases
if [ -x "/usr/bin/geoipupdate" ]; then
    /usr/bin/geoipupdate
fi
