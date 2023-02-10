#!/bin/bash

# Variables
DEST_DIR="${MOLOCH_DIR:-BUILD_ARKIME_INSTALL_DIR}/etc"
TIMEOUT=${WGET_TIMEOUT:-30}

# Check we have a number for timeout
if ! [[ $TIMEOUT =~ ^[0-9]+$ ]] ; then
    echo "WGET_TIMEOUT isn't a number '$TIMEOUT'"
    exit 1;
fi

# Try and download ipv4-address-space.csv, only copy if it works
FILENAME=$(mktemp)
wget -nv --timeout=${TIMEOUT} --no-check-certificate -O "$FILENAME" https://www.iana.org/assignments/ipv4-address-space/ipv4-address-space.csv
if (( $? == 0 )) ; then
  chmod a+r "$FILENAME"
  mv "$FILENAME" "${DEST_DIR}/ipv4-address-space.csv"
fi

# Try and download manuf, only copy if it works
FILENAME=$(mktemp)
wget -nv --timeout=${TIMEOUT} -O "$FILENAME" https://raw.githubusercontent.com/wireshark/wireshark/master/manuf
if (( $? == 0 )) ; then
  chmod a+r "$FILENAME"
  mv "$FILENAME" "${DEST_DIR}/oui.txt"
fi

# Run the maxind geoipupdate program if available. See
# https://blog.maxmind.com/2019/12/18/significant-changes-to-accessing-and-using-geolite2-databases/
# https://dev.maxmind.com/geoip/geoipupdate/#For_Free_GeoLite2_Databases
if [ -x "/usr/bin/geoipupdate" ]; then
    /usr/bin/geoipupdate
    if [ -d /usr/share/GeoIP ]; then
      chmod a+r /usr/share/GeoIP/*.mmdb
    fi
    if [ -d /var/lib/GeoIP ]; then
      chmod a+r /var/lib/GeoIP/*.mmdb
    fi
fi
