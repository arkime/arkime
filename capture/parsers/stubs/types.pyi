from typing import Any, Callable

PacketRC = int  # Return code from packet callbacks
PortKind = int  # Bitwise flags for port classifier types

# === Opaque objects ===
# These are C pointers passed as Python integers. Use with arkime_* module functions.
ArkimeSession = Any   # Session handle, valid during callback unless incref'd
ArkimePacketBatch = Any  # Batch handle for packet processing
ArkimePacket = Any    # Packet handle, use with arkime_packet.get()/set()
memoryview = Any      # Read-only packet bytes, only valid during callback

# === Callback types ===
ClassifyCb = Callable[[ArkimeSession, memoryview, int, int], None]
"""
Classifier callback for identifying protocols. Called for the first packet in each direction
that matches a registered tcp/udp/sctp/port classifier. The callback should look at the bytes
and see if it understands the protocol. If it does it will usually call arkime_session.add_protocol()
and/or arkime_session.register_parser().

Args:
    session: Opaque session handle, use with arkime_session module methods.
    packetBytes: Read-only memoryview of packet bytes; only valid during callback.
    packetLen: Length of the packet in bytes.
    which: For TCP/UDP: 0 = client to server, 1 = server to client.
           For SCTP: direction in bit 0, stream ID in upper bits (use which & 1 for direction).
"""

ParserCb = Callable[[ArkimeSession, memoryview, int, int], int]
"""
Parser callback for protocol dissection. Called for every packet of a session in each direction
after being registered with arkime_session.register_parser().

Args:
    session: Opaque session handle, use with arkime_session module methods.
    packetBytes: Read-only memoryview of packet bytes; only valid during callback.
    packetLen: Length of the packet in bytes.
    which: For TCP/UDP: 0 = client to server, 1 = server to client.
           For SCTP: direction in bit 0, stream ID in upper bits (use which & 1 for direction).

Returns:
    -1: Unregister parser (no more callbacks for this session)
     0: Normal case, continue receiving packets
    >0: Number of bytes consumed (used when this protocol wraps others)
"""

SaveCb = Callable[[ArkimeSession, int], None]
"""
Session save callback. Used for both pre_save and save callbacks.

Args:
    session: Opaque session handle, use with arkime_session module methods.
    final: Non-zero if this is the final save for this session, 0 if more linked sessions follow.
"""

PacketCb = Callable[[ArkimePacketBatch, ArkimePacket, memoryview, int], PacketRC]
"""
Low-level packet callback for handling custom Ethernet types or IP protocols.
Called by reader threads for registered ethertypes/protocols. Usually strips headers
and calls arkime_packet.run_ethernet_cb() or arkime_packet.run_ip_cb().

Args:
    batch: Opaque batch handle, pass to run_ethernet_cb/run_ip_cb.
    packet: Opaque packet handle, use with arkime_packet.get()/set().
    packetBytes: Read-only memoryview of packet bytes; only valid during callback.
    packetLen: Length of the packet in bytes.

Returns:
    PacketRC: Return result from run_*_cb(), or DO_PROCESS/CORRUPT/DONT_PROCESS/etc.
"""
