from typing import Any, Callable

PacketRC = int
PortKind = int

# === Opaque objects ===
ArkimeSession = Any
ArkimePacketBatch = Any
ArkimePacket = Any
memoryview = Any

# === Callback types ===
ClassifyCb = Callable[[ArkimeSession, memoryview, int, int], None]
""" 
This callback is called for the first packet of a session in each direction that matches the tcp/udp/port registered classifiers. The callback should look at the bytes and see if it understand the protocol. If it does it will usually call the arkime_session.ad_protocol and/or arkime_session.register_parser methods.

Args:
    session: The opaque session object, used with any arkime_session module methods.
    packetBytes: The memory view of the packet bytes; only valid during the callback.
    packetLen: The length of the packet.
    direction: The direction of the packet; 0 for client to server, 1 for server to client.
"""

ParserCb = Callable[[ArkimeSession, memoryview, int, int], int]
""" 
This callback is called for the every packet of a session in each direction where the callback has been registered using arkime_session.register_parser. Return -1 to unregister the parser for the session, 0 is normal case or positive value for the number of bytes consume if this protocol wraps others (rare).

Args:
    session: The opaque session object, used with any arkime_session module methods.
    packetBytes: The memory view of the packet bytes; only valid during the callback.
    packetLen: The length of the packet.
    direction: The direction of the packet; 0 for client to server, 1 for server to client.
"""

SaveCb = Callable[[ArkimeSession, bool], None]
""" 
This callback is used for both pre_save and save callbacks.

Args:
    session: The opaque session object, used with any arkime_session module methods.
    final: True if this is the final session save callback, False if there are more linked sessions.
"""

PacketCb = Callable[[ArkimePacketBatch, ArkimePacket, memoryview, int], PacketRC]
""" 
This callback is called for packets by the reader threads that the Python script has registered for. Usually some basic processing is done and then the run_ethernet_cb or run_ip_cb methods are called to process the packet. The callback should return the results from the run calls or one of the ARKIME_PACKET_* values.

Args:
    batch: The opaque batch object
    packet: The opaque patch object
    packetBytes: The memory view of the packet bytes; only valid during the callback.
    packetLen: The length of the packet.
"""
