Instructions for using the prebuilt Arkime packages.
Please report any bugs or feature requests by opening an issue at https://github.com/arkime/arkime/issues

Basic Arkime Installation steps:
 1) Download an Arkime build for your OS from https://arkime.com/downloads
 2) Install the downloaded package
 3) Configure basic arkime items by running the Configure script (this needs to be done only once)
     /opt/arkime/bin/Configure
 4) The Configure script can install OpenSearch/Elasticsearch for you or you can install yourself
 5) Initialize/Upgrade OpenSearch/Elasticsearch Arkime configuration
  a) If this is the first install, or want to delete all data
      /opt/arkime/db/db.pl http://ESHOST:9200 init
  b) If this is an update to an Arkime package
      /opt/arkime/db/db.pl http://ESHOST:9200 upgrade
 6) Add an admin user if a new install or after an init
      /opt/arkime/bin/arkime_add_user.sh admin "Admin User" THEPASSWORD --admin
 7) Start everything
      systemctl start arkimecapture.service
      systemctl start arkimeviewer.service
 8) Look at log files for errors
      /opt/arkime/logs/viewer.log
      /opt/arkime/logs/capture.log
 9) Visit http://arkimeHOST:8005 with your favorite browser.
      user: admin
      password: THEPASSWORD from step #6

If you want IP -> Geo/ASN to work, you need to setup a maxmind account and the geoipupdate program.
See https://arkime.com/faq#maxmind

Any configuration changes can be made to /opt/arkime/etc/config.ini
See https://arkime.com/faq#arkime-is-not-working for issues

Additional information can be found at:
  * https://arkime.com/faq
  * https://arkime.com/settings
