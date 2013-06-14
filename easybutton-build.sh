#!/bin/sh
# Use this script to install OS dependencies, downloading and compile moloch dependencies, and compile moloch capture.

# This script will 
# * use apt-get/yum to install OS dependancies
# * download known working versions of moloch dependancies
# * build them statically 
# * configure moloch-capture to use them
# * build moloch-capture


GLIB=2.34.3
YARA=1.7
GEOIP=1.4.8
PCAP=1.3.0
NIDS=1.24

TDIR="/data/moloch"
if [ "$#" -gt 0 ]; then
    TDIR="$1"
fi

# Installing dependencies
echo "MOLOCH: Installing Dependencies"
if [ -f "/etc/redhat-release" ]; then
  yum -y install wget curl pcre pcre-devel pkgconfig flex bison gcc-c++ zlib-devel e2fsprogs-devel openssl-devel file-devel make gettext libuuid-devel perl-JSON bzip2-libs bzip2-devel perl-libwww-perl libpng-devel xz libffi-devel
  if [ $? -ne 0 ]; then
    echo "MOLOCH - yum failed"
  exit
fi

if [ -f "/etc/debian_version" ]; then
  apt-get install wget curl libpcre3-dev uuid-dev libmagic-dev pkg-config g++ flex bison zlib1g-dev libffi-dev gettext libgeoip-dev make libjson-perl libbz2-dev libwww-perl libpng-dev xz-utils libffi-dev
  if [ $? -ne 0 ]; then
    echo "MOLOCH - apt-get failed"
  exit
fi




echo "MOLOCH: Downloading and building static thirdparty libraries"
if [ ! -d "thirdparty" ]; then
  mkdir thirdparty
fi
cd thirdparty

# glib
if [ ! -f "glib-$GLIB.tar.xz" ]; then
  wget http://ftp.gnome.org/pub/gnome/sources/glib/2.34/glib-$GLIB.tar.xz
fi
xzcat glib-$GLIB.tar.xz | tar xf -
(cd glib-$GLIB ; ./configure --disable-xattr --disable-shared --enable-static --disable-libelf --disable-selinux; make)

# yara
if [ ! -f "yara-$YARA.tar.gz" ]; then
  wget http://yara-project.googlecode.com/files/yara-$YARA.tar.gz
fi
tar zxf yara-$YARA.tar.gz
(cd yara-$YARA; ./configure --enable-static; make)

# GeoIP
if [ ! -f "GeoIP-$GEOIP.tar.gz" ]; then
  wget http://www.maxmind.com/download/geoip/api/c/GeoIP-$GEOIP.tar.gz
fi
tar zxf GeoIP-$GEOIP.tar.gz

# Not sure why this is required on some platforms
if [ -f "/usr/bin/libtoolize" ]; then
  (cd GeoIP-$GEOIP ; libtoolize -f)
fi
(cd GeoIP-$GEOIP ; ./configure --enable-static; make)

# libpcap
if [ ! -f "libpcap-$PCAP.tar.gz" ]; then
  wget http://www.tcpdump.org/release/libpcap-$PCAP.tar.gz
fi
tar zxf libpcap-$PCAP.tar.gz
(cd libpcap-$PCAP; ./configure --disable-libglib; make)

# libnids
if [ ! -f "libnids-$NIDS.tar.gz" ]; then
  wget http://downloads.sourceforge.net/project/libnids/libnids/$NIDS/libnids-$NIDS.tar.gz
fi
tar zxf libnids-$NIDS.tar.gz
( cd libnids-$NIDS; ./configure --enable-static --disable-libnet --with-libpcap=../libpcap-$PCAP --disable-libglib; make)


# Now build moloch
echo "MOLOCH: Building capture"
cd ..
echo "./configure --prefix=$TDIR --with-libpcap=thirdparty/libpcap-$PCAP --with-libnids=thirdparty/libnids-$NIDS --with-yara=thirdparty/yara-$YARA --with-GeoIP=thirdparty/GeoIP-$GEOIP --with-glib2=thirdparty/glib-$GLIB"
./configure --prefix=$TDIR --with-libpcap=thirdparty/libpcap-$PCAP --with-libnids=thirdparty/libnids-$NIDS --with-yara=thirdparty/yara-$YARA --with-GeoIP=thirdparty/GeoIP-$GEOIP --with-glib2=thirdparty/glib-$GLIB
make


