# Moloch
> Moloch is a large scale, open source, indexed packet capture and search system.

![banner](https://raw.githubusercontent.com/aol/moloch/master/viewer/public/moloch_155.png)


Moloch augments your current security infrastructure to store and index network traffic in standard PCAP format, providing fast, indexed access. An intuitive and simple web interface is provided for PCAP browsing, searching, and exporting. Moloch exposes APIs which allow for PCAP data and JSON formatted session data to be downloaded and consumed directly. Moloch stores and exports all packets in standard PCAP format, allowing you to also use your favorite PCAP ingesting tools, such as wireshark, during your analysis workflow.

Moloch is built to be deployed across many systems and can scale to handle tens of gigabits/sec of traffic. PCAP retention is based on available sensor disk space. Metadata retention is based on the Elasticsearch cluster scale. Both can be increased at anytime and are under your complete control.

## Table of Contents

- [Background](#background)
- [Install](#install)
- [Configuration](#configuration)
- [Usage](#usage)
- [Security](#security)
- [API](#api)
- [Contribute](#contribute)
- [License](#license)

## Background

Moloch was created to replace commercial full packet systems at AOL in 2012.  By having complete control of hardware and costs, we found we could deploy full packet capture across all our networks for the same cost as just one network using a commercial tool.

The Moloch system is comprised of 3 components:
* **capture** - A threaded C application that monitors network traffic, writes PCAP formatted files to disk, parses the captured packets, and sends metadata (SPI data) to elasticsearch.
* **viewer** - A [node.js](http://nodejs.org/) application that runs per capture machine. It handles the web interface and transfer of PCAP files.
* **[elasticsearch](https://www.elastic.co/guide/en/elasticsearch/reference/current/getting-started.html)** - The search database technology powering Moloch.

Once installed, a user can look at the data Moloch has captured using a simple web interface.  Moloch provides multiple views of the data.  The primary view is the Sessions page that contains a list of sessions. Each session can be opened to view the metadata and PCAP data.

<img src="https://raw.github.com/wiki/aol/moloch/sessions.png" width="500">

Another way to view the data is the SPI View page, which allows the user to see all the unique values for each field that Moloch understands.

<img src="https://raw.github.com/wiki/aol/moloch/spiview.png" width="500">

## Install

Most users should use the prebuilt binaries available at our [Downloads page](https://molo.ch/#downloads) and follow the simple install instructions on that page.

For advanced users, you can build Moloch yourself: 
* `git clone https://github.com/aol/moloch`
* `./easybutton-build.sh --install` downloads all the prerequisites, build, and install
* `make config` - performs an initial Moloch configuration


## Configuration

Most of the system configuration will take place in the `/data/moloch/etc/config.ini` file.  The variables are documented in our [Settings Wiki page](https://github.com/aol/moloch/wiki/Settings).

## Usage

Once Moloch is running, point your browser to http://localhost:8005 to access the web interface.  **Click on the Owl to reach the Moloch help page.**

## Security

Access to Moloch is protected by using HTTPS with digest passwords or by using an authentication providing web server proxy. All PCAPs are stored on the sensors and are only accessed using the Moloch interface or API. Moloch is not meant to replace an IDS but instead work alongside them to store and index all the network traffic in standard PCAP format, providing fast access.  

Elasticsearch provides NO security by default, so ``iptables`` **MUST** be used to allow only Moloch machines to talk to the ``elasticsearch`` machines (ports 9200-920x) and for them to mesh connect (ports 9300-930x).  An example with 3 ES machines 2 nodes each and a viewer only machine
```
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
```

* Moloch machines should be locked down, however they need to talk to each other (port 8005), to the elasticsearch machines (ports 9200-920x), and the web interface needs to be open (port 8005).

* Moloch ``viewer`` should be configured to use SSL.

  - It's easiest to use a single certificate with multiple DNs.
  - Make sure you protect the cert on the filesystem with proper file permissions.

* It is possible to set up a Moloch ``viewer`` on a machine that doesn't capture any data that gateways all requests.

  - It is also possible to place Apache in front of Moloch, so it can handle the authentication and pass the username on to Moloch.
  - This is how we deploy it.

* A shared password stored in the Moloch configuration file is used to encrypt password hashes AND for inter-Moloch communication.

  - Make sure you protect the config file on the filesystem with proper file permissions.
  - Encrypted password hashes are used so a new password hash can not be inserted into ``elasticsearch`` directly in case it hasn't been secured.

## API

You can learn more about the Moloch API on our [API Wiki page](https://github.com/aol/moloch/wiki/API).


## Contribute

Please refer to the [CONTRIBUTING.md](CONTRIBUTING.md) file for information about how to get involved. We welcome issues, feature requests, pull requests, and documentation updates in GitHub.  For questions about using and troubleshooting Moloch please use the Slack channels.

## Maintainers

The best way to reach us is on Slack.  Please request an invitation to join the Moloch FPC Slack workspace [here](https://slackinvite.molo.ch).

## License

This project is licensed under the terms of the Apache 2.0 open source license. Please refer to [LICENSE](LICENSE) for the full terms.
