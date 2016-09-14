#!/bin/sh

cd /data/moloch/etc
wget --no-check-certificate https://www.iana.org/assignments/ipv4-address-space/ipv4-address-space.csv
wget http://www.maxmind.com/download/geoip/database/asnum/GeoIPASNum.dat.gz; gunzip -f GeoIPASNum.dat.gz
wget http://download.maxmind.com/download/geoip/database/asnum/GeoIPASNumv6.dat.gz; gunzip -f GeoIPASNumv6.dat.gz
wget http://www.maxmind.com/download/geoip/database/GeoLiteCountry/GeoIP.dat.gz; gunzip -f GeoIP.dat.gz
wget http://geolite.maxmind.com/download/geoip/database/GeoIPv6.dat.gz; gunzip -f GeoIPv6.dat.gz
