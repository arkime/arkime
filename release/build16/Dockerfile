FROM ubuntu:16.04
MAINTAINER Andy Wick <andy.wick@oath.com>

RUN apt-get update && \
apt-get install -y ruby-dev make python-pip git libtest-differences-perl sudo openjdk-8-jre-headless wget libjson-pp-perl tzdata wget lsb-release && \
(cd /tmp; wget http://apt-stable.ntop.org/16.04/all/apt-ntop-stable.deb; dpkg -i apt-ntop-stable.deb) && \
apt-get update && \
apt-get install -y pfring && \
apt-get -f install && \
gem install --no-ri --no-rdoc fpm; \
pip install awscli; \
useradd -u 2000 elasticsearch; \
(cd / ; \
 wget https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-oss-6.7.0.tar.gz; \
 tar xf elasticsearch-oss-6.7.0.tar.gz; \
 chown -R elasticsearch elasticsearch-6.7.0; \
 rm -f elasticsearch-oss-6.7.0.tar.gz \
); \
git clone https://github.com/aol/moloch; (cd moloch ; ./easybutton-build.sh --daq --pfring); mv moloch/thirdparty /; rm -rf moloch; \
rm -rf /var/lib/apt/lists/*
