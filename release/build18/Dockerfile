FROM ubuntu:18.04
MAINTAINER Andy Wick <andy.wick@oath.com>

RUN apt-get update && \
apt-get install -y lsb-release ruby-dev make python-pip git libtest-differences-perl sudo wget && \
(cd /tmp && wget http://apt-stable.ntop.org/14.04/all/apt-ntop-stable.deb && dpkg -i apt-ntop-stable.deb) && \
apt-get update && \
apt-get install -y pfring && \
gem install --no-ri --no-rdoc fpm && \
pip install awscli && \
git clone https://github.com/aol/moloch && \
(cd moloch ; ./easybutton-build.sh --daq --pfring) && \
mv moloch/thirdparty / && \
rm -rf moloch && \
rm -rf /var/lib/apt/lists/*
