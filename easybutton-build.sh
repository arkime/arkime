#!/bin/bash
# Use this script to install OS dependencies, downloading and compile arkime dependencies, compile arkime capture, optionally install

# This script will
# * use apt-get/yum to install OS dependancies
# * download known working versions of arkime dependancies
# * build them statically
# * configure moloch-capture to use them
# * build moloch-capture
# * install node unless --nonode
# * install arkime if --install


# newer glib requires pcre2, issues on Centos 7
GLIB=2.72.4
# newer yara requires newer ssl, issues on Centos 7
YARA=4.2.3
MAXMIND=1.7.1
PCAP=1.10.4
CURL=8.3.0
LUA=5.3.6
DAQ=2.0.7
NGHTTP2=1.56.0
ZSTD=1.5.5
KAFKA=1.5.3

# node v18 doesn't support Centos 7/Ubuntu 18 and will be reset to v16 below
NODE=18.18.0
OLDNODE=16.20.2

TDIR="/opt/arkime"
DOPFRING=0
DODAQ=0
DOKAFKA=0
BUILDKAFKA=0
DOCLEAN=0
DONODE=1
DOINSTALL=0
DORMINSTALL=0
DOTHIRDPARTY=1
BUILDZSTD=1

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
  --kafka)
    DOKAFKA=1
    BUILDKAFKA=1
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
  --rminstall)
    DORMINSTALL=1
    shift
    ;;
  --nonode)
    DONODE=0
    shift
    ;;
  --nothirdparty)
    DOTHIRDPARTY=0
    shift
    ;;
  --help)
    echo "Make it easier to build Arkime!  This will download and build thirdparty libraries plus build Arkime."
    echo "--dir <directory>   = The directory to install everything into [$TDIR]"
    echo "--clean             = Do a 'make clean' first"
    echo "--rminstall         = Do a 'rm -rf <dir>' first"
    echo "--install           = Do a 'make install' at the end, adding our node to the path"
    echo "--nonode            = Do NOT download and install nodejs into the moloch directory"
    echo "--pfring            = Build pfring support"
    echo "--daq               = Build daq support"
    echo "--nothirdparty      = Use OS packages instead of building thirdparty"
    echo "--kafka             = Build kafka support"
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

# Warn users
echo ""
echo "This script is for building Arkime from source and meant for people who enjoy pain. The prebuilt versions at https://arkime.com/#download are recommended for installation."
echo ""

# Check the existance of sudo
command -v sudo >/dev/null 2>&1 || { echo >&2 "ARKIME: sudo is required to be installed"; exit 1; }

# Check if in right directory
[ -f "./easybutton-build.sh" ] || { echo >&2 "ARKIME: must run from arkime directory"; exit 1; }

MAKE=make
UNAME="$(uname)"

# Installing dependencies
echo "ARKIME: Installing Dependencies"
if [ -f "/etc/redhat-release" ] || [ -f "/etc/system-release" ]; then
  . /etc/os-release
  if [[ $DONODE == "1" && "$VERSION_ID" == "7" ]]; then
      NODE=$OLDNODE
      echo "Using $NODE on Centos 7 build since Node v18 isn't supported"
  fi

  if [[ "$VERSION_ID" == 9* || "$VERSION_ID" == 2023 ]]; then
      sudo yum install -y glib2-devel libmaxminddb-devel libcurl-devel
      WITHGLIB=" "
      WITHCURL=" "
  fi
  sudo yum -y install --skip-broken wget curl pcre pcre-devel pkgconfig flex bison gcc-c++ zlib-devel e2fsprogs-devel openssl-devel file-devel make gettext libuuid-devel perl-JSON bzip2-libs bzip2-devel perl-libwww-perl libpng-devel xz libffi-devel readline-devel libtool libyaml-devel perl-Socket6 perl-Test-Differences
  if [ $? -ne 0 ]; then
    echo "ARKIME: yum failed"
    exit 1
  fi
fi

if [ -f "/etc/debian_version" ]; then
  . /etc/os-release
  if [[ "$VERSION_CODENAME" == "bionic" || "$VERSION_CODENAME" == "focal" ]]; then
      BUILDZSTD=1
  fi
  sudo apt-get -qq install wget curl libpcre3-dev uuid-dev libmagic-dev pkg-config g++ flex bison zlib1g-dev libffi-dev gettext libgeoip-dev make libjson-perl libbz2-dev libwww-perl libpng-dev xz-utils libffi-dev libssl-dev libreadline-dev libtool libyaml-dev dh-autoreconf libsocket6-perl libtest-differences-perl libzstd-dev
  if [ $? -ne 0 ]; then
    echo "ARKIME: apt-get failed"
    exit 1
  fi

  if [[ $DONODE == "1" && "$VERSION_CODENAME" == "bionic" ]]; then
      NODE=$OLDNODE
      echo "Using $NODE on Ubuntu 18 build since Node v18 isn't supported"
  fi

  # Just use OS packages, currently for Ubuntu 22
  if [ $DOTHIRDPARTY -eq 0 ]; then
      apt-get -qq install libmaxminddb-dev libcurl4-openssl-dev libyara-dev libglib2.0-dev libpcap-dev libnghttp2-dev liblua5.4-dev librdkafka-dev
      if [ $? -ne 0 ]; then
        echo "ARKIME: apt-get failed"
        exit 1
      fi
      export LUA_CFLAGS="-I/usr/include/lua5.4/"
      export LUA_LIBS="-llua5.4"
      with_lua=no

      export KAFKA_CFLAGS="-I/usr/include/librdkafka/"
      export KAFKA_LIBS="-lrdkafka"
      with_kafka=no
  fi
fi

if [ "$UNAME" = "Darwin" ]; then
  DONODE=0
  DOINSTALL=0
  if [ -x "/opt/local/bin/port" ]; then
    sudo port install libpcap yara glib2 jansson ossp-uuid libmaxminddb libmagic pcre lua libyaml wget nghttp2 librdkafka zstd
  elif [ -x "/usr/local/bin/brew" ] || [ -x "/opt/homebrew/bin/brew" ]; then
    brew install libpcap yara glib jansson ossp-uuid libmaxminddb libmagic pcre lua libyaml openssl wget autoconf automake nghttp2 zstd librdkafka
  else
    echo "ARKIME: Please install MacPorts or Homebrew"
    exit 1
  fi
fi

if [ "$UNAME" = "FreeBSD" ]; then
  sudo pkg_add -Fr wget curl pcre flex bison gettext e2fsprogs-libuuid glib gmake libexecinfo
  MAKE=gmake
fi

# do autoconf
./bootstrap.sh

if [ "$UNAME" = "Darwin" ]; then
  echo "ARKIME: Building capture"
  if [ -x "/opt/local/bin/port" ]; then
    echo './configure \
      --with-maxminddb=/opt/local \
      --with-libpcap=/opt/local \
      --with-yara=/opt/local LDFLAGS="-L/opt/local/lib" \
      --with-glib2=no GLIB2_CFLAGS="-I/opt/local/include/glib-2.0 -I/opt/local/lib/glib-2.0/include" GLIB2_LIBS="-L/opt/local/lib -lglib-2.0 -lgmodule-2.0 -lgobject-2.0 -lgio-2.0" \
      --with-pfring=no \
      --with-curl=yes \
      --with-nghttp2=yes \
      --with-lua=no LUA_CFLAGS="-I/opt/local/include" LUA_LIBS="-L/opt/local/lib -llua"'

    ./configure \
      --with-maxminddb=/opt/local \
      --with-libpcap=/opt/local \
      --with-yara=/opt/local LDFLAGS="-L/opt/local/lib" \
      --with-glib2=no GLIB2_CFLAGS="-I/opt/local/include/glib-2.0 -I/opt/local/lib/glib-2.0/include" GLIB2_LIBS="-L/opt/local/lib -lglib-2.0 -lgmodule-2.0 -lgobject-2.0 -lgio-2.0" \
      --with-pfring=no \
      --with-curl=yes \
      --with-nghttp2=yes \
      --with-lua=no LUA_CFLAGS="-I/opt/local/include" LUA_LIBS="-L/opt/local/lib -llua"
  elif [ -x "/usr/local/bin/brew" ]; then
    echo './configure \
      --with-libpcap=/usr/local/opt/libpcap \
      --with-yara=/usr/local LDFLAGS="-L/usr/local/lib" \
      --with-glib2=no GLIB2_CFLAGS="-I/usr/local/include/glib-2.0 -I/usr/local/lib/glib-2.0/include -I/usr/local/opt/openssl@1.1/include" GLIB2_LIBS="-L/usr/local/lib -lglib-2.0 -lgmodule-2.0 -lgobject-2.0 -lgio-2.0 -L/usr/local/opt/openssl@1.1/lib" \
      --with-pfring=no \
      --with-curl=yes \
      --with-nghttp2=yes \
      --with-lua=no LUA_CFLAGS="-I/usr/local/include/lua" LUA_LIBS="-L/usr/local/lib -llua" \
      --with-zstd=yes \
      --with-kafka=no KAFKA_CFLAGS="-I/opt/homebrew/Cellar/librdkafka/2.0.2/include/librdkafka" KAFKA_LIBS="-L/opt/homebrew/lib -lrdkafka"'

    ./configure \
      --with-libpcap=/usr/local/opt/libpcap \
      --with-yara=/usr/local LDFLAGS="-L/usr/local/lib" \
      --with-glib2=no GLIB2_CFLAGS="-I/usr/local/include/glib-2.0 -I/usr/local/lib/glib-2.0/include -I/usr/local/opt/openssl@1.1/include" GLIB2_LIBS="-L/usr/local/lib -lglib-2.0 -lgmodule-2.0 -lgobject-2.0 -lgio-2.0 -L/usr/local/opt/openssl@1.1/lib" \
      --with-pfring=no \
      --with-curl=yes \
      --with-nghttp2=yes \
      --with-lua=no LUA_CFLAGS="-I/usr/local/include/lua" LUA_LIBS="-L/usr/local/lib -llua" \
      --with-zstd=yes \
      --with-kafka=no KAFKA_CFLAGS="-I/usr/local/Cellar/librdkafka/2.0.2/include/librdkafka" KAFKA_LIBS="-L/opt/homebrew/lib -lrdkafka"

  elif [ -x "/opt/homebrew/bin/brew" ]; then
    echo './configure \
      --with-libpcap=/opt/homebrew/opt/libpcap \
      --with-maxminddb=/opt/homebrew \
      --with-yara=/opt/homebrew CFLAGS="-I/opt/homebrew/include" LDFLAGS="-L/opt/homebrew/lib" \
      --with-glib2=no GLIB2_CFLAGS="-I/opt/homebrew/include/glib-2.0 -I/opt/homebrew/lib/glib-2.0/include -I/opt/homebrew/opt/openssl@1.1/include" GLIB2_LIBS="-L/opt/homebrew/lib -lglib-2.0 -lgmodule-2.0 -lgobject-2.0 -lgio-2.0 -L/opt/homebrew/opt/openssl@1.1/lib" \
      --with-pfring=no \
      --with-curl=yes \
      --with-nghttp2=yes \
      --with-lua=no LUA_CFLAGS="-I/opt/homebrew/include/lua" LUA_LIBS="-L/opt/homebrew/lib -llua" \
      --with-zstd=yes \
      --with-kafka=no KAFKA_CFLAGS="-I/opt/homebrew/Cellar/librdkafka/2.0.2/include/librdkafka" KAFKA_LIBS="-L/opt/homebrew/lib -lrdkafka"'

    ./configure \
      --with-libpcap=/opt/homebrew/opt/libpcap \
      --with-maxminddb=/opt/homebrew \
      --with-yara=/opt/homebrew CFLAGS="-I/opt/homebrew/include" LDFLAGS="-L/opt/homebrew/lib" \
      --with-glib2=no GLIB2_CFLAGS="-I/opt/homebrew/include/glib-2.0 -I/opt/homebrew/lib/glib-2.0/include -I/opt/homebrew/opt/openssl@1.1/include" GLIB2_LIBS="-L/opt/homebrew/lib -lglib-2.0 -lgmodule-2.0 -lgobject-2.0 -lgio-2.0 -L/opt/homebrew/opt/openssl@1.1/lib" \
      --with-pfring=no \
      --with-curl=yes \
      --with-nghttp2=yes \
      --with-lua=no LUA_CFLAGS="-I/opt/homebrew/include/lua" LUA_LIBS="-L/opt/homebrew/lib -llua" \
      --with-zstd=yes \
      --with-kafka=no KAFKA_CFLAGS="-I/opt/homebrew/Cellar/librdkafka/2.0.2/include/librdkafka" KAFKA_LIBS="-L/opt/homebrew/lib -lrdkafka"
  fi
elif [ -f "/etc/arch-release" ]; then
    sudo pacman -Sy --noconfirm gcc ruby make python-pip git perl perl-test-differences sudo wget gawk lua geoip yara file libpcap libmaxminddb libnet lua libtool autoconf gettext automake perl-http-message perl-lwp-protocol-https perl-json perl-socket6 zstd openssl-1.1 pcre librdkafka

    DOKAFKA=1
    BUILDKAFKA=0
    BUILDZSTD=0

    echo './configure \
      --with-zstd=yes \
      --with-libpcap=no \
      --with-yara=no \
      --with-glib2=no \
      --with-pfring=no \
      --with-curl=no \
      --with-lua=no LIBS="-lpcap -lyara -llua -lcurl" GLIB2_CFLAGS="-I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include" GLIB2_LIBS="-lglib-2.0 -lgmodule-2.0 -lgobject-2.0 -lgio-2.0"
      KAFKA_LIBS="-lrdkafka" KAFKA_CFLAGS="-I/usr/include/librdkafka" \
      --with-kafka=no'
    ./configure \
      --with-zstd=yes \
      --with-libpcap=no \
      --with-yara=no \
      --with-glib2=no \
      --with-pfring=no \
      --with-curl=no \
      --with-lua=no LIBS="-lpcap -lyara -llua -lcurl" GLIB2_CFLAGS="-I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include" GLIB2_LIBS="-lglib-2.0 -lgmodule-2.0 -lgobject-2.0 -lgio-2.0" \
      KAFKA_LIBS="-lrdkafka" KAFKA_CFLAGS="-I/usr/include/librdkafka" \
      --with-kafka=no
elif [ $DOTHIRDPARTY -eq 0 ]; then
    ./configure --with-lua=$with_lua --with-kafka=$with_kafka
else
  echo "ARKIME: Downloading and building static thirdparty libraries"
  if [ ! -d "thirdparty" ]; then
    mkdir thirdparty
  fi
  cd thirdparty || exit

  TPWD=`pwd`

  # glib
  if [ "$UNAME" = "FreeBSD" ]; then
    #Screw it, use whatever the OS has
    WITHGLIB=" "
  elif [ ! -z "$WITHGLIB" ]; then
    echo "ARKIME: withglib $WITHGLIB"
  else
    WITHGLIB="--with-glib2=thirdparty/glib-$GLIB"
    if [ ! -f "glib-$GLIB.tar.xz" ]; then
      GLIBDIR=$(echo $GLIB | cut -d. -f 1-2)
      wget "https://ftp.gnome.org/pub/gnome/sources/glib/$GLIBDIR/glib-$GLIB.tar.xz"
    fi

    if [ ! -f "glib-$GLIB/_build/gio/libgio-2.0.a" ] || [ ! -f "glib-$GLIB/_build/glib/libglib-2.0.a" ]; then
      sudo pip3 install meson
      git clone https://github.com/ninja-build/ninja.git
      (echo $PATH; cd ninja; git checkout release; python3 configure.py --bootstrap)
      xzcat glib-$GLIB.tar.xz | tar xf -
      (export PATH=$TPWD/ninja:$PATH; cd glib-$GLIB ; meson _build -Ddefault_library=static -Dselinux=disabled -Dxattr=false -Dlibmount=disabled; ninja -C _build)
      if [ $? -ne 0 ]; then
        echo "ARKIME: $MAKE failed"
        exit 1
      fi
    else
      echo "ARKIME: Not rebuilding glib"
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
      echo "ARKIME: $MAKE failed"
      exit 1
    fi
  else
    echo "ARKIME: Not rebuilding yara"
  fi

  # Maxmind
  if [ ! -f "libmaxminddb-$MAXMIND.tar.gz" ]; then
    wget https://github.com/maxmind/libmaxminddb/releases/download/$MAXMIND/libmaxminddb-$MAXMIND.tar.gz
  fi

  if [ ! -f "libmaxminddb-$MAXMIND/src/.libs/libmaxminddb.a" ]; then
    tar zxf libmaxminddb-$MAXMIND.tar.gz

    (cd libmaxminddb-$MAXMIND ; ./configure --enable-static; $MAKE)
    if [ $? -ne 0 ]; then
      echo "ARKIME: $MAKE failed"
      exit 1
    fi
  else
    echo "ARKIME: Not rebuilding libmaxmind"
  fi

  # libpcap
  if [ ! -f "libpcap-$PCAP.tar.gz" ]; then
    wget https://www.tcpdump.org/release/libpcap-$PCAP.tar.gz
  fi
  if [ ! -f "libpcap-$PCAP/libpcap.a" ]; then
    tar zxf libpcap-$PCAP.tar.gz
    echo "ARKIME: Building libpcap";
    (cd libpcap-$PCAP; ./configure --disable-rdma --disable-dbus --disable-usb --disable-bluetooth --with-snf=no; $MAKE)
    if [ $? -ne 0 ]; then
      echo "ARKIME: $MAKE failed"
      exit 1
    fi
  else
    echo "ARKIME: NOT rebuilding libpcap";
  fi
  PCAPDIR=$TPWD/libpcap-$PCAP
  PCAPBUILD="--with-libpcap=$PCAPDIR"

  # curl
  if [ ! -z "$WITHCURL" ]; then
    echo "ARKIME: withcurl $WITHCURL"
  else
    WITHCURL="--with-curl=thirdparty/curl-$CURL"
    if [ ! -f "curl-$CURL.tar.gz" ]; then
      wget https://curl.haxx.se/download/curl-$CURL.tar.gz
    fi

    if [ ! -f "curl-$CURL/lib/.libs/libcurl.a" ]; then
      tar zxf curl-$CURL.tar.gz
      ( cd curl-$CURL; ./configure --disable-ldap --disable-ldaps --without-libidn2 --without-librtmp --without-libpsl --without-nghttp2 --without-nghttp2 --without-nss --with-openssl --without-zstd; $MAKE)
      if [ $? -ne 0 ]; then
        echo "ARKIME: $MAKE failed"
        exit 1
      fi
    else
      echo "ARKIME: Not rebuilding curl"
    fi
  fi

  # nghttp2
  if [ ! -f "nghttp2-$NGHTTP2.tar.gz" ]; then
    wget https://github.com/nghttp2/nghttp2/releases/download/v$NGHTTP2/nghttp2-$NGHTTP2.tar.gz
  fi

  if [ ! -f "nghttp2-$NGHTTP2/lib/.libs/libnghttp2.a" ]; then
    tar zxf nghttp2-$NGHTTP2.tar.gz
    ( cd nghttp2-$NGHTTP2; ./configure --enable-lib-only; $MAKE)
    if [ $? -ne 0 ]; then
      echo "ARKIME: $MAKE failed"
      exit 1
    fi
  else
    echo "ARKIME: Not rebuilding nghttp2"
  fi

  # lua
  if [ ! -f "lua-$LUA.tar.gz" ]; then
    wget https://www.lua.org/ftp/lua-$LUA.tar.gz
  fi

  if [ ! -f "lua-$LUA/src/liblua.a" ]; then
    tar zxf lua-$LUA.tar.gz
    ( cd lua-$LUA; make MYCFLAGS=-fPIC linux)
    if [ $? -ne 0 ]; then
      echo "ARKIME: $MAKE failed"
      exit 1
    fi
  else
    echo "ARKIME: Not rebuilding lua"
  fi

  # daq
  if [ $DODAQ -eq 1 ]; then
    if [ ! -f "daq-$DAQ.tar.gz" ]; then
      wget https://www.snort.org/downloads/snort/daq-$DAQ.tar.gz
    fi

    if [ ! -f "daq-$DAQ/api/.libs/libdaq_static.a" ]; then
      tar zxf daq-$DAQ.tar.gz
      ( cd daq-$DAQ; autoreconf -f -i; ./configure --with-libpcap-includes=$TPWD/libpcap-$PCAP/ --with-libpcap-libraries=$TPWD/libpcap-$PCAP; make; sudo make install)
      if [ $? -ne 0 ]; then
        echo "ARKIME: $MAKE failed"
        exit 1
      fi
    else
      echo "ARKIME: Not rebuilding daq"
    fi
  fi

  # zstd
  if [ $BUILDZSTD -eq 1 ]; then
    WITHZSTD="--with-zstd=thirdparty/zstd-$ZSTD"
    if [ ! -f "zstd-$ZSTD.tar.gz" ]; then
      wget https://github.com/facebook/zstd/releases/download/v$ZSTD/zstd-$ZSTD.tar.gz
    fi

    if [ ! -f "zstd-$ZSTD/lib/libzstd.a" ]; then
      tar zxf zstd-$ZSTD.tar.gz
      ( cd zstd-$ZSTD; $MAKE)
      if [ $? -ne 0 ]; then
        echo "ARKIME: $MAKE failed"
        exit 1
      fi
    else
      echo "ARKIME: Not rebuilding zstd"
    fi
  else
    WITHZSTD=""
  fi

  # kafka
  if [ $BUILDKAFKA -eq 1 ]; then
    if [ ! -f "librdkafka-$KAFKA.tar.gz" ]; then
      wget https://github.com/edenhill/librdkafka/archive/v$KAFKA.tar.gz -O librdkafka-$KAFKA.tar.gz
    fi
    if [ ! -f "librdkafka-$KAFKA/src/librdkafka.a" ]; then
      tar zxf librdkafka-$KAFKA.tar.gz
      echo "ARKIME: Building librddkafka";
      (cd librdkafka-$KAFKA; ./configure --disable-sasl --install-deps; $MAKE)
      if [ $? -ne 0 ]; then
        echo "ARKIME: $MAKE failed"
        exit 1
      fi
    else
      echo "ARKIME: NOT rebuilding librdkafka";
    fi
    rm -f $TPWD/librdkafka-$KAFKA/src/*.so
    KAFKALIBDIR=$TPWD/librdkafka-$KAFKA
    KAFKABUILD="--with-kafka=$KAFKALIBDIR"
  fi

  # Now build arkime
  echo "ARKIME: Building capture"
  cd ..
  echo "./configure --prefix=$TDIR $PCAPBUILD --with-yara=thirdparty/yara/yara-$YARA --with-maxminddb=thirdparty/libmaxminddb-$MAXMIND $WITHGLIB $WITHCURL --with-nghttp2=thirdparty/nghttp2-$NGHTTP2 --with-lua=thirdparty/lua-$LUA $WITHZSTD $KAFKABUILD"
        ./configure --prefix=$TDIR $PCAPBUILD --with-yara=thirdparty/yara/yara-$YARA --with-maxminddb=thirdparty/libmaxminddb-$MAXMIND $WITHGLIB $WITHCURL --with-nghttp2=thirdparty/nghttp2-$NGHTTP2 --with-lua=thirdparty/lua-$LUA $WITHZSTD $KAFKABUILD
fi

if [ $DOCLEAN -eq 1 ]; then
    $MAKE clean
fi

$MAKE
if [ $? -ne 0 ]; then
  echo "ARKIME: $MAKE failed"
  exit 1
fi

./capture/capture --version

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

if [ $DOKAFKA -eq 1 ]; then
    (cd capture/plugins/kafka; $MAKE)
    if [ $? -ne 0 ]; then
      echo "ARKIME: Kafka plugin failed"
      exit 1
    fi
fi

# Remove old install dir
if [ $DORMINSTALL -eq 1 ]; then
    rm -rf $TDIR
fi

# Install node if not already there
case "$(uname -m)" in
    "x86_64")
        ARCH="x64"
        ;;
    "aarch64")
        ARCH="arm64"
        ;;
esac

if [ $DONODE -eq 1 ] && [ ! -f "$TDIR/bin/node" ]; then
    echo "ARKIME: Installing node $NODE"
    sudo mkdir -p $TDIR/bin $TDIR/etc
    if [ ! -f node-v$NODE-linux-x64.tar.xz ] ; then
        wget https://nodejs.org/download/release/v$NODE/node-v$NODE-linux-$ARCH.tar.xz
    fi
    sudo tar xfC node-v$NODE-linux-$ARCH.tar.xz $TDIR
    (cd $TDIR/bin ; sudo ln -sf ../node-v$NODE-linux-$ARCH/bin/* .)
fi

echo
if [ $DOINSTALL -eq 1 ]; then
    sudo env "PATH=$TDIR/bin:$PATH" make install
    echo "ARKIME: Installed, now type sudo make config'"
elif [ "$UNAME" = "Darwin" ]; then
    echo "ARKIME: On Macs we don't recommend installing, instead use 'make check' to update node deps and run regression tests"
else
    echo "ARKIME: Now type 'sudo make install' and 'sudo make config'"
fi


exit 0
