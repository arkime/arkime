#!/bin/sh

cd BUILD_MOLOCH_INSTALL_DIR/etc
wget -N -nv --no-check-certificate https://www.iana.org/assignments/ipv4-address-space/ipv4-address-space.csv

wget -N -nv -O GeoLite2-Country.mmdb.gz 'https://updates.maxmind.com/app/update_secure?edition_id=GeoLite2-Country'
/bin/rm -f GeoLite2-Country.mmdb
zcat GeoLite2-Country.mmdb.gz > GeoLite2-Country.mmdb

wget -N -nv -O GeoLite2-ASN.mmdb.gz 'https://updates.maxmind.com/app/update_secure?edition_id=GeoLite2-ASN'
/bin/rm -f GeoLite2-ASN.mmdb
zcat GeoLite2-ASN.mmdb.gz > GeoLite2-ASN.mmdb

wget -nv -O oui.txt https://raw.githubusercontent.com/wireshark/wireshark/master/manuf
