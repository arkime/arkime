FROM oraclelinux:8
MAINTAINER Andy Wick <andy.wick@oath.com>

RUN \
yum -y update && \
yum -y install git sudo perl-Test perl-Test-Simple bzip2 wget ruby-devel rubygems rpm-build pango which make python3-pip gcc gcc-c++ perl-Test-Harness && \
(cd /etc/yum.repos.d/; wget https://packages.ntop.org/centos/ntop.repo) && \
rpm -ivh https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm && \
rpm -ivh http://rpms.remirepo.net/enterprise/remi-release-8.rpm && \
yum -y install dnf-plugins-core && \
dnf config-manager --set-enabled ol8_codeready_builder && \
dnf config-manager --set-enabled remi && \
yum -y clean all && \
yum -y install pfring && \
git clone https://github.com/arkime/arkime && \
(cd arkime; git checkout main; ./easybutton-build.sh --daq --pfring) && \
mv arkime/thirdparty / && \
rm -rf /thirdparty/glib*/*/tests /thirdparty/glib*/*/*/tests && \
rm -rf arkime && \
gem install --no-ri --no-rdoc fpm && \
pip3 install awscli && \
yum -y clean all && \
rm -rf /var/cache/yum
