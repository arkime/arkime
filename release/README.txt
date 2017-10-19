Instructions for using the prebuilt Moloch packages.
Please report any bugs or feature requests by opening an issue at https://github.com/aol/moloch/issues

Basic Moloch Installation steps:
 1) Download a Moloch build for 64bit Ubuntu 14.04, Ubuntu 16.04, Centos 6, or Centos 7 from http://molo.ch/index.html#downloads
 2) Install package
 3) Configure basic moloch items by running the Configure script (this needs to be done only once)
     /data/moloch/bin/Configure
 4a) If NOT using the demo Elasticsearch, download, install, start elasticsearch, > 5.4.x recommended
      https://www.elastic.co/downloads/past-releases
 4b) If using the demo Elasticsearch, these won't work with real Elasticsearch installs
      /sbin/start elasticsearch # for upstart/Centos 6/Ubuntu 14.04
      systemctl start elasticsearch.service # for systemd/Centos 7/Ubuntu 16.04
 5) Initialize/Upgrade Elasticsearch Moloch configuration
  a) If this is the first install, or want to delete all data
      /data/moloch/db/db.pl http://ESHOST:9200 init
  b) If this is an update to moloch package
      /data/moloch/db/db.pl http://ESHOST:9200 upgrade
 6) Add an admin user if a new install or after an init
      /data/moloch/bin/moloch_add_user.sh admin "Admin User" THEPASSWORD --admin
 7) Start everything
   a) If using upstart (Centos 6 or sometimes Ubuntu 14.04):
      /sbin/start molochcapture
      /sbin/start molochviewer
   b) If using systemd (Centos 7 or Ubuntu 16.04 or sometimes Ubuntu 14.04)
      systemctl start molochcapture.service
      systemctl start molochviewer.service
 8) Look at log files for errors
      /data/moloch/logs/viewer.log
      /data/moloch/logs/capture.log
 9) Visit http://molochhost:8005 with your favorite browser.
      user: admin
      password: password from step #6


Additional information can be found at:
  * https://github.com/aol/moloch/wiki/FAQ
  * https://github.com/aol/moloch/wiki/Settings
