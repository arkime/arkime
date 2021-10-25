FROM ubuntu:20.04 as build

RUN \
  export DEBIAN_FRONTEND=noninteractive && \
  apt update && \
  apt install -y lsb-release ruby-dev make python3-pip git libtest-differences-perl sudo wget apt-utils tzdata curl && \
  apt install -y wget curl libpcre3-dev uuid-dev libmagic-dev pkg-config g++ flex bison zlib1g-dev libffi-dev gettext libgeoip-dev make libjson-perl libbz2-dev libwww-perl libpng-dev xz-utils libffi-dev libssl-dev libreadline-dev libtool libyaml-dev dh-autoreconf libsocket6-perl libtest-differences-perl

# Previous line taken from easybutton-build.sh

RUN \
  curl -sL https://deb.nodesource.com/setup_14.x | sudo bash - ; \
  apt update && apt install -y nodejs

COPY . /src
WORKDIR /src

RUN ./easybutton-build.sh --nonode