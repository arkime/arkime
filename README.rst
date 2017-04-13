.. contents::
    :local:
    :depth: 2
    
**Current Moloch users, please fill out the** `Moloch User Survey <https://docs.google.com/forms/d/1weLp8P18IUgVBSJ5saU3ee5cJhXksRg6XYtDxMQLirY/viewform>`_.

What is Moloch?
===============

Moloch is an open source, large scale, full packet capturing, indexing, and database system. Moloch augments your current security infrastructure to store and index network traffic in standard PCAP format, providing fast, indexed access. An intuitive and simple web interface is provided for PCAP browsing, searching, and exporting. Moloch exposes APIs which allow for PCAP data and JSON formatted session data to be downloaded and consumed directly. Moloch stores and exports all packets in standard PCAP format allow you to also use your favorite PCAP ingesting tools, such as wireshark, during your analysis workflow.

Access to Moloch is protected by using HTTPS with digest passwords or by using an authentication providing web server proxy. All PCAPs are stored on the sensors and are only accessed using the Moloch interface or API. Moloch is not meant to replace an IDS but instead work along side them to store and index all the network traffic in standard PCAP format, providing fast access.  Moloch is built to be deployed across many systems and can scale to handle tens of gigabits/sec of traffic. PCAP retention is based on available sensor disk space. Meta data retention is based on the Elasticsearch cluster scale. Both can be increased at anytime and are under your complete control.


Sessions Tab

.. image:: https://raw.github.com/wiki/aol/moloch/sessions.png
    :width: 300px
    :align: center
    :alt: Sample sessions screen shot


SPI View Tab

.. image:: https://raw.github.com/wiki/aol/moloch/spiview.png
    :width: 300px
    :align: center
    :alt: Sample spiview screen shot

.. _downloads:
RPM & DEB Downloads
===================

Starting with Moloch 0.15 we are now offering prebuilt RPMs and DEBs.  Follow the directions in
`/data/moloch/README.txt <https://raw.githubusercontent.com/aol/moloch/master/release/README.txt>`_
after installing.


http://molo.ch/#downloads

.. _quick-start:

Building
========

If you wish to build Moloch yourself run ``./easybutton-build.sh`` which will download all the prerequisites and build.  ``make install`` and ```make config`` can be used to install and configure moloch.

.. _components:

Components
==========

The Moloch system is comprised of 3 components

1. ``capture`` - A threaded C application that monitors network traffic, writes PCAP formatted files to disk, parses the captured packets and sends meta data (SPI data) to elasticsearch.

2. ``viewer`` - A `node.js <http://nodejs.org/>`_ application that runs per capture machine and handles the web interface and transfer of PCAP files.

3. ``elasticsearch`` - The search database technology powering Moloch.

.. _install:

Building and Installing
=======================

Moloch is a complex system to build and install manually. The following are rough guidelines.

.. _install-elasticsearch:

Installing Elasticsearch
------------------------

Recommended version **2.4.4** for Moloch 0.17 and **5.2** for Moloch 0.18 and later.

1. Prep the ``elasticsearch`` machines by increasing max file descriptors and allowing memory locking. 
   On CentOS and others this is done by adding the following to bottom of: 
   ``/etc/security/limits.conf``::

    *                -      nofile          128000
    *                -      memlock         unlimited

2. If this is a dedicated machine, disable swap by commenting out the ``swap`` lines in ``/etc/fstab`` and either reboot or use the ``swapoff`` command.

3. `Download elasticsearch <https://www.elastic.co/downloads/elasticsearch>`_.
   **Important:** At this time all development is done with `elasticsearch
   5.2.2 <https://www.elastic.co/downloads/past-releases/elasticsearch-5-2-2>`_.

4. Uncompress the archive you downloaded.

5. Create or modify ``elasticsearch.yml`` and push it to all machines. (See ``db/elasticsearch.yml.sample`` in the Moloch source distribution for an example.)
   
   - set ``cluster.name`` to something unique
   - set ``node.name`` to ``${ES_HOSTNAME}``
   - set ``node.max_local_storage_nodes`` to number of nodes per machine
   - set ``index.fielddata.cache: node``
   - set ``path.data`` and ``path.logs``
   - set ``gateway.type: local``
   - set ``gateway.recover_after_nodes`` should match the number of nodes you
     will run 
   - set ``gateway.expected_nodes`` to the number of nodes you will run
   - disable ``zen.ping.multicast``
   - enable ``zen.ping.unicast`` and set the list of hosts

6. Create an ``elasticsearch`` launch script or use `one of the ones out there <https://gist.github.com/3569769>`_. (See ``db/runes.sh.sample`` in the Moloch source distribution for a simple one.)

   - Make sure you call ``ulimit -a`` first 
   - set ``ES_HEAP_SIZE=20G`` (or whatever number you are using, less then 32G) 
   - set ``JAVA_OPTS="-XX:+UseCompressedOops"`` if using real Java
   - set ``ES_HOSTNAME`` to ```hostname -s```

7. Start the cluster, waiting ~5s between starting each node to give them time to properly mesh.

8. Inside the *installed* ``$MOLOCH_PREFIX/db`` directory run the 
    ``db.pl http://A_ES_HOSTNAME:9200 init`` script.

.. _building-capture:

Building Capture
----------------

1. Install prerequisite standard packages.

   - CentOS::

        yum install wget curl pcre pcre-devel pkgconfig flex bison gcc-c++ zlib-devel e2fsprogs-devel openssl-devel file-devel make gettext libuuid-devel perl-JSON bzip2-libs bzip2-devel perl-libwww-perl libpng-devel xz libffi-devel

   - Ubuntu::
    
        apt-get install wget curl libpcre3-dev uuid-dev libmagic-dev pkg-config g++ flex bison zlib1g-dev libffi-dev gettext libgeoip-dev make libjson-perl libbz2-dev libwww-perl libpng-dev xz-utils libffi-dev

   - OS X::

        port install yara libpcap openssl pcre flex bison zlib file gettext p5-JSON p5-libwww-perl libffi xz ossp-uuid libgeoip glib2
        ./configure --with-libpcap=/opt/local --with-yara=/opt/local --with-GeoIP=/opt/local LDFLAGS=-L/opt/local/lib --with-glib2=no GLIB2_CFLAGS="-I/opt/local/include/glib-2.0 -I/opt/local/lib/glib-2.0/include" GLIB2_LIBS="-L/opt/local/lib -lglib-2.0 -lgmodule-2.0 -lgobject-2.0 -lgio-2.0"

2. Building ``capture`` can be a pain because of OS versions.

   - Try ``./easybutton-build.sh`` which will download all the following, compile them statically, and run the local configure script.
   - Or if you want build yourself, or use some already installed packages then here are the pieces you need:

     + `glib-2 <http://ftp.gnome.org/pub/gnome/sources/glib>`_ version 2.40 or
       higher (2.50.2 is recommended)::

            wget http://ftp.gnome.org/pub/gnome/sources/glib/2.50/glib-2.50.2.tar.xz
            ./configure --disable-xattr --disable-shared --enable-static --disable-libelf --disable-selinux --disable-libmount --with-pcre=internal

     + `yara <https://github.com/VirusTotal/yara>`_ version 1.6 or higher::

            wget https://github.com/VirusTotal/yara/archive/v3.5.0.tar.gz -O yara-3.5.0.tar.gz
            ./configure --enable-static

     + `MaxMind GeoIP <http://www.maxmind.com/app/c>`_ - The OS version may be recent enough::
            wget http://www.maxmind.com/download/geoip/api/c/GeoIP-1.6.9.tar.gz
            libtoolize -f # Only some platforms need this
            ./configure --enable-static

     + `libpcap <http://www.tcpdump.org/#latest-release>`_ - version 1.3 or higher (most OS versions are older)::
       
             wget http://www.tcpdump.org/release/libpcap-1.7.4.tar.gz
             ./configure --disable-dbus

3. Run ``configure``. Optionally use the ``--with-<foo>`` directives to use static libraries from build directories.

4. Run ``make``.

.. _building-viewer:

Building Viewer
---------------
1. Install `Node.js <http://nodejs.org/>`_ version 4.6.0, currently 6.x is not supported.  (Moloch versions before 0.16 required 0.10.x)

2. In the ``viewer`` directory run ``npm update``.

.. _configuration:

Configuration
-------------

1. Make sure you download the latest freely available GeoIP and RIR files. 

   - `GeoLiteCountry <http://geolite.maxmind.com/download/geoip/database/GeoLiteCountry/GeoIP.dat.gz>`_ - Geographic IP data
   - `GeoIPASNum <http://www.maxmind.com/download/geoip/database/asnum/GeoIPASNum.dat.gz>`_ - Geographic Autonomous System (AS) number data
   - `ipv4-address-space <https://www.iana.org/assignments/ipv4-address-space/ipv4-address-space.csv>`_ - RIR assignments   

2. Edit the ``config.ini`` file.
   
3. In the ``viewer`` directory, run ``addUser.js`` to add users. Pass the ``--admin`` flag if you want admin users that can edit users from the web site. This is a good test if ``elasticsearch`` and ``config.ini`` are setup correctly::

    node addUser.js <userid> "<Friendly Name>" <password> [--admin]

4. Edit the ``db/daily.sh`` script, and set it up in the crontab on one
   machine.

.. _running:

Get it Running
--------------

If you've made it this far, you are awesome!

On each ``capture`` machine you need to run at least one ``moloch-capture`` and one ``moloch-viewer``. Using ``make config`` will create startup files, or you can find the source files for ``make config`` in the release directory.

.. _test:

Test it Out
-----------

Point your browser to any Moloch instance at ``https://<hostname>:<port>`` and start tinkering!

.. _advanced:

Advanced Configuration
======================

.. _hardware-reqs:

Hardware Requirements
---------------------

Moloch is built to run across many machines for large deployments.  For demo, small network, or home installations everything on a single machine is fine.

For larger installations please see the FAQ for recomended configurations.



The following are rough guidelines for capturing large amounts of data with high bit rates, obviously tailor for your specific situation.  It is not recommended to run the ``capture`` and ``elasticsearch``  processes on the same machines for highly utilized GigE networks.


1. Moloch ``capture``/``viewer`` systems read `FAQ Entry <https://github.com/aol/moloch/wiki/FAQ#What_kind_of_capture_machines_should_we_buy>`_

2. Moloch ``elasticsearch`` systems read `FAQ Entry <https://github.com/aol/moloch/wiki/FAQ#How_many_elasticsearch_nodes_or_machines_do_I_need>`_

Example Configuration
~~~~~~~~~~~~~~~~~~~~~

Here is an example system setup for monitoring 8x GigE highly-utilized networks, with an average of ~5 Gigabit/sec, with ~7 days of pcap storage.

* ``capture``/``viewer`` machines
 
  - 5x HP Apollo 4200
  - 64GB of memory 
  - 80TB of disk
  - Running Moloch and `Suricata <http://suricata-ids.org/>`_

* ``elasticsearch`` machines

  - 10x HP DL380-G7
  - 128GB of memory
  - 6TB of disk
  - Each system running 1 node

.. _security:

Security Information
====================

.. _security-ports:

Ports Used
----------

* tcp 8005 - Moloch web interface
* tcp 9200-920x (configurable upper limit) - Elasticsearch service ports
* tcp 9300-930x (configurable upper limit) - Elasticsearch mesh connections

.. _security-tips:

Important Considerations
------------------------

* Elasticsearch provides NO security, so ``iptables`` MUST be used allowing only Moloch machines to talk to the ``elasticsearch`` machines (ports 9200-920x) and for them to mesh connect (ports 9300-930x).  An example with 3 ES machines 2 nodes each and a viewer only machine::
    for ip in moloches1 moloches2 moloches3 molochvieweronly1; do
      iptables -A INPUT -i eth0 -p tcp --dport 9300 -s $ip -j ACCEPT
      iptables -A INPUT -i eth0 -p tcp --dport 9200 -s $ip -j ACCEPT
      iptables -A INPUT -i eth0 -p tcp --dport 9301 -s $ip -j ACCEPT
      iptables -A INPUT -i eth0 -p tcp --dport 9201 -s $ip -j ACCEPT
    done
    iptables -A INPUT -i eth0 -p tcp --dport 9300 -j DROP
    iptables -A INPUT -i eth0 -p tcp --dport 9200 -j DROP
    iptables -A INPUT -i eth0 -p tcp --dport 9301 -j DROP
    iptables -A INPUT -i eth0 -p tcp --dport 9201 -j DROP
  
* Moloch machines should be locked down, however they need to talk to each other (port 8005), to the elasticsearch machines (ports 9200-920x), and the web interface needs to be open (port 8005).
* Moloch ``viewer`` should be configured to use SSL.

  - It's easiest to use a single certificate with multiple DNs.
  - Make sure you protect the cert on the filesystem with proper file permissions.

* It is possible to set up a Moloch ``viewer`` on a machine that doesn't capture any data that gateways all requests.

  - It is also possible to place apache in front of moloch, so it can handle the authentication and pass the username on to moloch
  - This is how we deploy it

* A shared password stored in the Moloch configuration file is used to encrypt password hashes AND for inter-Moloch communication. 

  - Make sure you protect the config file on the filesystem with proper file permissions.
  - Encrypted password hashes are used so a new password hash can not be inserted into ``elasticsearch`` directly in case it hasn't been secured.

.. _documentation:

Documentation
=============

For now this README is the bulk of the documentation. This will improve over
time. 

.. _faq:

FAQ
---

For answers to frequently asked questions, please see the `FAQ <https://github.com/aol/moloch/wiki/FAQ>`_.

.. _wiki:

Wiki
----

We use GitHub’s built-in wiki located at `https://github.com/aol/moloch/wiki <https://github.com/aol/moloch/wiki>`_.

.. _upgrading:

Upgrading
=========

Currently upgrading from previous versions of Moloch is a manual process, however recorded sessions and pcap files should be retained

* Update the moloch repository from github
* Build the moloch system using "easybutton-build.sh"
* Shut down currently running old capture and viewer processes
* Optionally use "make install" to copy the new binaries and other items and/or push the new items to the capture hosts
* Run "npm update" in the viewer directory if not using "make install"
* Make sure ES is running and update the database using the "db/db.pl host:port upgrade" script
* Start the new capture and viewer processes

