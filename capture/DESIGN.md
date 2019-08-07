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
