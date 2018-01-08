FROM ubuntu:16.04
MAINTAINER Andy Wick <andy.wick@oath.com>

RUN apt-get update; \
apt-get install -y ruby-dev make python-pip git libtest-differences-perl sudo; \
gem install --no-ri --no-rdoc fpm; \
pip install awscli; \
git clone https://github.com/aol/moloch; (cd moloch ; ./easybutton-build.sh); mv moloch/thirdparty /; rm -rf moloch; \
rm -rf /var/lib/apt/lists/*
