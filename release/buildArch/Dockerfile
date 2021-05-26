FROM archlinux:latest
MAINTAINER Andy Wick <andy.wick@oath.com>

RUN pacman -Sy --noconfirm gcc ruby make python-pip git perl-test-differences sudo wget gawk \
lua geoip yara file libpcap libmaxminddb libnet lua libtool autoconf gettext automake perl-http-message perl-lwp-protocol-https perl-json perl-socket6

RUN gem install --no-document fpm rexml
RUN pip3 install awscli
