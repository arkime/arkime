#!/bin/sh

export PATH=/usr/local/bin:$PATH
umask 022

# Update and install any packages required for easybutton to run
if [ -f "/etc/redhat-release" ]; then
    yum -y update
    if grep -q "release 5." /etc/redhat-release; then
        # Centos 5 - Need EPEL for git and python26
        if [ ! -f /etc/yum.repos.d/epel.repo ]; then
	    rpm -Uvh http://dl.fedoraproject.org/pub/epel/5/x86_64/epel-release-5-4.noarch.rpm
        fi
	yum -y install python26
        mkdir -p /usr/local/bin
        ln -s /usr/bin/python26 /usr/local/bin/python
    fi
    yum -y install git java-1.7.0-openjdk curl psmisc perl-Test-Differences perl-TAP-Harness perl-Test-Simple
fi

if [ -f "/etc/debian_version" ]; then
    apt-get -y update
    apt-get -y upgrade
    apt-get -y install git openjdk-7-jre-headless curl libtest-differences-perl libhttp-message-perl
fi

if [ $(uname) == "FreeBSD" ]; then
    pkg_add -Fr git openjdk7 p5-libwww p5-JSON p5-Test-Differences
    export LIBRARY_PATH=/usr/local/lib
    export C_INCLUDE_PATH=/usr/local/include
fi


# Cleanup anything left around if just running script agian
curl -s -XPOST http://127.0.0.1:9200/_shutdown
/bin/rm -rf /data/moloch
killall node moloch-capture

if [ -d moloch.github ]; then
  (cd moloch.github ; git pull)
else
  git clone https://github.com/aol/moloch moloch.github
fi

# Build using easy button
cd moloch.github 
echo -e 'no\n128M\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n' | ./easybutton-singlehost.sh
sleep 1

# Easy button starts stuff, kill it :)
killall -q node moloch-capture
sleep 1


# Run the tests out of the moloch.github dir
cd tests
(cd ../capture/plugins/wiseService; npm install)
(cd ../viewer; npm install)
./tests.pl
./tests.pl --viewer
