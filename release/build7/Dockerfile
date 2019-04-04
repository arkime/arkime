FROM centos:7
MAINTAINER Andy Wick <andy.wick@oath.com>

RUN \
yum -y update; \
yum -y install centos-release-scl scl-utils centos-release-scl-rh wget; \
yum -y update; \
yum -y install git sudo perl-Test-Base perl-Test-Differences perl-Test-Simple bzip2 wget ruby-devel rubygems rpm-build python-devel pango libXcomposite gtk3 gdk2 atk libXtst GConf2 libXScrnSaver alsa-lib java-1.8.0-openjdk-headless which; \
(cd /etc/yum.repos.d/; wget http://packages.ntop.org/centos-stable/ntop.repo); \
yum -y update; \
yum -y install devtoolset-6-gcc-c++ pfring; \
git clone https://github.com/aol/moloch; \
(cd moloch ; scl enable devtoolset-6 './easybutton-build.sh --daq --pfring'); \
mv moloch/thirdparty /; \
rm -rf moloch; \
gem install --no-ri --no-rdoc fpm; \
curl https://bootstrap.pypa.io/get-pip.py | python; \
pip install awscli; \
useradd -u 2000 elasticsearch; \
(cd / ; \
 wget https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-oss-6.7.0.tar.gz; \
 tar xf elasticsearch-oss-6.7.0.tar.gz; \
 chown -R elasticsearch elasticsearch-6.3.1; \
 rm -f elasticsearch-oss-6.7.0.tar.gz \
); \
yum -y clean all; \
rm -rf /var/cache/yum
