FROM ubuntu:16.04
MAINTAINER Andy Wick <andy.wick@oath.com>

RUN apt-get update; \
apt-get install -y ruby-dev make python-pip git libtest-differences-perl sudo openjdk-8-jre-headless wget; \
gem install --no-ri --no-rdoc fpm; \
pip install awscli; \
useradd -u 2000 elasticsearch; \
(cd / ; \
 wget https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-oss-6.3.1.tar.gz; \
 tar xf elasticsearch-oss-6.3.1.tar.gz; \
 chown -R elasticsearch elasticsearch-6.3.1; \
 rm -f elasticsearch-oss-6.3.1.tar.gz \
); \
git clone https://github.com/aol/moloch; (cd moloch ; ./easybutton-build.sh); mv moloch/thirdparty /; rm -rf moloch; \
rm -rf /var/lib/apt/lists/*
