FROM almalinux:9
MAINTAINER Andy Wick <andy.wick@oath.com>

RUN \
yum -y update && \
dnf -y install 'dnf-command(config-manager)' && \
yum -y install git sudo perl-Test perl-Test-Simple bzip2 wget ruby-devel rubygems rpm-build pango which make python3-pip gcc gcc-c++ perl-Test-Harness && \
(cd /etc/yum.repos.d/; wget https://packages.ntop.org/centos/ntop.repo) && \
rpm -ivh https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm && \
/usr/bin/crb enable && \
yum -y clean all && \
yum -y install pfring && \
yum -y install --allowerasing curl && \
git clone https://github.com/arkime/arkime && \
(cd arkime; git checkout main; ./easybutton-build.sh --daq --pfring --kafka) && \
mv arkime/thirdparty / && \
rm -rf /thirdparty/glib*/*/tests /thirdparty/glib*/*/*/tests && \
rm -rf arkime && \
gem install --no-document fpm && \
pip3 install awscli && \
useradd -u 2000 elasticsearch && \
yum -y clean all && \
rm -rf /var/cache/yum
