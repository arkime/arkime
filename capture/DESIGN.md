Capture is a multithreaded glib2 application

# Threads

In general capture tries to not use locks for anything but queues when communicating between threads.
When possible we use read only complex data structures shared across threads.
When those data structures need to be updated we create a new one and replace the old one, which is schedule to be freed at a later time (moloch_free_later) so any curernt readers don't crash.

## moloch-capture
The main thread, all http requests are on the main thread.  
Since sessions aren't locked, any sessions actions need to be added to the packet threads.

## moloch-stats
Simple thread that just calculates all the stats occasionally and sends to ES.

## moloch-pkt##
The packet threads controlled by packetThreads.
These threads are responsible for processing packets that are passed to it in batches.
Sessions are hashed across packet threads and all packets are processed by where the session is.
Any operations to a session has to happen in the packet thread since sessions don't have locks.
Use moloch_session_add_cmd to schedule a session task from a different thread.

## moloch-pcap#
When using the libpcap reader a thread is created for each interface.
These threads are responsible for reading in the packets and batch adding them to the packet threads.

## moloch-af3#-#
When using the afpacket reader a thread is created for each interface * tpacketv3NumThreads
These threads are responsible for reading in the packets and batch adding them to the packet threads.

## moloch-simple
A single thread that is responsible for writing out to disk the completed pcap buffers.


# Files

# Creating new parsers

Packets have two phases of life and most parsers will need to deal with both phases.
The first phase runs on the reader thread and should do very basic parsing and validation of the packet.
The second phase runs on the packet thread and does whatever decoding and SPI data generation need to be done.

## Ethernet/IP Enqueue phase

This phase is responsible for 
* basic decoding and verification of the packet
* setting the `ses` field with the packet session type
* setting the `hash` field with the hash of the session id
* setting the `mProtocol` field with which the moloch protocol information

You only need to create a new enqueue callback for special ethernet and ip protocols, which can be set with the moloch_packet_set_ethernet_cb and moloch_packet_set_ip_cb.

## Ethernet/IP Process phase

This phase is responsible for actually processing the packets and generating the SPI data.
You only need to create new process callbacks for special ethernet adn ip protocols.
The callbacks are set with the moloch_mprotocol_register

moloch_mprotocol_register (create_session_id, pre_process, process)

* create_session_id - required - Given a packet, fill in the session id
* pre_process - required - called before saving/rules. Given the session, packet, isNewSession - can be used to set any initial SPI data fields or packet direction
* process - optional - called after saving to disk and rules.  Should generate most of the SPI data or enqueue for higher level protocol decoding.  Returns if the packet should be freed or not yet.

## TCP/UDP

moloch_parsers_register2
define moloch_parsers_register

#define moloch_parsers_classifier_register_tcp
#define moloch_parsers_classifier_register_udp

#define moloch_parsers_classifier_register_port
