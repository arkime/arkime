FROM ubuntu:18.04
MAINTAINER Andy Wick <andy.wick@oath.com>

RUN apt-get update && \
apt-get install -y lsb-release ruby-dev make python3-pip git libtest-differences-perl sudo wget && \
(cd /tmp && wget https://packages.ntop.org/apt-stable/18.04/all/apt-ntop-stable.deb && dpkg -i apt-ntop-stable.deb) && \
apt-get update && \
apt-get install -y pfring && \
gem install --no-ri --no-rdoc fpm && \
pip3 install awscli && \
git clone https://github.com/arkime/arkime && \
(cd arkime; git checkout master; ./easybutton-build.sh --daq --pfring) && \
mv arkime/thirdparty / && \
rm -rf /thirdparty/glib*/*/tests /thirdparty/glib*/*/*/tests && \
rm -rf arkime && \
rm -rf /var/lib/apt/lists/*
