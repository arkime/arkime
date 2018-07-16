FROM ubuntu:14.04
MAINTAINER Andy Wick <andy.wick@oath.com>

RUN apt-get update; \
apt-get install -y ruby-dev make python-pip git libtest-differences-perl sudo libyaml-dev python-dev wget lsb-release; \
(cd /tmp; wget http://apt-stable.ntop.org/14.04/all/apt-ntop-stable.deb; dpkg -i apt-ntop-stable.deb); \
apt-get update; \
apt-get install -y pfring; \
pip install awscli; \
gem install --no-ri --no-rdoc fpm; \
git clone https://github.com/aol/moloch; (cd moloch ; ./easybutton-build.sh --daq --pfring); mv moloch/thirdparty /; rm -rf moloch; \
rm -rf /var/lib/apt/lists/*
