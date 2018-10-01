FROM centos:7
MAINTAINER Andy Wick <andy.wick@oath.com>

RUN \
yum -y update; \
yum -y install centos-release-scl scl-utils centos-release-scl-rh wget; \
yum -y update; \
yum -y install git sudo perl-Test-Base perl-Test-Differences perl-Test-Simple bzip2 wget ruby-devel rubygems rpm-build python-devel pango libXcomposite gtk3 gdk2 atk libXtst GConf2 libXScrnSaver alsa-lib ; \
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
yum -y clean all; \
rm -rf /var/cache/yum
