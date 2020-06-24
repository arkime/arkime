Instructions for using the prebuilt Moloch packages.
Please report any bugs or feature requests by opening an issue at https://github.com/aol/moloch/issues

Basic Moloch Installation steps:
 1) Download a Moloch build for 64bit Ubuntu 14.04, Ubuntu 16.04, Centos 6, or Centos 7 from http://molo.ch/index.html#downloads
 2) Install package
 3) Configure basic moloch items by running the Configure script (this needs to be done only once)
     moloch/release/Configure
 4) The Configure script can install elasticsearch for you or you can install yourself
      /sbin/start elasticsearch # for upstart/Centos 6/Ubuntu 14.04
      sudo systemctl start elasticsearch.service # for systemd/Centos 7/Ubuntu 16.04
 5) Initialize/Upgrade Elasticsearch Moloch configuration
  a) If this is the first install, or want to delete all data
      moloch/db/db.pl http://ESHOST:9200 init
  b) If this is an update to moloch package
      moloch/db/db.pl http://ESHOST:9200 upgrade
 6) Add an admin user if a new install or after an init
      ./moloch/release/moloch_add_user.sh admin "Admin User" THEPASSWORD --admin
 7) Start everything
   a) If using upstart (Centos 6 or sometimes Ubuntu 14.04):
      /sbin/start molochcapture
      /sbin/start molochviewer
   b) If using systemd (Centos 7 or Ubuntu 16.04 or sometimes Ubuntu 14.04)
      sudo systemctl start molochcapture.service
      sudo systemctl start molochviewer.service
 8) Look at log files for errors
      moloch/logs/viewer.log
      moloch/logs/capture.log
 9) Visit http://MOLOCHHOST:8005 with your favorite browser.
      user: admin
      password: THEPASSWORD from step #6

If you want IP -> Geo/ASN to work, you need to setup a maxmind account and the geoipupdate program.
See https://molo.ch/faq#maxmind

Any configuration changes can be made to /data/moloch/etc/config.ini
See https://molo.ch/faq#moloch-is-not-working for issues

Additional information can be found at:
  * https://molo.ch/faq
  * https://molo.ch/settings
