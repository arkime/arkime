FROM centos:7 as build

MAINTAINER Benoit Perroud <benoit@sqooba.io>

ARG MOLOCH_VERSION

RUN \
  yum -y update; \
  yum -y install centos-release-scl scl-utils centos-release-scl-rh wget sudo; \
  (cd /etc/yum.repos.d/; wget https://packages.ntop.org/centos-stable/ntop.repo); \
  yum -y update; \
  curl -sL https://rpm.nodesource.com/setup_12.x | sudo bash - ;\
  yum install -y python3-pip git nodejs \
    wget curl pcre pcre-devel pkgconfig flex bison gcc-c++ zlib-devel e2fsprogs-devel openssl-devel \
    file-devel make gettext libuuid-devel perl-JSON bzip2-libs bzip2-devel perl-libwww-perl libpng-devel \
    xz libffi-devel readline-devel libtool libyaml-devel perl-Socket6 perl-Test-Base perl-Test-Differences \
    perl-Test-Simple rh-ruby23-ruby-devel rh-ruby23-ruby rpm-build pango which \
    devtoolset-9-gcc-c++ rh-python36 pfring cyrus-sasl-devel

COPY . /src
WORKDIR /src

RUN \
  scl enable devtoolset-9 rh-python36 './easybutton-build.sh --daq --pfring --kafka' && \
  echo "build ok" && \
  scl enable devtoolset-9 rh-python36 'make install'



FROM centos:7 as run

RUN mkdir -p /data

COPY --from=build /data/moloch /data/moloch

RUN \
  yum install -y cyrus-sasl libyaml \
  yum remove -y wget && \
  yum clean all && rm -rf /var/cache/yum

# "install" librdkafka
COPY --from=build /usr/local/lib/librdkafka* /usr/local/lib/
COPY --from=build /usr/local/include/librdkafka /usr/local/include/librdkafka

COPY ulimit-entrypoint.sh /ulimit-entrypoint.sh

ENTRYPOINT ["/ulimit-entrypoint.sh"]
