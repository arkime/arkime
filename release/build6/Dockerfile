FROM centos:6
MAINTAINER Andy Wick <andy.wick@oath.com>

RUN yum -y update; \
yum -y install centos-release-scl scl-utils centos-release-scl-rh; \
yum -y update; \
yum -y install devtoolset-3-gcc-c++ git sudo perl-Test-Base perl-Test-Differences perl-Test-Simple bzip2 wget ruby193-ruby-devel ruby193-rubygems rpm-build python27-python-devel python27-python-pip; \
git clone https://github.com/aol/moloch; \
(cd moloch ; scl enable devtoolset-3 python27 ./easybutton-build.sh); \
mv moloch/thirdparty /; \
rm -rf moloch; \
scl enable ruby193 "gem install --no-ri --no-rdoc fpm"; \
scl enable python27 "pip install awscli"; \
yum -y clean all; \
rm -rf /var/cache/yum
