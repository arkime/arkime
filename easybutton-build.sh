#!/bin/sh
# Use this script to install OS dependencies, downloading and compile moloch dependencies, and compile moloch capture.

# This script will 
# * use apt-get/yum to install OS dependancies
# * download known working versions of moloch dependancies
# * build them statically 
# * configure moloch-capture to use them
# * build moloch-capture


GLIB=2.42.0
YARA=1.7
GEOIP=1.6.0
PCAP=1.7.2
NIDS=1.24
PFRING=6.0.2
CURL=7.42.1

TDIR="/data/moloch"
DOPFRING=0

while :
do
  case $1 in
  -p | --pf_ring | --pfring)
    DOPFRING=1
    shift
    ;;
  -d | --dir)
    TDIR=$2
    shift 2
    ;;
  -*)
    echo "Unknown option '$1'"
    exit 1
    ;;
  *)
    break
    ;;
  esac
done


MAKE=make

# Installing dependencies
echo "MOLOCH: Installing Dependencies"
if [ -f "/etc/redhat-release" ]; then
  yum -y install wget curl pcre pcre-devel pkgconfig flex bison gcc-c++ zlib-devel e2fsprogs-devel openssl-devel file-devel make gettext libuuid-devel perl-JSON bzip2-libs bzip2-devel perl-libwww-perl libpng-devel xz libffi-devel
  if [ $? -ne 0 ]; then
    echo "MOLOCH - yum failed"
    exit 1
  fi
fi

if [ -f "/etc/debian_version" ]; then
  apt-get -y install wget curl libpcre3-dev uuid-dev libmagic-dev pkg-config g++ flex bison zlib1g-dev libffi-dev gettext libgeoip-dev make libjson-perl libbz2-dev libwww-perl libpng-dev xz-utils libffi-dev
  if [ $? -ne 0 ]; then
    echo "MOLOCH - apt-get failed"
    exit 1
  fi
fi

if [ $(uname) == "FreeBSD" ]; then
    pkg_add -Fr wget curl pcre flex bison gettext e2fsprogs-libuuid glib gmake libexecinfo
    MAKE=gmake
fi




echo "MOLOCH: Downloading and building static thirdparty libraries"
if [ ! -d "thirdparty" ]; then
  mkdir thirdparty
fi
cd thirdparty

# glib
if [ $(uname) == "FreeBSD" ]; then
  #Screw it, use whatever the OS has
  WITHGLIB=" "
else
  WITHGLIB="--with-glib2=thirdparty/glib-$GLIB"
  if [ ! -f "glib-$GLIB.tar.xz" ]; then
    wget http://ftp.gnome.org/pub/gnome/sources/glib/2.42/glib-$GLIB.tar.xz
  fi

  if [ ! -f "glib-$GLIB/gio/.libs/libgio-2.0.a" -o ! -f "glib-$GLIB/glib/.libs/libglib-2.0.a" ]; then
    xzcat glib-$GLIB.tar.xz | tar xf -
    (cd glib-$GLIB ; ./configure --disable-xattr --disable-shared --enable-static --disable-libelf --disable-selinux; $MAKE)
    if [ $? -ne 0 ]; then
      echo "MOLOCH: $MAKE failed"
      exit 1
    fi
  else
    echo "MOLOCH: Not rebuilding glib"
  fi
fi

# yara
if [ ! -f "yara-$YARA.tar.gz" ]; then
  wget http://yara-project.googlecode.com/files/yara-$YARA.tar.gz
fi

if [ ! -f "yara-$YARA/libyara/.libs/libyara.a" ]; then
  tar zxf yara-$YARA.tar.gz
  (cd yara-$YARA; ./configure --enable-static; $MAKE)
  if [ $? -ne 0 ]; then
    echo "MOLOCH: $MAKE failed"
    exit 1
  fi
else
  echo "MOLOCH: Not rebuilding yara"
fi

# GeoIP
if [ ! -f "GeoIP-$GEOIP.tar.gz" ]; then
  wget http://www.maxmind.com/download/geoip/api/c/GeoIP-$GEOIP.tar.gz
fi

if [ ! -f "GeoIP-$GEOIP/libGeoIP/.libs/libGeoIP.a" ]; then
tar zxf GeoIP-$GEOIP.tar.gz

# Crossing fingers, this is no longer needed
# Not sure why this is required on some platforms
#  if [ -f "/usr/bin/libtoolize" ]; then
#    (cd GeoIP-$GEOIP ; libtoolize -f)
#  fi

  (cd GeoIP-$GEOIP ; ./configure --enable-static; $MAKE)
  if [ $? -ne 0 ]; then
    echo "MOLOCH: $MAKE failed"
    exit 1
  fi
else
  echo "MOLOCH: Not rebuilding libGeoIP"
fi

if [ $DOPFRING -eq 1 ]; then
    # pfring
    echo "MOLOCH: Building libpcap with pfring";
    if [ ! -f "PF_RING-$PFRING.tar.gz" ]; then
      wget -O PF_RING-$PFRING.tar.gz http://sourceforge.net/projects/ntop/files/PF_RING/PF_RING-$PFRING.tar.gz/download
    fi
    tar zxf PF_RING-$PFRING.tar.gz
    (cd PF_RING-$PFRING; $MAKE)
    if [ $? -ne 0 ]; then
      echo "MOLOCH: pfring failed to build"
      exit 1
    fi

    PFRINGDIR=`pwd`/PF_RING-$PFRING
    PCAPDIR=$PFRINGDIR/userland/libpcap
    PCAPBUILD="--with-pfring=$PFRINGDIR"
else
    echo "MOLOCH: Building libpcap without pfring";
    # libpcap
    if [ ! -f "libpcap-$PCAP.tar.gz" ]; then
      wget http://www.tcpdump.org/release/libpcap-$PCAP.tar.gz
    fi
    tar zxf libpcap-$PCAP.tar.gz
    (cd libpcap-$PCAP; ./configure --disable-dbus; $MAKE)
    if [ $? -ne 0 ]; then
      echo "MOLOCH: $MAKE failed"
      exit 1
    fi
    PCAPDIR=`pwd`/libpcap-$PCAP
    PCAPBUILD="--with-libpcap=$PCAPDIR"
fi

# libnids
if [ ! -f "libnids-$NIDS.tar.gz" ]; then
  wget http://downloads.sourceforge.net/project/libnids/libnids/$NIDS/libnids-$NIDS.tar.gz
fi

if [ ! -f "libnids-$NIDS/src/libnids.a" ]; then
  tar zxf libnids-$NIDS.tar.gz
  if [ $(uname) == "FreeBSD" ]; then
    ( cd libnids-$NIDS; cp ../yara-$YARA/config.sub . ; touch src/alloca.h )
  fi
  ( cd libnids-$NIDS; ./configure --enable-static --disable-libnet --with-libpcap=$PCAPDIR --disable-libglib; $MAKE)
  if [ $? -ne 0 ]; then
    echo "MOLOCH: $MAKE failed"
    exit 1
  fi
else 
  echo "MOLOCH: Not rebuilding libnids"
fi

# curl
if [ ! -f "curl-$CURL.tar.gz" ]; then
  wget http://curl.haxx.se/download/curl-$CURL.tar.gz
fi

if [ ! -f "curl-$CURL/lib/.libs/curl.a" ]; then
  tar zxf curl-$CURL.tar.gz
  ( cd curl-$CURL; ./configure --disable-ldap --disable-ldaps --without-libidn; $MAKE)
  if [ $? -ne 0 ]; then
    echo "MOLOCH: $MAKE failed"
    exit 1
  fi
else 
  echo "MOLOCH: Not rebuilding curl"
fi


# Now build moloch
echo "MOLOCH: Building capture"
cd ..
echo "./configure --prefix=$TDIR $PCAPBUILD --with-libnids=thirdparty/libnids-$NIDS --with-yara=thirdparty/yara-$YARA --with-GeoIP=thirdparty/GeoIP-$GEOIP $WITHGLIB --with-curl=thirdparty/curl-$CURL"
./configure --prefix=$TDIR $PCAPBUILD --with-libnids=thirdparty/libnids-$NIDS --with-yara=thirdparty/yara-$YARA --with-GeoIP=thirdparty/GeoIP-$GEOIP $WITHGLIB --with-curl=thirdparty/curl-$CURL

$MAKE
if [ $? -ne 0 ]; then
  echo "MOLOCH: $MAKE failed"
  exit 1
fi

exit 0
