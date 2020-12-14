FROM ubuntu:16.04
MAINTAINER Andy Wick <andy.wick@oath.com>

RUN apt-get update && \
apt-get install -y ruby-dev make python3-pip git libtest-differences-perl sudo wget libjson-pp-perl tzdata wget lsb-release apt-transport-https && \
(cd /tmp; wget https://packages.ntop.org/apt-stable/16.04/all/apt-ntop-stable.deb; dpkg -i apt-ntop-stable.deb) && \
apt-get update && \
apt-get install -y pfring && \
apt-get -f install -y && \
gem install --no-ri --no-rdoc fpm; \
pip3 install awscli; \
useradd -u 2000 elasticsearch; \
(cd / ; \
 wget https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-7.9.3-linux-x86_64.tar.gz; \
 tar xf elasticsearch-7.9.3-linux-x86_64.tar.gz; \
 chown -R elasticsearch elasticsearch-7.9.3; \
 rm -f elasticsearch-7.9.3-linux-x86_64.tar.gz \
) && \
git clone https://github.com/arkime/arkime && \
apt-get -f install -y && \
(cd arkime; git checkout master; ./easybutton-build.sh --daq --pfring) && \
mv arkime/thirdparty / && \
rm -rf /thirdparty/glib*/*/tests /thirdparty/glib*/*/*/tests && \
rm -rf arkime && \
rm -rf /var/lib/apt/lists/*
