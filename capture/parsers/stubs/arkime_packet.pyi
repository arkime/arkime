"""
Python Arkime Packet Module

The Python Arkime Packet module has methods for dealing with packets before they are associated with sessions. The API is very unpythonic and treats the packet as a opaque object that needs to be passed around.
"""

from typing import Any

from .types import PacketRC, ArkimePacket, ArkimePacketBatch, memoryview, PacketCb

# === Constants for PacketRC ===
DO_PROCESS: PacketRC      # Process the packet normally
CORRUPT: PacketRC         # The packet is corrupt
UNKNOWN_ETHER: PacketRC   # Unknown Ethernet type encountered
UNKNOWN_IP: PacketRC      # Unknown IP protocol encountered
DONT_PROCESS: PacketRC    # The packet should not be processed but can be freed
DONT_PROCESS_OR_FREE: PacketRC  # The packet should not be processed and should not be freed

# === Methods ===
def get(packet: ArkimePacket, field: str) -> Any:
    """
    Retrieve the value of a packet field.

    Args:
        packet: The packet object from the packetCb.
        field: The string field name to retrieve.
            - copied - Integer - 0 = not copied, 1 = copied
            - direction - Integer - 0 = client to server, 1 = server to client
            - etherOffset - Integer - Offset of ethernet header in packet
            - ipOffset - Integer - Offset of IP header in packet
            - ipProtocol - Integer - IP protocol number (6=TCP, 17=UDP, 132=SCTP, etc.)
            - mProtocol - Integer - The Arkime mProtocol number
            - outerEtherOffset - Integer - Offset of outer ethernet header for tunneled packets
            - outerIpOffset - Integer - Offset of outer IP header for tunneled packets
            - outerv6 - Integer - 1 if outer IP is IPv6, 0 if IPv4
            - payloadLen - Integer - Length of the payload
            - payloadOffset - Integer - Offset of the payload in packet
            - pktlen - Integer - The full packet length
            - readerFilePos - Integer - The file position of the packet in the pcap file
            - readerPos - Integer - Index of the reader internal data
            - tunnel - Integer - Bitflags: 0x01=GRE, 0x02=PPPoE, 0x04=MPLS, 0x08=PPP, 0x10=GTP, 0x20=VXLAN, 0x40=VXLAN-GPE, 0x80=Geneve
            - v6 - Integer - 1 if IP is IPv6, 0 if IPv4
            - vlan - Integer - The first VLAN tag if present, 0 if not present
            - vni - Integer - The VXLAN VNI if present, 0 if not present
            - wasfrag - Integer - 1 if the packet was a fragment, 0 if not
            - writerFileNum - Integer - The writer file number from files index
            - writerFilePos - Integer - The offset in the writer file
    """
    ...
def set(packet: ArkimePacket, field: str, value: int) -> None:
    """
    Set the value of a packet field. Only certain fields can be set.

    Args:
        packet: The packet object from the packetCb.
        field: The string field name to set.
            - etherOffset - Integer - Offset of ethernet header in packet
            - mProtocol - Integer - The Arkime mProtocol number
            - outerEtherOffset - Integer - Offset of outer ethernet header for tunneled packets
            - outerIpOffset - Integer - Offset of outer IP header for tunneled packets
            - outerv6 - Integer - 1 if outer IP is IPv6, 0 if IPv4
            - payloadLen - Integer - Length of the payload
            - payloadOffset - Integer - Offset of the payload in packet
            - tunnel - Integer - Bitflags: 0x01=GRE, 0x02=PPPoE, 0x04=MPLS, 0x08=PPP, 0x10=GTP, 0x20=VXLAN, 0x40=VXLAN-GPE, 0x80=Geneve
            - v6 - Integer - 1 if IP is IPv6, 0 if IPv4
            - vlan - Integer - The first VLAN tag if present, 0 if not present
            - vni - Integer - The VXLAN VNI if present, 0 if not present
        value: The integer value to set.
    """
    ...

def set_ethernet_cb(type: int, packetCb: PacketCb) -> None:
    """
    Register an ethertype packet callback. Called for packets of the given ethertype.
    Typically the callback strips headers and calls run_ip_cb or run_ethernet_cb.

    Args:
        type: The Ethertype to register for (e.g., 0x0800=IPv4, 0x86DD=IPv6, 0x8100=VLAN).
        packetCb: The callback to call for packets of the given ethertype.
    """
    ...
def set_ip_cb(type: int, ipCb: PacketCb) -> None:
    """
    Register an IP protocol packet callback. Called for packets of the given IP protocol.

    Args:
        type: The IP protocol number to register for (e.g., 6=TCP, 17=UDP, 47=GRE, 132=SCTP).
        ipCb: The callback to call for packets of the given protocol.
    """
    ...

def run_ethernet_cb(
    batch: ArkimePacketBatch,
    packet: ArkimePacket,
    packetBytes: memoryview,
    type: int,
    description: str
) -> PacketRC:
    """
    Continue processing a packet at the Ethernet layer. Calls the registered callback for the ethertype.

    Args:
        batch: The opaque batch object from the packetCb.
        packet: The opaque packet object from the packetCb.
        packetBytes: The memoryview of packet bytes starting at the new ethernet header.
        type: The Ethertype of the inner packet (e.g., 0x0800=IPv4, 0x86DD=IPv6).
        description: A short description for logging/debugging (e.g., "myproto").

    Returns:
        PacketRC: The result from the ethertype handler, or UNKNOWN_ETHER if no handler registered.
    """
    ...
def run_ip_cb(
    batch: ArkimePacketBatch,
    packet: ArkimePacket,
    packetBytes: memoryview,
    type: int,
    description: str
) -> PacketRC:
    """
    Continue processing a packet at the IP layer. Calls the registered callback for the IP protocol.

    Args:
        batch: The opaque batch object from the packetCb.
        packet: The opaque packet object from the packetCb.
        packetBytes: The memoryview of packet bytes starting at the IP header.
        type: The IP protocol number (e.g., 6=TCP, 17=UDP, 132=SCTP).
        description: A short description for logging/debugging (e.g., "myproto").

    Returns:
        PacketRC: The result from the IP protocol handler, or UNKNOWN_IP if no handler registered.
    """
    ...
