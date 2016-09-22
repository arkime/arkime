Instructions for using the prebuilt Moloch packages.
These packages are still in the testing phase so there may be some issues.  
Please report and bugs or feature requests by opening an issue at https://github.com/aol/moloch/issues

Basic Moloch Installation steps:
 1) Download a Moloch build for 64bit Ubuntu 14.04, Ubuntu 16.04, Centos 6, or Centos 7 from http://molo.ch/index.html#downloads
 2) Install package
 3) Configure basic moloch items (only need to do once)
     /data/moloch/bin/Configure
 4) If NOT using the demo Elasticsearch, download, install, start elasticsearch 2.4.0
      http://download.elastic.co/elasticsearch/elasticsearch/elasticsearch-2.4.0.tar.gz
 5) Initialize/Upgrade Elasticsearch Moloch configuration
  a) If this is the first install, or want to delete all data
      /data/moloch/db/db.pl http://ESHOST:9200 init
  b) If this is an update to moloch package
      /data/moloch/db/db.pl http://ESHOST:9200 upgrade
 6) Add an admin user
      /data/moloch/bin/moloch_add_user.sh admin admin THEPASSWORDGOESHERE --admin
 7) Start everything
      start molochcapture
      start molochviewer
 8) Look at log files for errors
      /data/moloch/logs/viewer.log
      /data/moloch/logs/capture.log
 9) Visit http://molochhost:8005 with your favorite browser.


Additional information can be found at:
  * https://github.com/aol/moloch/wiki/FAQ
  * https://github.com/aol/moloch/wiki/Settings
