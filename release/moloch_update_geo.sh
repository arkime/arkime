#!/bin/sh

cd BUILD_MOLOCH_INSTALL_DIR/etc
wget -N -nv --no-check-certificate https://www.iana.org/assignments/ipv4-address-space/ipv4-address-space.csv
wget -N -nv http://www.maxmind.com/download/geoip/database/asnum/GeoIPASNum.dat.gz; gunzip -f GeoIPASNum.dat.gz
wget -N -nv http://download.maxmind.com/download/geoip/database/asnum/GeoIPASNumv6.dat.gz; gunzip -f GeoIPASNumv6.dat.gz
wget -N -nv http://www.maxmind.com/download/geoip/database/GeoLiteCountry/GeoIP.dat.gz; gunzip -f GeoIP.dat.gz
wget -N -nv http://geolite.maxmind.com/download/geoip/database/GeoIPv6.dat.gz; gunzip -f GeoIPv6.dat.gz
