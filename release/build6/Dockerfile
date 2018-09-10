FROM centos:6
MAINTAINER Andy Wick <andy.wick@oath.com>

RUN yum -y update; \
yum -y install centos-release-scl scl-utils centos-release-scl-rh wget; \
(cd /etc/yum.repos.d/; wget http://packages.ntop.org/centos-stable/ntop.repo); \
yum -y update; \
yum -y remove kernel-headers; \
yum -y install http://files.molo.ch/kernel-ml-headers-4.4.0-1.el6.elrepo.x86_64.rpm; \
yum -y install devtoolset-6-gcc-c++ git sudo perl-Test-Base perl-Test-Differences perl-Test-Simple bzip2 wget rh-ruby23-ruby-devel rh-ruby23-rubygems rpm-build python27-python-devel python27-python-pip pfring; \
git clone https://github.com/aol/moloch; \
(cd moloch ; scl enable devtoolset-6 python27 './easybutton-build.sh --daq --pfring'); \
mv moloch/thirdparty /; \
rm -rf moloch; \
scl enable rh-ruby23 "gem install --no-ri --no-rdoc fpm"; \
scl enable python27 "pip install awscli"; \
yum -y clean all; \
rm -rf /var/cache/yum
