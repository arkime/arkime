.. contents::
    :local:
    :depth: 2

What is Moloch?
===============

Moloch is an open source, large scale IPv4 packet capturing (PCAP), indexing
and database system. A simple web interface is provided for PCAP browsing,
searching, and exporting. APIs are exposed that allow PCAP data and
JSON-formatted session data to be downloaded directly. Simple security is
implemented by using HTTPS and HTTP digest password support or by using apache
in front. Moloch is not meant to replace IDS engines but instead work along side 
them to store and index all the network traffic in standard PCAP format, providing 
fast access.  Moloch is built to be deployed across many systems and can scale to 
handle multiple gigabits/sec of traffic. 

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

.. _quick-start:

Quick Start
===========

IMPATIENT? Run ``./easybutton-singlehost.sh`` in the Moloch source distribution
to build a complete single-machine Moloch system. This is good for a demo and
can also be used as a starting point for a real production deployment.

.. _components:

Components
==========

The Moloch system is comprised of 3 components

1. ``capture`` - A single-threaded C application that runs per network
   interface. It is possible to run multiple capture processes per machine if
   there are multiple interfaces to monitor.

2. ``viewer`` - A `node.js <http://nodejs.org/>`_ application that runs per
   capture machine and handles the web interface and transfer of PCAP files.

3. ``elasticsearch`` - The search database technology powering Moloch.

.. _install:

Building and Installing
=======================

Moloch is a complex system to build and install. The following are rough
guidelines. (Improvements to these instructions are always welcome!)

.. _install-elasticsearch:

Installing Elasticsearch
------------------------

Recommend version **1.4.4**, requires at least 1.2.x

1. Prep the ``elasticsearch`` machines by increasing max file descriptors add
   allowing memory locking. 
   On CentOS and others this is done by adding the following to bottom of
   ``/etc/security/limits.conf``::

    *                -      nofile          128000
    *                -      memlock         unlimited

2. If this is a dedicated machine, disable swap by commenting out the ``swap``
   lines in ``/etc/fstab`` and either reboot or use the ``swapoff`` command.

3. `Download elasticsearch <https://www.elastic.co/downloads/elasticsearch>`_.
   **Important:** At this time all development is done with `elasticsearch
   1.5.2 <https://www.elastic.co/downloads/past-releases/elasticsearch-1-5-2>`_.

4. Uncompress the archive you downloaded.

5. Install ``bigdesk`` and ``elasticsearch-head`` **BEFORE** pushing to all
   machines::

    cd elasticsearch-*
    bin/plugin -install mobz/elasticsearch-head
    bin/plugin -install lukas-vlcek/bigdesk

6. Create or modify ``elasticsearch.yml`` and push it to all machines. (See
   ``db/elasticsearch.yml.sample`` in the Moloch source distribution for an
   example.)
   
   - set ``cluster.name`` to something unique
   - set ``node.name`` to ``${ES_HOSTNAME}``
   - set ``node.max_local_storage_nodes`` to number of nodes per machine
   - set ``index.fielddata.cache: node``
   - set ``indices.fielddata.cache.size: 40%``
   - set ``path.data`` and ``path.logs``
   - set ``gateway.type: local``
   - set ``gateway.recover_after_nodes`` should match the number of nodes you
     will run 
   - set ``gateway.expected_nodes`` to the number of nodes you will run
   - disable ``zen.ping.multicast``
   - enable ``zen.ping.unicast`` and set the list of hosts

7. Create an ``elasticsearch`` launch script or use `one of the ones out there
   <https://gist.github.com/3569769>`_. (See ``db/runes.sh.sample`` in the
   Moloch source distribution for a simple one.)

   - Make sure you call ``ulimit -a`` first 
   - set ``ES_HEAP_SIZE=20G`` (or whatever number you are using, less then 32G) 
   - set ``JAVA_OPTS="-XX:+UseCompressedOops"`` if using real Java
   - set ``ES_HOSTNAME`` to ```hostname -s```

8. Start the cluster, waiting ~5s between starting each node to give them time
   to properly mesh.

9. Use ``elasticsearch-head`` to look at your cluster and make sure it is
   **GREEN**.

10. Inside the *installed* ``$MOLOCH_PREFIX/db`` directory run the 
    ``db.pl A_ES_HOSTNAME init`` script.

11. Check ``elasticsearch-head`` again and make sure it is still **GREEN** and
    now you should see some of the indexes.

.. _building-capture:

Building Capture
----------------

1. Install prerequisite standard packages.

   - CentOS::

        yum install wget curl pcre pcre-devel pkgconfig flex bison gcc-c++ zlib-devel e2fsprogs-devel openssl-devel file-devel make gettext libuuid-devel perl-JSON bzip2-libs bzip2-devel perl-libwww-perl libpng-devel xz libffi-devel

   - Ubuntu::
    
        apt-get install wget curl libpcre3-dev uuid-dev libmagic-dev pkg-config g++ flex bison zlib1g-dev libffi-dev gettext libgeoip-dev make libjson-perl libbz2-dev libwww-perl libpng-dev xz-utils libffi-dev

   - OS X::

        port install yara libpcap libnids openssl pcre flex bison zlib file gettext p5-JSON p5-libwww-perl libffi xz ossp-uuid libgeoip glib2
        ./configure --with-libpcap=/opt/local --with-libnids=/opt/local --with-yara=/opt/local --with-GeoIP=/opt/local LDFLAGS=-L/opt/local/lib --with-glib2=no GLIB2_CFLAGS="-I/opt/local/include/glib-2.0 -I/opt/local/lib/glib-2.0/include" GLIB2_LIBS="-L/opt/local/lib -lglib-2.0 -lgmodule-2.0 -lgobject-2.0 -lgio-2.0"

2. Building ``capture`` can be a pain because of OS versions.

   - Try ``./easybutton-build.sh`` which will download all the following,
     compile them statically, and run the local configure script.
   - Or if you want build yourself, or use some already installed packages then
     here are the pieces you need:

     + `glib-2 <http://ftp.gnome.org/pub/gnome/sources/glib>`_ version 2.30 or
       higher (2.42 is recommended for static builds)::

            wget http://ftp.gnome.org/pub/gnome/sources/glib/2.42/glib-2.42.0.tar.xz
            ./configure --disable-xattr --disable-shared --enable-static --disable-libelf --disable-selinux

     + `yara <http://yara-project.googlecode.com>`_ version 1.6 or higher::

            wget http://yara-project.googlecode.com/files/yara-1.7.tar.gz
            ./configure --enable-static

     + `MaxMind GeoIP <http://www.maxmind.com/app/c>`_ - The OS version may be
       recent enough::
            wget http://www.maxmind.com/download/geoip/api/c/GeoIP-1.6.0.tar.gz
            libtoolize -f # Only some platforms need this
            ./configure --enable-static

     + `libpcap <http://www.tcpdump.org/#latest-release>`_ - version 1.3 or
       higher (most OS versions are older)::
       
             wget http://www.tcpdump.org/release/libpcap-1.7.2.tar.gz
             ./configure --disable-dbus

     + `libnids <http://libnids.sourceforge.net/>`_ - version 1.24 or higher::

             wget http://downloads.sourceforge.net/project/libnids/libnids/1.24/libnids-1.24.tar.gz
             ./configure --disable-libnet --disable-glib2

3. Run ``configure``. Optionally use the ``--with-<foo>`` directives to use
   static libraries from build directories.

4. Run ``make``.

.. _building-viewer:

Building Viewer
---------------

1. You'll need `Python <http://python.org>`_ 2.6 or higher. If you're using
   CentOS 5.x (which provides Python 2.4), install a parallel version of Python
   from the `EPEL <http://fedoraproject.org/wiki/EPEL>`_ repository. Make sure
   ``python2.6`` is in your path before proceeding!

2. Install `Node.js <http://nodejs.org/>`_ version 0.10.x (0.10.21 or higher), currently 0.12.x is not supported.

   - **Binary install:** Please see the `platform-specific instructions
     <https://github.com/joyent/node/wiki/Installing-Node.js-via-package-manager>`_.
   - **Source install:** `Download the Node.js source <http://nodejs.org/dist/v0.10.38/node-v0.10.38.tar.gz>`_, build, and install.

3. In the ``viewer`` directory run ``npm update``.

.. _configuration:

Configuration
-------------

1. Make sure you download the latest freely available GeoIP and RIR files. 

   - `GeoLiteCountry <http://geolite.maxmind.com/download/geoip/database/GeoLiteCountry/GeoIP.dat.gz>`_ - Geographic IP data
   - `GeoIPASNum <http://www.maxmind.com/download/geoip/database/asnum/GeoIPASNum.dat.gz>`_ - Geographic Autonomous System (AS) number data
   - `ipv4-address-space <https://www.iana.org/assignments/ipv4-address-space/ipv4-address-space.csv>`_ - RIR assignments   

2. Edit the ``config.ini`` file.
   
3. In the ``viewer`` directory, run ``addUser.js`` to add users. Pass the
   ``-admin`` flag if you want admin users that can edit users from the web
   site. This is a good test if ``elasticsearch`` and ``config.ini`` are setup
   correctly::

    node addUser.js <userid> "<Friendly Name>" <password>

4. Edit the ``db/daily.sh`` script, and set it up in the crontab on one
   machine.

.. _running:

Get it Running
--------------

If you've made it this far, you are awesome!

On each ``capture`` machine you need to run at least one ``moloch-capture`` and
one ``moloch-viewer``. You may use the good old inittab. Add this to
``/etc/inittab`` (where ``/home/moloch`` is in fact the prefix where Moloch is
installed)::

    m1:2345:respawn:/home/moloch/capture/run.sh
    v1:2345:respawn:/home/moloch/viewer/run.sh

Sample versions can be found in ``capture/run.sh.sample`` and
``viewer/run.sh.sample`` in the Moloch source distribution.

.. _test:

Test it Out
-----------

Point your browser to any Moloch instance at ``https://<hostname>:<port>`` and
start tinkering!

.. _advanced:

Advanced Configuration
======================

.. _hardware-reqs:

Hardware Requirements
---------------------

Moloch is built to run across many machines for large deployments. 
What follows are rough guidelines for folks capturing large amounts 
of data with high bit rates, obviously tailor for the situation. 
It is not recommended to run the ``capture`` and ``elasticsearch`` 
processes on the same machines for highly utilized GigE networks.
For demo, small network, or home installations everything on a 
single machine is fine.

1. Moloch ``capture``/``viewer`` systems

   * One dedicated management network interface and CPU for OS
   * For each network interface being monitored recommend ~10G of memory and
     another dedicated CPU
   * If running suricata or another IDS add an additional two (2) CPUs per
     interface, and an additional 5G memory (or more depending on IDS
     requirements)
   * Disk space to store the PCAP files: We recommend at least 10TB, xfs (with
     inode64 option set in fstab), RAID 5, at least 5 spindles)
   * Disable swap by removing it from fstab
   * If networks are highly utilized and running IDS then CPU affinity is required

2. Moloch ``elasticsearch`` systems (some black magic here!)

   * ``1/4 * Number_Highly_Utilized_Interfaces * Number_of_Days_of_History`` is
     a **ROUGH** guideline for number of ``elasticsearch`` instances (nodes)
     required. (Example: 1/4 * 8 interfaces * 7 days = 14 nodes)
   * Each ``elasticsearch`` node should have ~30G-40G memory (20G-30G [no
     more!] for the java process, at least 10G for the OS disk cache)
   * You can have multiple nodes per machine (Example 64G machine can have 2 ES
     nodes, 22G for the java process 10G saved for the disk cache)
   * Disable swap by removing it from fstab
   * Obviously the more nodes, the faster responses will be
   * You can always add more nodes, but it's hard to remove nodes (more on this
     later)

Example Configuration
~~~~~~~~~~~~~~~~~~~~~

Here is an example system setup for monitoring 8x GigE highly-utilized networks, with an average of ~5 Gigabit/sec, with ~7 days of pcap storage.

* ``capture``/``viewer`` machines
 
  - 8x PenguinComputing Relion 4724 
  - 48GB of memory 
  - 40TB of disk-
  - Running Moloch and `Suricata <http://suricata-ids.org/>`_

* ``elasticsearch`` machines

  - 10x HP DL380-G7
  - 64GB of memory
  - 2TB of disk
  - Each system running 2 nodes

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

* Elasticsearch provides NO security, so ``iptables`` MUST be used allowing
  only Moloch machines to talk to the ``elasticsearch`` machines (ports
  9200-920x) and for them to mesh connect (ports 9300-930x).  An example with 3 ES machines 2 nodes each and a viewer only machine::
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
  
* Moloch machines should be locked down, however they need to talk to each
  other (port 8005), to the elasticsearch machines (ports 9200-920x), and the
  web interface needs to be open (port 8005).
* Moloch ``viewer`` should be configured to use SSL.

  - It's easiest to use a single certificate with multiple DNs.
  - Make sure you protect the cert on the filesystem with proper file
    permissions.

* It is possible to set up a Moloch ``viewer`` on a machine that doesn't
  capture any data that gateways all requests.

  - It is also possible to place apache in front of moloch, so it can handle the
    authentication and pass the username on to moloch
  - This is how we deploy it

* A shared password stored in the Moloch configuration file is used to encrypt
  password hashes AND for inter-Moloch communication. 

  - Make sure you protect the config file on the filesystem with proper file
    permissions.
  - Encrypted password hashes are used so a new password hash can not be
    inserted into ``elasticsearch`` directly in case it hasn't been secured.

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

