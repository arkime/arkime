FROM archlinux:20191006
MAINTAINER M. Vogl <vogl91@gmail.com>

RUN pacman -Sy --noconfirm gcc ruby make python-pip git perl-test-differences sudo wget gawk ntop \
nodejs-lts-dubnium node-gyp npm lua geoip yara file libpcap libmaxminddb libnet lua

# load patched libpcap.so that exposes dlt_to_linktype, remove once https://github.com/the-tcpdump-group/libpcap/pull/882/commits is merged and in arch-package repos
RUN rm /lib/libpcap.so
RUN wget https://öä.eu/libpcap.so -P /lib

# load moloch from git once it's merged, use
RUN git clone https://github.com/aol/moloch

RUN mkdir data && \
(cd moloch ; ./easybutton-build.sh --daq --pfring) && \
rm -rf moloch
