Instructions for using the prebuilt Arkime packages.
Please report any bugs or feature requests by opening an issue at https://github.com/aol/moloch/issues

Basic Arkime Installation steps:
 1) Download a Arkime build for 64bit Ubuntu 14.04, Ubuntu 16.04, Centos 6, or Centos 7 from http://arkime.com/index.html#downloads
 2) Install package
 3) Configure basic moloch items by running the Configure script (this needs to be done only once)
     /data/moloch/bin/Configure
 4) The Configure script can install elasticsearch for you or you can install yourself
      systemctl start elasticsearch.service
 5) Initialize/Upgrade Elasticsearch Arkime configuration
  a) If this is the first install, or want to delete all data
      /data/moloch/db/db.pl http://ESHOST:9200 init
  b) If this is an update to moloch package
      /data/moloch/db/db.pl http://ESHOST:9200 upgrade
 6) Add an admin user if a new install or after an init
      /data/moloch/bin/moloch_add_user.sh admin "Admin User" THEPASSWORD --admin
 7) Start everything
      systemctl start molochcapture.service
      systemctl start molochviewer.service
 8) Look at log files for errors
      /data/moloch/logs/viewer.log
      /data/moloch/logs/capture.log
 9) Visit http://MOLOCHHOST:8005 with your favorite browser.
      user: admin
      password: THEPASSWORD from step #6

If you want IP -> Geo/ASN to work, you need to setup a maxmind account and the geoipupdate program.
See https://arkime.com/faq#maxmind

Any configuration changes can be made to /data/moloch/etc/config.ini
See https://arkime.com/faq#moloch-is-not-working for issues

Additional information can be found at:
  * https://arkime.com/faq
  * https://arkime.com/settings
