Initial version of vagrant and ansible files that will build official releases.  We will build for 64bit Ubuntu 14.04, 16.04  and Centos 6, 7.

A user will need to
* Install package
* Copy /data/moloch/etc/config.ini.sample to /data/moloch/etc/config.ini and edit
* Link to upstart files

They will also need to create an elasticsearch cluster and run db.pl on it
