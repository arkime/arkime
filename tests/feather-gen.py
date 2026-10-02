#!/usr/bin/env python3
"""
Generate tests/pcap/feather_synthetic.pcap — synthetic traffic exercising
every signal source featherprint cares about (ARP, DHCP, DNS answers,
mDNS/DNS-SD PTR/SRV, NBNS responses, SSDP) plus intentional changes (MAC
change for an IP, DHCP hostname change, new mDNS service, known MAC moving
to a new IP) so history/alert paths are exercised end-to-end.

All device addresses live in 10.177.0.0/16, which no other test pcap uses,
so viewer regression assertions on classifications stay isolated.

Regenerate with:  python3 feather-gen.py   (then rebuild the .test file:
cd tests && ../capture/capture --tests -c config.test.ini -n test \
    -r pcap/feather_synthetic.pcap | ./tests.pl --fix > pcap/feather_synthetic.test)
"""
import os, random, time
from scapy.all import (
    Ether, ARP, IP, UDP, BOOTP, DHCP, DNS, DNSQR, DNSRR, DNSRRSRV,
    Raw, wrpcap
)

random.seed(20260829)
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "pcap",
                   "feather_synthetic.pcap")

pkts = []          # main timeline
late_pkts = []     # change events appended after the main timeline
GW_MAC = "00:11:22:33:44:55"
GW_IP  = "10.177.0.1"

# ----------------------------------------------------------------------------
# ARP — 30 hosts with stable (ip,mac) bindings, each producing one reply
# session. 10.177.0.12 (kitchen-pi) deliberately carries a Raspberry Pi OUI
# so the macPrefixIn rule outranks its Linux DHCP vendor class.
def arp_mac(i):
    if i == 2:
        return "b8:27:eb:00:00:12"   # kitchen-pi, Raspberry Pi OUI
    return f"aa:bb:cc:00:{i // 256:02x}:{i % 256:02x}"

arp_hosts = [(f"10.177.0.{10+i}", arp_mac(i)) for i in range(30)]
for ip, mac in arp_hosts:
    pkts.append(Ether(src=mac, dst="ff:ff:ff:ff:ff:ff") /
                ARP(op=2, hwsrc=mac, psrc=ip, hwdst=GW_MAC, pdst=GW_IP))

# Gratuitous ARP requests from the gateway (different 5-tuple → new sessions).
for ip, _ in arp_hosts[:20]:
    pkts.append(Ether(src=GW_MAC, dst="ff:ff:ff:ff:ff:ff") /
                ARP(op=1, hwsrc=GW_MAC, psrc=GW_IP, pdst=ip))

# MAC change: 10.177.0.10 re-announces with a brand new MAC (changeMac). Use a
# different target so this forms its own session instead of merging with the
# original reply.
NEW_MAC_FOR_10 = "ee:ee:ee:00:00:0a"
late_pkts.append(Ether(src=NEW_MAC_FOR_10, dst="ff:ff:ff:ff:ff:ff") /
                 ARP(op=2, hwsrc=NEW_MAC_FOR_10, psrc="10.177.0.10",
                     hwdst=GW_MAC, pdst="10.177.0.2"))

# IP change: 10.177.0.11's MAC shows up later announcing 10.177.0.99
# (arpwatch-style changeIp via the featherprint_macs tracking).
late_pkts.append(Ether(src="aa:bb:cc:00:00:01", dst="ff:ff:ff:ff:ff:ff") /
                 ARP(op=2, hwsrc="aa:bb:cc:00:00:01", psrc="10.177.0.99",
                     hwdst=GW_MAC, pdst=GW_IP))

# ----------------------------------------------------------------------------
# DHCP — DISCOVER/REQUEST/ACK per host. Broadcast DISCOVER/REQUEST come from
# 0.0.0.0, so the engine binds via option 50 (requested addr) and the ACK's
# yiaddr. Vendor classes drive the windows_pc/android/linux_host rules; the
# pi's OUI outranks its Linux vendor class (rule ordering).
DHCP_HOSTS = [
    # (mac, ip, hostname, vendor class)
    ("aa:bb:cc:00:00:01", "10.177.0.11", "bobs-laptop",  None),
    ("b8:27:eb:00:00:12", "10.177.0.12", "kitchen-pi",   "dhcpcd-8.1.2:Linux-5.10.63"),
    ("aa:bb:cc:00:00:03", "10.177.0.13", "android-13",   "android-dhcp-13"),
    ("aa:bb:cc:00:00:04", "10.177.0.14", "win11-rig",    "MSFT 5.0"),
    ("aa:bb:cc:02:00:30", "10.177.2.30", "officemac",    None),
]
def dhcp_opts(mtype, host=None, reqip=None, vendor=None, prl=True):
    o = [("message-type", mtype)]
    if reqip:  o.append(("requested_addr", reqip))
    if host:   o.append(("hostname", host.encode()))
    if vendor: o.append(("vendor_class_id", vendor.encode()))
    if prl:    o.append(("param_req_list", [1, 3, 6, 15, 31, 33, 43, 44]))
    o.append("end")
    return o

for i, (mac, ip, host, vendor) in enumerate(DHCP_HOSTS):
    chaddr = bytes.fromhex(mac.replace(":", "")) + b"\x00" * 10
    xid = 0xfea70000 + i
    pkts.append(Ether(src=mac, dst="ff:ff:ff:ff:ff:ff") /
                IP(src="0.0.0.0", dst="255.255.255.255") /
                UDP(sport=68, dport=67) /
                BOOTP(chaddr=chaddr, xid=xid) /
                DHCP(options=dhcp_opts("discover", host=host, vendor=vendor)))
    pkts.append(Ether(src=mac, dst="ff:ff:ff:ff:ff:ff") /
                IP(src="0.0.0.0", dst="255.255.255.255") /
                UDP(sport=68, dport=67) /
                BOOTP(chaddr=chaddr, xid=xid) /
                DHCP(options=dhcp_opts("request", host=host, reqip=ip, vendor=vendor)))
    pkts.append(Ether(src=GW_MAC, dst=mac) /
                IP(src=GW_IP, dst=ip) /
                UDP(sport=67, dport=68) /
                BOOTP(op=2, chaddr=chaddr, yiaddr=ip, siaddr=GW_IP, xid=xid) /
                DHCP(options=[("message-type", "ack"),
                              ("subnet_mask", "255.255.0.0"),
                              ("router", GW_IP),
                              ("name_server", GW_IP),
                              "end"]))

# Hostname change: bobs-laptop re-DHCPs with a new hostname (changeName).
mac, ip, _, _ = DHCP_HOSTS[0]
chaddr = bytes.fromhex(mac.replace(":", "")) + b"\x00" * 10
late_pkts.append(Ether(src=mac, dst="ff:ff:ff:ff:ff:ff") /
                 IP(src="0.0.0.0", dst="255.255.255.255") /
                 UDP(sport=68, dport=67) /
                 BOOTP(chaddr=chaddr, xid=0xfea71001) /
                 DHCP(options=dhcp_opts("request", host="bobs-new-laptop", reqip=ip)))

# ----------------------------------------------------------------------------
# DNS A answers — bind name↔ip via dns.answers (served by the gateway).
def dns_resp(qname, qtype, rdata, ttl=300):
    return DNS(id=random.randint(1, 65535), qr=1,
               qd=DNSQR(qname=qname, qtype=qtype),
               an=DNSRR(rrname=qname, type=qtype, rdata=rdata, ttl=ttl))

CLIENTS = ["10.177.0.99", "10.177.0.100", "10.177.0.101"]
DNS_FIXED = [
    ("alice.example.com",  "A", "10.177.0.10"),
    ("bob.example.com",    "A", "10.177.0.11"),
    ("dhcphost.lan",       "A", "10.177.2.30"),
    ("router.lan",         "A", "10.177.0.1"),
    ("github.com",         "A", "140.82.112.3"),
]
for i, (n, t, d) in enumerate(DNS_FIXED):
    pkts.append(Ether(src=GW_MAC, dst=f"aa:bb:cc:00:00:{60+i:02x}") /
                IP(src=GW_IP, dst=CLIENTS[i % len(CLIENTS)]) /
                UDP(sport=53, dport=33000 + i) / dns_resp(n, t, d))

# 22 more parametric ones so we comfortably clear the 100-session bar.
for i in range(22):
    n = f"host{i:02d}.example.com"
    d = f"10.177.3.{200+i}"
    pkts.append(Ether(src=GW_MAC, dst=f"aa:bb:cc:00:00:{70+i:02x}") /
                IP(src=GW_IP, dst=CLIENTS[i % len(CLIENTS)]) /
                UDP(sport=53, dport=34000 + i) / dns_resp(n, "A", d))

# ----------------------------------------------------------------------------
# mDNS/DNS-SD — devices announce their own services (PTR type + SRV instance
# with port/target, plus an A record for hostname↔ip binding).
MDNS_DST = "224.0.0.251"
MDNS_DMAC = "01:00:5e:00:00:fb"

def mdns_pkt(ip, smac, sport, an):
    return (Ether(src=smac, dst=MDNS_DMAC) / IP(src=ip, dst=MDNS_DST) /
            UDP(sport=sport, dport=5353) / DNS(qr=1, aa=1, an=an))

MDNS_DEVS = [
    # (ip, src mac, hostname, [service types])
    ("10.177.1.50", "0a:17:70:00:01:50", "appletv.local",
     ["_airplay._tcp.local", "_raop._tcp.local"]),
    ("10.177.1.51", "0a:17:70:00:01:51", "chromecast.local",
     ["_googlecast._tcp.local"]),
    ("10.177.1.52", "0a:17:70:00:01:52", "hpprinter.local",
     ["_ipp._tcp.local", "_pdl-datastream._tcp.local"]),
    ("10.177.1.53", "0a:17:70:00:01:53", "homepod.local",
     ["_airplay._tcp.local", "_raop._tcp.local"]),
    ("10.177.1.54", "0a:17:70:00:01:54", "sonos-livingroom.local",
     ["_sonos._tcp.local"]),
    ("10.177.1.55", "0a:17:70:00:01:55", "synology-ds920.local",
     ["_smb._tcp.local", "_sftp-ssh._tcp.local"]),
]
# One record per packet: scapy silently drops chained heterogeneous answer
# records, so keep each PTR/SRV/A in its own announcement (own sport → own
# session).
for i, (ip, smac, host, svcs) in enumerate(MDNS_DEVS):
    for j, svc in enumerate(svcs):
        instance = f"{host.split('.')[0]}.{svc}"
        pkts.append(mdns_pkt(ip, smac, 5000 + i * 20 + j * 2,
                    DNSRR(rrname=svc, type="PTR", rdata=instance, ttl=4500)))
        pkts.append(mdns_pkt(ip, smac, 5001 + i * 20 + j * 2,
                    DNSRRSRV(rrname=instance, type="SRV", port=7000 + j,
                             target=host, ttl=4500)))
    pkts.append(mdns_pkt(ip, smac, 5018 + i * 20,
                DNSRR(rrname=host, type="A", rdata=ip, ttl=4500)))

# Service enumeration response: the rdata carries the advertised type.
pkts.append(mdns_pkt("10.177.1.56", "0a:17:70:00:01:56", 5353,
                     DNSRR(rrname="_services._dns-sd._udp.local", type="PTR",
                           rdata="_spotify-connect._tcp.local", ttl=4500)))

# New service for appletv, announced later (newService event).
late_pkts.append(mdns_pkt("10.177.1.50", "0a:17:70:00:01:50", 5499,
                          DNSRR(rrname="_homekit._tcp.local", type="PTR",
                                rdata="appletv._homekit._tcp.local", ttl=4500)))

# ----------------------------------------------------------------------------
# NBNS — positive name query responses from the owners themselves (nbns.c only
# fills nbns.host from resource records, i.e. responses).
def nbns_encode(name):
    """RFC1001 first-level encoding: 15-char space-padded name + 0x20 suffix."""
    padded = name.upper().ljust(15)[:15] + "\x20"
    out = b""
    for ch in padded.encode():
        out += bytes([(ch >> 4) + 0x41, (ch & 0xF) + 0x41])
    return b"\x20" + out + b"\x00"

def nbns_response(ip, name):
    ipb = bytes(int(o) for o in ip.split("."))
    return (b"\x12\x34" +          # transaction id
            b"\x85\x00" +          # flags: response, authoritative
            b"\x00\x00" +          # qdcount
            b"\x00\x01" +          # ancount
            b"\x00\x00\x00\x00" +  # nscount / arcount
            nbns_encode(name) +
            b"\x00\x20" +          # type NB
            b"\x00\x01" +          # class IN
            b"\x00\x00\x0e\x10" +  # ttl
            b"\x00\x06" +          # rdlength
            b"\x00\x00" + ipb)     # nbFlags + address

NBNS_HOSTS = [
    ("10.177.0.60", "WORKSTATION01"),
    ("10.177.0.61", "WORKSTATION02"),
    ("10.177.0.62", "DESKTOP-AB12CD3"),
]
for i, (ip, name) in enumerate(NBNS_HOSTS):
    pkts.append(Ether(src=f"00:50:56:c0:00:{i:02x}", dst="ff:ff:ff:ff:ff:ff") /
                IP(src=ip, dst="10.177.255.255") /
                UDP(sport=137, dport=137) / Raw(load=nbns_response(ip, name)))

# ----------------------------------------------------------------------------
# SSDP — NOTIFYs from 4 devices with distinct SERVER/USN, M-SEARCH from 3
# controllers. Parsed by capture/parsers/ssdp.c into ssdp.* fields.
SSDP_NOTIFIERS = [
    ("10.177.1.71", "Roku/9.4.0 UPnP/1.0 Roku/9.4.0",
     "uuid:roku:ecp:1GU48K000123::upnp:rootdevice"),
    ("10.177.1.72", "Linux/4.x UPnP/1.0 LGTV/2.0",
     "uuid:lgtv-555::upnp:rootdevice"),
    ("10.177.1.73", "POSIX UPnP/1.0 IKEAGateway/2.3",
     "uuid:ikea-tradfri-001::upnp:rootdevice"),
    ("10.177.1.54", "Linux/3.x UPnP/1.0 SonosZP/68.x",
     "uuid:RINCON-1111::upnp:rootdevice"),
]
for i, (ip, server, usn) in enumerate(SSDP_NOTIFIERS):
    body = (b"NOTIFY * HTTP/1.1\r\n"
            b"HOST: 239.255.255.250:1900\r\n"
            b"CACHE-CONTROL: max-age=1800\r\n"
            b"LOCATION: http://" + ip.encode() + b":80/desc.xml\r\n"
            b"SERVER: " + server.encode() + b"\r\n"
            b"NT: upnp:rootdevice\r\n"
            b"NTS: ssdp:alive\r\n"
            b"USN: " + usn.encode() + b"\r\n\r\n")
    pkts.append(Ether(src=f"0a:17:70:00:02:{i:02x}", dst="01:00:5e:7f:ff:fa") /
                IP(src=ip, dst="239.255.255.250") /
                UDP(sport=1900 + i, dport=1900) / Raw(load=body))

for i, src in enumerate(CLIENTS):
    body = (b"M-SEARCH * HTTP/1.1\r\n"
            b"HOST: 239.255.255.250:1900\r\n"
            b"MAN: \"ssdp:discover\"\r\n"
            b"MX: 1\r\n"
            b"ST: ssdp:all\r\n\r\n")
    pkts.append(Ether(src=f"0a:17:70:00:03:{i:02x}", dst="01:00:5e:7f:ff:fa") /
                IP(src=src, dst="239.255.255.250") /
                UDP(sport=49152 + i, dport=1900) / Raw(load=body))

# ----------------------------------------------------------------------------
# Stagger timestamps over ~1 hour, with the change events strictly last so
# history ordering is meaningful.
base = time.time() - 3600
for i, p in enumerate(pkts):
    p.time = base + i * 1.2
for i, p in enumerate(late_pkts):
    p.time = base + len(pkts) * 1.2 + 60 + i * 1.2

pkts += late_pkts
wrpcap(OUT, pkts)
print(f"wrote {len(pkts)} packets -> {OUT}")
