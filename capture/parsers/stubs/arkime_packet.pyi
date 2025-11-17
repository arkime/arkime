"""
Python Arkime Packet Module

The Python Arkime Packet module has methods for dealing with packets before they are associated with sessions. The API is very unpythonic and treats the packet as a opaque object that needs to be passed around.
"""

from typing import Any

from .types import PacketRC, ArkimePacket, ArkimePacketBatch, memoryview, PacketCb

# === Constants for PacketRC ===
DO_PROCESS: PacketRC      # Process the packet normally
CORRUPT: PacketRC         # The packet is corrupt
UNKNOWN: PacketRC         # The packet is unknown and can't be processed
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
            - etherOffset - Integer - Offset of ethernet header
            - ipOffset - Integer - Offset of IP header
            - ipProtocol - Integer - If an ip packet, the IP protocol
            - mProtocol - Integer - The Arkime mProtocol number
            - outerEtherOffset - Integer - Offset of outer ethernet header for tunneled packets
            - outerIpOffset - Integer - Offset of outer IP header for tunneled packets
            - outerv6 - Integer - 1 if outer IP is IPv6, 0 if IPv4
            - payloadLen - Integer - Length of the payload
            - payloadOffset - Integer - Offset of the payload
            - pktlen - Integer - The full packet length
            - readerFilePos - Integer - The file position of the packet in the pcap file
            - readerPos - Integer - Index of the reader internal data
            - tunnel - Integer - bitflag of various tunnel protocols that were seen
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
    Set the value of a packet field. Not all fields can be set.

    Args:
        packet: The packet object from the packetCb.
        field: The string field name to set.
            - etherOffset - Integer - Offset of ethernet header
            - mProtocol - Integer - The Arkime mProtocol number
            - outerEtherOffset - Integer - Offset of outer ethernet header for tunneled packets
            - outerIpOffset - Integer - Offset of outer IP header for tunneled packets
            - outerv6 - Integer - 1 if outer IP is IPv6, 0 if IPv4
            - payloadLen - Integer - Length of the payload
            - payloadOffset - Integer - Offset of the payload
            - tunnel - Integer - bitflag of various tunnel protocols that were seen
            - v6 - Integer - 1 if IP is IPv6, 0 if IPv4
            - vlan - Integer - The first VLAN tag if present, 0 if not present
            - vni - Integer - The VXLAN VNI if present, 0 if not present
        value: The integer value to set.
    """
    ...

def set_ethernet_cb(type: int, packetCb: PacketCb) -> None:
    """
    Register an ethertype packet callback that will be called for packets of the given type. Usually this callback with just need to strip some headers and call either run_ip_cb or run_ethernet_cb.

    Args:
        type: The Ethertype to register the callback for.
        packetCb: The callback to call for packets of the given ethertype.
    """
    ...
def set_ip_cb(type: int, ipCb: PacketCb) -> None:
    """
    Register an IP protocol packet callback that will be called for packets of the given protocol.

    Args:
        type: The IP protocol to register the callback for.
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
    Process a packet at the Ethernet layer by running the registered Ethernet callback.

    Args:
        batch: The opaque batch object.
        packet: The opaque packet object.
        packetBytes: The memory view of the packet bytes
        type: The Ethertype of the packet now
        description: A string description of the packet now
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
    Process a packet at the IP layer by running the registered IP callback.

    Args:
        batch: The opaque batch object.
        packet: The opaque packet object.
        packetBytes: A memoryview of the packet bytes.
        type: The ip protocol of the packet now
        description: A string description of the packet now
    """
    ...
