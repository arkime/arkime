Instructions for using the prebuilt Moloch packages.  These packages are still 
in testing phase, so there may be some issues.  Please report and bugs
or feature requests by opening an issue at https://github.com/aol/moloch/issues

Basic Moloch Installation steps:
* Install elasticsearch 2.4 (usually a different machine) and startup
* Download a Moloch build for 64bit Ubuntu 14.04, 16.04 or Centos 6, 7 from http://molo.ch/index.html#downloads
* Install package
* Initialize Elasticsearch 
    /data/moloch/db/db.pl http://ESHOST:ESPORT init
* Configure basic moloch items
    /data/moloch/bin/Configure once
* More advanced configuration can be done by editing /data/moloch/etc/config.ini
* Start moloch capture and moloch viewer
    start molochcapture
    start molochviewer
* Visit http://molochhost:8005 with your favorite browser.


Additional information can be found
* https://github.com/aol/moloch/wiki/FAQ
* https://github.com/aol/moloch/wiki/Settings

