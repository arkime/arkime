import arkime
import arkime_packet

# Test plugin for tests/python-packet-cb.t
#
# Registers an ethernet callback for experimental ethertype 0x88B5 whose
# payload is a normal IPv4 packet. The callback forwards to the real IPv4
# handler, but returns None (a non-int) for the first packet. capture must
# treat the non-int return as 0 (process) instead of -1 (PyLong_AsLong
# error value), which used to silently drop the packet and corrupt
# packetStats.

count = 0


def ether_cb(batch, packet, data, length):
    global count
    count += 1
    rc = arkime_packet.run_ethernet_cb(batch, packet, data, 0x0800, "pytest")
    if count == 1:
        return None
    return rc


arkime_packet.set_ethernet_cb(0x88B5, ether_cb)
