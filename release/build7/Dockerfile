FROM centos:7.6.1810
MAINTAINER Andy Wick <andy.wick@oath.com>

RUN \
yum -y update; \
yum -y install centos-release-scl scl-utils centos-release-scl-rh wget; \
yum -y update; \
yum -y install git sudo perl-Test-Base perl-Test-Differences perl-Test-Simple bzip2 wget ruby-devel rubygems rpm-build python-devel pango which; \
(cd /etc/yum.repos.d/; wget https://packages.ntop.org/centos-stable/ntop.repo); \
yum -y update; \
yum -y install devtoolset-9-gcc-c++ pfring rh-python36; \
git clone https://github.com/aol/moloch; \
(cd moloch ; scl enable devtoolset-9 rh-python36 './easybutton-build.sh --daq --pfring'); \
mv moloch/thirdparty /; \
rm -rf moloch; \
gem install --no-ri --no-rdoc fpm; \
curl https://bootstrap.pypa.io/get-pip.py | python; \
pip install awscli; \
useradd -u 2000 elasticsearch; \
(cd / ; \
 wget https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-7.5.1-linux-x86_64.tar.gz; \
 tar xf elasticsearch-7.5.1-linux-x86_64.tar.gz; \
 chown -R elasticsearch elasticsearch-7.5.1; \
 rm -f elasticsearch-7.5.1-linux-x86_64.tar.gz \
); \
yum -y clean all; \
rm -rf /var/cache/yum
