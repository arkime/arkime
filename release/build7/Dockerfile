FROM --platform=linux/amd64 centos:7.9.2009
MAINTAINER Andy Wick <andy.wick@oath.com>
ARG ARKIME_BRANCH=main

RUN \
yum -y update; \
yum -y install centos-release-scl scl-utils centos-release-scl-rh wget; \
yum -y update; \
yum -y install git sudo perl-Test-Base perl-Test-Differences perl-Test-Simple bzip2 wget rh-ruby26-ruby-devel rh-ruby26-ruby rpm-build pango which; \
(cd /etc/yum.repos.d/; wget https://packages.ntop.org/centos-stable/ntop.repo); \
yum -y update; \
yum -y install devtoolset-9-gcc-c++ pfring rh-python36; \
git clone https://github.com/arkime/arkime; \
(cd arkime; git checkout $ARKIME_BRANCH; scl enable devtoolset-9 rh-python36 './easybutton-build.sh --daq --pfring --kafka'); \
mv arkime/thirdparty /; \
rm -rf /thirdparty/glib*/*/tests /thirdparty/glib*/*/*/tests && \
rm -rf arkime; \
scl enable rh-ruby26 'gem install fpm'; \
scl enable devtoolset-9 rh-python36 'pip3 install awscli'; \
useradd -u 2000 opensearch; \
(cd / ; \
 wget https://artifacts.opensearch.org/releases/core/opensearch/2.3.0/opensearch-min-2.3.0-linux-x64.tar.gz; \
 tar xf opensearch-min-2.3.0-linux-x64.tar.gz; \
 chown -R opensearch opensearch-2.3.0; \
 rm -f opensearch-min-2.3.0-linux-x64.tar.gz \
); \
yum -y clean all; \
rm -rf /var/cache/yum
