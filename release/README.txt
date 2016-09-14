Instructions for using the prebuilt Moloch packages.  These packages are still 
in testing phase, so there may be some issues.  Please report and bugs
or feature requests by opening an issue at https://github.com/aol/moloch/issues

Basic Moloch Installation steps:
* Install elasticsearch 2.4 somewhere (usually a different machine) and start
* Download a build for 64bit Ubuntu 14.04, 16.04 or Centos 6, 7 from http://molo.ch/index.html#downloads
* Install package
* cp /data/moloch/etc/config.ini.sample to /data/moloch/etc/config.ini
* edit /data/moloch/etc/config.ini and search for multiple CHANGEME lines
   * elasticsearch=   to the url of your elasticsearch server
   * passwordSecret=  the password for encrypting things in ES
   * interface=       to the interface to listen on
* Fetch the initial geo lookup files by running
    /data/moloch/bin/moloch_update_geo.sh
* create the upstart jobs
    ln -s /data/moloch/etc/molochcapture.upstart.conf /etc/init/molochcapture.conf
    ln -s /data/moloch/etc/molochviewer.upstart.conf /etc/init/molochviewer.conf
* Initialize ES once
    /data/moloch/db/db.pl http://ESHOST:ESPORT init
* Start moloch capture and moloch viewer
    start molochcapture
    start molochviewer
* Visit http://molochhost:8005 with your favorite browser.


Additional information can be found
* https://github.com/aol/moloch/wiki/FAQ
* https://github.com/aol/moloch/wiki/Settings

