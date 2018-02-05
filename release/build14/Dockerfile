FROM ubuntu:14.04
MAINTAINER Andy Wick <andy.wick@oath.com>

RUN apt-get update; \
apt-get install -y ruby-dev make python-pip git libtest-differences-perl sudo libyaml-dev python-dev; \
pip install awscli; \
gem install --no-ri --no-rdoc fpm; \
git clone https://github.com/aol/moloch; (cd moloch ; ./easybutton-build.sh); mv moloch/thirdparty /; rm -rf moloch; \
rm -rf /var/lib/apt/lists/*
