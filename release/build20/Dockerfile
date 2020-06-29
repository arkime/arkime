FROM ubuntu:20.04
MAINTAINER Andy Wick <andy.wick@oath.com>

RUN apt-get update && \
apt-get install -y lsb-release ruby-dev make python3-pip git libtest-differences-perl sudo wget && \
(cd /tmp && wget https://packages.ntop.org/apt-stable/20.04/all/apt-ntop-stable.deb && dpkg -i apt-ntop-stable.deb) && \
apt-get update && \
apt-get install -y pfring && \
gem install --no-document fpm && \
pip3 install awscli && \
git clone https://github.com/aol/moloch && \
(cd moloch ; ./easybutton-build.sh --daq --pfring) && \
mv moloch/thirdparty / && \
rm -rf moloch && \
rm -rf /var/lib/apt/lists/*
