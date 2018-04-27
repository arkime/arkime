#!/bin/sh
# Use this script to install OS dependencies, downloading and compile moloch dependencies, compile moloch capture, optionally install

# This script will
# * use apt-get/yum to install OS dependancies
# * download known working versions of moloch dependancies
# * build them statically
# * configure moloch-capture to use them
# * build moloch-capture
# * install node unless --nonode
# * install moloch if --install


GLIB=2.54.3
YARA=3.7.1
MAXMIND=1.3.2
PCAP=1.8.1
CURL=7.59.0
LUA=5.3.4
DAQ=2.0.6
NODE=8.11.1

TDIR="/data/moloch"
DOPFRING=0
DODAQ=0
DOCLEAN=0
DONODE=1
DOINSTALL=0

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
  --daq)
    DODAQ=1
    shift
    ;;
  --clean)
    DOCLEAN=1
    shift
    ;;
  --install)
    DOINSTALL=1
    shift
    ;;
  --nonode)
    DONODE=0
    shift
    ;;
  --help)
    echo "Make it easier to build Moloch!  This will download and build thirdparty libraries plus build Moloch."
    echo "--dir <directory>   = The directory to install everything into [/data/moloch]"
    echo "--clean             = Do a 'make clean' first"
    echo "--install           = Do a 'make install' at the end, adding our node to the path"
    echo "--nonode            = Do NOT download and install nodejs into the moloch directory"
    echo "--pfring            = Build pfring support"
    echo "--daq               = Build daq support"
    exit 0;
    ;;
  -*)
    echo "Unknown option '$1', try '--help'"
    exit 1
    ;;
  *)
    break
    ;;
  esac
done

# Check the existance of sudo
command -v sudo >/dev/null 2>&1 || { echo >&2 "MOLOCH: sudo is required to be installed"; exit 1; }

MAKE=make

# Installing dependencies
echo "MOLOCH: Installing Dependencies"
if [ -f "/etc/redhat-release" ]; then
  sudo yum -y install wget curl pcre pcre-devel pkgconfig flex bison gcc-c++ zlib-devel e2fsprogs-devel openssl-devel file-devel make gettext libuuid-devel perl-JSON bzip2-libs bzip2-devel perl-libwww-perl libpng-devel xz libffi-devel readline-devel libtool libyaml-devel perl-Socket6
  if [ $? -ne 0 ]; then
    echo "MOLOCH: yum failed"
    exit 1
  fi
fi

if [ -f "/etc/debian_version" ]; then
  sudo apt-get -y install wget curl libpcre3-dev uuid-dev libmagic-dev pkg-config g++ flex bison zlib1g-dev libffi-dev gettext libgeoip-dev make libjson-perl libbz2-dev libwww-perl libpng-dev xz-utils libffi-dev libssl-dev libreadline-dev libtool libyaml-dev dh-autoreconf libsocket6-perl
  if [ $? -ne 0 ]; then
    echo "MOLOCH: apt-get failed"
    exit 1
  fi
fi

if [ "$(uname)" = "FreeBSD" ]; then
    sudo pkg_add -Fr wget curl pcre flex bison gettext e2fsprogs-libuuid glib gmake libexecinfo
    MAKE=gmake
fi




echo "MOLOCH: Downloading and building static thirdparty libraries"
if [ ! -d "thirdparty" ]; then
  mkdir thirdparty
fi
cd thirdparty || exit

TPWD=`pwd`

# glib
if [ "$(uname)" = "FreeBSD" ]; then
  #Screw it, use whatever the OS has
  WITHGLIB=" "
else
  WITHGLIB="--with-glib2=thirdparty/glib-$GLIB"
  if [ ! -f "glib-$GLIB.tar.xz" ]; then
    GLIBDIR=$(echo $GLIB | cut -d. -f 1-2)
    wget "http://ftp.gnome.org/pub/gnome/sources/glib/$GLIBDIR/glib-$GLIB.tar.xz"
  fi

  if [ ! -f "glib-$GLIB/gio/.libs/libgio-2.0.a" ] || [ ! -f "glib-$GLIB/glib/.libs/libglib-2.0.a" ]; then
    xzcat glib-$GLIB.tar.xz | tar xf -
    (cd glib-$GLIB ; ./configure --disable-xattr --disable-shared --enable-static --disable-libelf --disable-selinux --disable-libmount --with-pcre=internal; $MAKE)
    if [ $? -ne 0 ]; then
      echo "MOLOCH: $MAKE failed"
      exit 1
    fi
  else
    echo "MOLOCH: Not rebuilding glib"
  fi
fi

# yara
if [ ! -f "yara/yara-$YARA.tar.gz" ]; then
  mkdir -p yara
  wget https://github.com/VirusTotal/yara/archive/v$YARA.tar.gz -O yara/yara-$YARA.tar.gz
fi

if [ ! -f "yara/yara-$YARA/libyara/.libs/libyara.a" ]; then
  (cd yara ; tar zxf yara-$YARA.tar.gz)
  (cd yara/yara-$YARA; ./bootstrap.sh ; ./configure --enable-static; $MAKE)
  if [ $? -ne 0 ]; then
    echo "MOLOCH: $MAKE failed"
    exit 1
  fi
else
  echo "MOLOCH: Not rebuilding yara"
fi

# Maxmind
if [ ! -f "libmaxminddb-$MAXMIND.tar.gz" ]; then
  wget https://github.com/maxmind/libmaxminddb/releases/download/$MAXMIND/libmaxminddb-$MAXMIND.tar.gz
fi

if [ ! -f "libmaxminddb-$MAXMIND/src/.libs/libmaxminddb.a" ]; then
  tar zxf libmaxminddb-$MAXMIND.tar.gz

  (cd libmaxminddb-$MAXMIND ; ./configure --enable-static; $MAKE)
  if [ $? -ne 0 ]; then
    echo "MOLOCH: $MAKE failed"
    exit 1
  fi
else
  echo "MOLOCH: Not rebuilding libmaxmind"
fi

# libpcap
if [ ! -f "libpcap-$PCAP.tar.gz" ]; then
  wget http://www.tcpdump.org/release/libpcap-$PCAP.tar.gz
fi
if [ ! -f "libpcap-$PCAP/libpcap.a" ]; then
  tar zxf libpcap-$PCAP.tar.gz
  echo "MOLOCH: Building libpcap";
  (cd libpcap-$PCAP; ./configure --disable-dbus --disable-usb --disable-canusb --disable-bluetooth --with-snf=no; $MAKE)
  if [ $? -ne 0 ]; then
    echo "MOLOCH: $MAKE failed"
    exit 1
  fi
else
  echo "MOLOCH: NOT rebuilding libpcap";
fi
PCAPDIR=$TPWD/libpcap-$PCAP
PCAPBUILD="--with-libpcap=$PCAPDIR"

# curl
if [ ! -f "curl-$CURL.tar.gz" ]; then
  wget http://curl.haxx.se/download/curl-$CURL.tar.gz
fi

if [ ! -f "curl-$CURL/lib/.libs/libcurl.a" ]; then
  tar zxf curl-$CURL.tar.gz
  ( cd curl-$CURL; ./configure --disable-ldap --disable-ldaps --without-libidn2 --without-librtmp --without-libpsl --without-nghttp2 --without-nghttp2 --without-nss; $MAKE)
  if [ $? -ne 0 ]; then
    echo "MOLOCH: $MAKE failed"
    exit 1
  fi
else
  echo "MOLOCH: Not rebuilding curl"
fi

# lua
if [ ! -f "lua-$LUA.tar.gz" ]; then
  wget https://www.lua.org/ftp/lua-$LUA.tar.gz
fi

if [ ! -f "lua-$LUA/src/liblua.a" ]; then
  tar zxf lua-$LUA.tar.gz
  ( cd lua-$LUA; make MYCFLAGS=-fPIC linux)
  if [ $? -ne 0 ]; then
    echo "MOLOCH: $MAKE failed"
    exit 1
  fi
else
  echo "MOLOCH: Not rebuilding lua"
fi

# daq
if [ $DODAQ -eq 1 ]; then
  if [ ! -f "daq-$DAQ.tar.gz" ]; then
    wget https://www.snort.org/downloads/snort/daq-$DAQ.tar.gz
  fi

  if [ ! -f "daq-$DAQ/api/.libs/libdaq_static.a" ]; then
    tar zxf daq-$DAQ.tar.gz
    ( cd daq-$DAQ; ./configure --with-libpcap-includes=$TPWD/libpcap-$PCAP/ --with-libpcap-libraries=$TPWD/libpcap-$PCAP; make; sudo make install)
    if [ $? -ne 0 ]; then
      echo "MOLOCH: $MAKE failed"
      exit 1
    fi
  else
    echo "MOLOCH: Not rebuilding daq"
  fi
fi


# Now build moloch
echo "MOLOCH: Building capture"
cd ..
echo "./configure --prefix=$TDIR $PCAPBUILD --with-yara=thirdparty/yara/yara-$YARA --with-maxminddb=thirdparty/libmaxminddb-$MAXMIND $WITHGLIB --with-curl=thirdparty/curl-$CURL --with-lua=thirdparty/lua-$LUA"
./configure --prefix=$TDIR $PCAPBUILD --with-yara=thirdparty/yara/yara-$YARA --with-maxminddb=thirdparty/libmaxminddb-$MAXMIND $WITHGLIB --with-curl=thirdparty/curl-$CURL --with-lua=thirdparty/lua-$LUA

if [ $DOCLEAN -eq 1 ]; then
    $MAKE clean
fi

$MAKE
if [ $? -ne 0 ]; then
  echo "MOLOCH: $MAKE failed"
  exit 1
fi

# Build plugins
(cd capture/plugins/lua; $MAKE)

if [ $DOPFRING -eq 1 ] || [ -f "/usr/local/lib/libpfring.so" ]; then
    (cd capture/plugins/pfring; $MAKE)
fi

if [ $DODAQ -eq 1 ]; then
    (cd capture/plugins/daq; $MAKE)
fi

if [ -f "/opt/snf/lib/libsnf.so" ]; then
    (cd capture/plugins/snf; $MAKE)
fi

if [ $DONODE -eq 1 ] && [ ! -f "$TDIR/bin/node" ]; then
    echo "MOLOCH: Installing node $NODE"
    sudo mkdir -p $TDIR/bin $TDIR/etc
    if [ ! -f node-v$NODE-linux-x64.tar.xz ] ; then
        wget https://nodejs.org/download/release/v$NODE/node-v$NODE-linux-x64.tar.xz
    fi
    sudo tar xfC node-v$NODE-linux-x64.tar.xz $TDIR
    (cd $TDIR/bin ; sudo ln -s ../node-v$NODE-linux-x64/bin/* .)
fi

if [ $DOINSTALL -eq 1 ]; then
    export PATH=$TDIR/bin:$PATH
    sudo make install
    echo "MOLOCH: Installed, now type sudo make config'"
else
    echo "MOLOCH: Now type 'sudo make install' and 'sudo make config'"
fi


exit 0
