#!/usr/bin/env python3
"""Compare tshark's view of the test pcaps with Arkime's golden .test output.

For every pcap/*.pcap that has a .test golden, tshark is run with a fixed field list and the
result is compared with the golden on two levels:

  * protocol coverage: protocols tshark dissects (frame.protocols) that Arkime never tagged on
    any session of that pcap, and the reverse
  * field values: for a mapping of tshark fields to Arkime fields, the distinct values each
    side extracted, so values tshark found that Arkime did not stand out

The goldens are the Arkime side, so regenerate them (./tests.pl --make) before running this.

    cd tests
    ./compare-tshark.py --report /tmp/tshark-report.md
    ./compare-tshark.py pcap/dns-*.pcap            # a subset
"""

import argparse
import collections
import glob
import ipaddress
import json
import os
import re
import subprocess
import sys

# tshark protocol token -> Arkime protocol tag. Transports and link layers are skipped.
SKIP = {"eth", "ethertype", "ip", "ipv6", "tcp", "udp", "data", "frame", "vlan", "sll", "null", "raw",
        "ieee80211", "wlan", "wlan_radio", "radiotap", "llc", "snap", "ppp", "pppoes", "pppoed", "chdlc",
        "frelay", "mpls", "ipcomp", "x509af", "x509ce", "x509if", "x509sat", "pkix1explicit", "pkix1implicit",
        "ber", "tpkt", "cotp", "nbss", "gss-api", "spnego", "spnego-krb5", "ntlmssp", "rtp", "rtcp", "xml",
        "json", "text", "lines", "media", "mime_multipart", "urlencoded-form", "image-gif", "image-jfif",
        "png", "tcp.segments", "ip.frags", "ipv6.frags", "udpencap", "_ws.malformed", "_ws.unreassembled",
        "_ws.short", "_ws.lua", "fake-field-wrapper", "nbdgm", "nbt", "smb_pipe", "lanman", "dcerpc.cn_bind",
        "ssl.segments", "tls.segments", "http.segments", "sip.segments", "gtpv2", "data-text-lines",
        "ipv6.hopopts", "ipv6.dstopts", "ipv6.routing", "ipv6.fraghdr", "prp", "pkixalgs", "ns_cert_exts",
        "osi", "isis.hello", "isis.csnp", "isis.psnp", "isis.lsp", "cipcls", "cipcm", "cipio", "cmd", "amr",
        "gsm_sms", "gsm_a.rp", "gsm_map", "fr", "loop", "eap", "mtp3", "ieee8021ad", "sdp", "x509if"}
PROTO_MAP = {
    "ssl": "tls", "tls": "tls", "dtls": "dtls", "kerberos": "krb5", "pgsql": "postgresql", "mbtcp": "modbus",
    "bacnet": "bacnet", "bacapp": "bacnet", "bvlc": "bacnet", "cip": "cip", "enip": "enip", "imf": "smtp",
    "icmpv6": "icmp", "mdns": "mdns", "nbns": "nbns", "llmnr": "llmnr", "ssdp": "ssdp", "db-lsp-disc": None,
    "mysql": "mysql", "tds": "tds", "mongo": "mongo", "cql": "cassandra", "kafka": "kafka", "rdp": "rdp",
    "rdpudp": "rdp", "gtp": "gtp", "vxlan": "vxlan", "geneve": "geneve", "gre": "gre", "erspan": "erspan",
    "esp": "esp", "ah": "ah", "isakmp": "isakmp", "ikev2": "isakmp", "smb": "smb", "smb2": "smb",
    "browser": "nbds", "dcerpc": "dcerpc", "drsuapi": "drsuapi", "samr": "samr", "lsarpc": "lsarpc",
    "netlogon": "netlogon", "srvsvc": "srvsvc", "svcctl": "svcctl", "wkssvc": "wkssvc", "dnp3": "dnp3",
    "s7comm": "s7comm", "s7comm-plus": "s7comm-plus", "opcua": "opcua", "diameter": "diameter", "m3ua": "m3ua",
    "sccp": "sccp", "tcap": "tcap", "camel": "camel", "ptp": "ptp", "c1222": "c1222", "synphasor": "synchrophasor",
    "pana": "pana", "adb": "adb", "adb_cs": "adb", "adb_service": "adb", "aerospike": "aerospike",
    "thrift": "thrift", "openvpn": "openvpn", "stun": "stun", "classicstun": "stun", "turnchannel": "stun",
    "quic": "quic", "http2": "http2", "websocket": "websocket", "ospf": "ospf", "isis": "isis", "bgp": "bgp",
    "lldp": "lldp", "pim": "pim", "igmp": "igmp", "sctp": "sctp", "tftp": "tftp", "telnet": "telnet",
    "syslog": "syslog", "dhcpv6": "dhcpv6", "dhcp": "dhcp", "bootp": "dhcp", "ntp": "ntp", "snmp": "snmp",
    "socks": "socks", "mqtt": "mqtt", "ldap": "ldap", "cldap": "ldap", "radius": "radius", "sip": "sip",
    "sdp": None, "ftp": "ftp", "ftp-data": None, "irc": "irc", "pop": "pop3", "imap": "imap", "smtp": "smtp",
    "http": "http", "dns": "dns", "ssh": "ssh", "ntlm": "ntlm", "arp": "arp", "icmp": "icmp", "oracle": "oracle",
    "tns": "oracle", "nflog": None, "bittorrent": "bittorrent", "bt-dht": "bittorrent", "bt-utp": "bittorrent",
    "xmpp": "xmpp", "ldp": None, "udplite": None, "iso": None, "esis": "esis", "clnp": None, "nsh": None,
    "pfcp": None, "epl": None, "rsvp": None, "bfd": None, "mp2t": None, "rtps": None, "gquic": "quic",
    "wsp": None, "ipx": None, "lcp": None, "ipcp": None, "pppoe": "pppoe", "tzsp": None, "sflow": "sflow",
    "cflow": "netflow", "ipfix": "ipfix", "teredo": None, "ayiya": None, "etherip": None, "l2tp": None,
    "vrrp": None, "hsrp": None, "cdp": None, "stp": None, "x11": None, "nfs": None, "rpc": None, "portmap": None,
    "mount": None, "ipp": None, "lpd": None, "vnc": "rfb", "rfb": "rfb", "ssl.alert": None, "ocsp": None,
    "wccp": None, "nat-pmp": None, "rip": None, "ripng": None, "eigrp": None, "babel": None, "ipmi": None,
    "zep": None, "ieee802154": None, "wg": "wireguard", "openflow": None, "ldss": None, "kdp": None,
    "dropbox": None, "spotify": None, "gvsp": None, "gvcp": None, "memcache": None, "redis": "redis", "resp": "redis",
    "modbus": "modbus", "tcap": "tcap", "isup": "isup", "m2ua": "m2ua", "fb_zero": "fbzero", "ah": "ah",
}

# tshark field -> (arkime golden path, normalizer); paths use group.key and [] for object arrays
FIELD_MAP = [
    ("dns.qry.name", "dns[].host", "host"),
    ("dns.a", "dns[].answers[].ip", "ip"),
    ("dns.aaaa", "dns[].answers[].ip", "ip"),
    ("dns.cname", "dns[].answers[].cname", "host"),
    ("dns.mx.mail_exchange", "dns[].answers[].mx", "host"),
    ("dns.ns", "dns[].answers[].nameserver", "host"),
    ("http.host", "http.host", "host"),
    ("http.request.method", "http.method", "str"),
    ("http.user_agent", "http.useragent", "str"),
    ("http.response.code", "http.statuscode", "int"),
    ("http.request.uri", "http.path", "uripath"),
    ("http.referer", "http.request-referer", "str"),
    ("http.request.version", "http.clientVersion", "httpver"),
    ("tls.handshake.extensions_server_name", "http.host", "host"),
    ("tls.handshake.extensions_alpn_str", "tls.clientAlpn", "str"),
    ("x509ce.dNSName", "cert[].alt", "host"),
    ("x509af.serialNumber", "cert[].serial", "serial"),
    ("ssh.protocol", "ssh.version", "sshver"),
    ("ssh.kex.hassh", "ssh.hassh", "str"),
    ("ssh.kex.hasshserver", "ssh.hasshServer", "str"),
    ("dhcp.option.hostname", "dhcp.host", "host"),
    ("dhcp.option.dhcp_server_id", "dhcp.serverIp", "ip"),
    ("dhcp.option.ip_address_lease_time", "dhcp.leaseTime", "int"),
    ("dhcp.option.requested_ip_address", "dhcp.requestIp", "ip"),
    ("dhcp.ip.your", "dhcp.yiaddrIp", "ipnz"),
    ("dhcp.hw.mac_addr", "dhcp.mac", "str"),
    ("imf.subject", "email.subject", "str"),
    ("imf.message_id", "email.id", "msgid"),
    ("ftp.request.command", "ftp.command", "str"),
    ("ftp.response.code", "ftp.responseCode", "int"),
    ("kerberos.CNameString", "krb5.cname", "str"),
    ("kerberos.realm", "krb5.realm", "str"),
    ("ntlmssp.auth.domain", "ntlm.domain", "nonull"),
    ("ntlmssp.auth.hostname", "ntlm.host", "nonull"),
    ("ntlmssp.auth.username", "user", "nonull"),
    ("ntp.stratum", "ntp.stratumNum", "int"),
    ("snmp.community", "snmp.community", "str"),
    ("socks.dst", "socks.ip+socks.boundIp", "ipnz"),
    ("socks.dstport", "socks.port+socks.boundPort", "intnz"),
    ("socks.username", "socks.user", "str"),
    ("socks.remote_name", "socks.host", "host"),
    ("mqtt.topic", "mqtt.topic", "str"),
    ("mqtt.clientid", "mqtt.clientId", "str"),
    ("ldap.name", "ldap.bindname", "str"),
    ("radius.User_Name", "radius.user", "str"),
    ("sip.Method", "sip.method", "str"),
    ("sip.Call-ID", "sip.callid", "str"),
    ("smb2.filename", "smb.filename", "str"),
    ("smb.path", "smb.share", "unesc"),
    ("smb.native_os", "smb.os", "str"),
]

SEP = "\t"
AGG = "|"


def norm(kind, v, side="tshark"):
    v = v.strip()
    if v == "":
        return None
    if kind == "host":
        return v.lower().rstrip(".")
    if kind in ("ip", "ipnz"):
        if kind == "ipnz" and v in ("0.0.0.0", "0.0.0.1", "::"):
            return None
        try:
            return str(ipaddress.ip_address(v))
        except ValueError:
            return v.lower()
    if kind in ("int", "intnz"):
        try:
            n = int(v, 0)
        except ValueError:
            return v
        return None if kind == "intnz" and n == 0 else str(n)
    if kind == "unesc":
        # tshark escapes backslashes in field output, the golden does not
        return (v.replace("\\\\", "\\") if side == "tshark" else v).lower()
    if kind == "nonull":
        return None if v.upper() == "NULL" else v.lower()
    if kind == "uripath":
        return v.split("?", 1)[0]
    if kind == "httpver":
        return v.replace("HTTP/", "")
    if kind == "sshver":
        return v.lower()
    if kind == "serial":
        return v.lower().replace("0x", "").replace(":", "").lstrip("0")
    if kind == "msgid":
        return v.strip("<>").lower()
    return v.lower()


def _walk(obj, parts):
    """Yield values at a path like dns[].answers[].ip, where [] means each element of a list."""
    if not parts:
        if isinstance(obj, list):
            for v in obj:
                yield str(v)
        elif obj is not None:
            yield str(obj)
        return
    key = parts[0]
    if key.endswith("[]"):
        key = key[:-2]
        seq = obj.get(key) if isinstance(obj, dict) else None
        if isinstance(seq, list):
            for item in seq:
                yield from _walk(item, parts[1:])
        elif isinstance(seq, dict):
            yield from _walk(seq, parts[1:])
    elif isinstance(obj, dict):
        yield from _walk(obj.get(key), parts[1:])


def golden_values(sessions, path):
    """Collect values at one or more '+' separated paths across all sessions."""
    out = set()
    for p in path.split("+"):
        parts = p.split(".")
        for s in sessions:
            out.update(_walk(s["body"], parts))
    return out


def golden_protocols(sessions):
    out = set()
    for s in sessions:
        out.update(s["body"].get("protocol", []))
    return out


def run_tshark(pcap, fields):
    cmd = ["tshark", "-n", "-r", pcap, "-T", "fields", "-E", "separator=/t", "-E", "occurrence=a",
           "-E", "aggregator=" + AGG, "-e", "frame.protocols"] + [a for f in fields for a in ("-e", f)]
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, errors="replace")
    protos = set()
    values = collections.defaultdict(set)
    for line in r.stdout.splitlines():
        cols = line.split(SEP)
        for tok in cols[0].split(":"):
            if tok:
                protos.add(tok)
        for f, col in zip(fields, cols[1:]):
            for v in col.split(AGG):
                if v:
                    values[f].add(v)
    return protos, values


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pcaps", nargs="*")
    ap.add_argument("--report", help="write the markdown report here instead of stdout")
    args = ap.parse_args()

    pcaps = args.pcaps or sorted(glob.glob("pcap/*.pcap") + glob.glob("pcap/*.pcapng"))
    fields = [f for f, _, _ in FIELD_MAP]

    proto_tshark = collections.Counter()
    proto_missing = collections.defaultdict(list)     # tshark protocol -> pcaps where Arkime lacks it
    proto_unknown = collections.Counter()             # tshark tokens with no mapping
    arkime_only = collections.defaultdict(list)
    field_both = collections.Counter()
    field_tshark_only = collections.Counter()
    field_arkime_only = collections.Counter()
    field_examples = collections.defaultdict(list)
    field_pcaps = collections.Counter()
    done = 0

    for pcap in pcaps:
        test = re.sub(r"\.pcapn?g?$", ".test", pcap)
        if not os.path.exists(test):
            continue
        try:
            sessions = json.load(open(test, errors="replace"))["sessions3"]
        except Exception as e:
            sys.stderr.write(f"skip {test}: {e}\n")
            continue
        stem = os.path.basename(pcap).rsplit(".", 1)[0]
        sys.stderr.write(f"\r[{done + 1}/{len(pcaps)}] {stem}            ")
        done += 1
        tprotos, tvalues = run_tshark(pcap, fields)
        aprotos = golden_protocols(sessions)

        mapped = set()
        for tok in tprotos:
            if tok in SKIP:
                continue
            if tok in PROTO_MAP:
                name = PROTO_MAP[tok]
                if name is None:
                    continue
            else:
                proto_unknown[tok] += 1
                name = tok
            mapped.add(name)
        for name in mapped:
            proto_tshark[name] += 1
            if name not in aprotos:
                proto_missing[name].append(stem)
        for name in aprotos:
            if name not in mapped and name not in ("tcp", "udp", "icmp", "sctp"):
                arkime_only[name].append(stem)

        for f, path, kind in FIELD_MAP:
            tv = {x for x in (norm(kind, v) for v in tvalues.get(f, ())) if x}
            av = {x for x in (norm(kind, v, "arkime") for v in golden_values(sessions, path)) if x}
            if not tv and not av:
                continue
            field_pcaps[f] += 1
            both = tv & av
            field_both[f] += len(both)
            field_tshark_only[f] += len(tv - av)
            field_arkime_only[f] += len(av - tv)
            for v in sorted(tv - av)[:2]:
                if len(field_examples[f]) < 4:
                    field_examples[f].append(f"{stem}: {v[:60]}")
    sys.stderr.write("\n")

    out = open(args.report, "w") if args.report else sys.stdout
    out.write(f"# tshark vs Arkime\n\nPcaps compared: {done}\n\n")
    out.write("## Protocols tshark dissects that Arkime never tags\n\n| protocol | pcaps with it (tshark) | pcaps where Arkime has no tag | examples |\n|---|---|---|---|\n")
    for name, pcs in sorted(proto_missing.items(), key=lambda kv: -len(kv[1])):
        out.write(f"| {name} | {proto_tshark[name]} | {len(pcs)} | {', '.join(pcs[:5])} |\n")
    out.write("\n## Protocols Arkime tags that tshark does not (after name mapping)\n\n| protocol | pcaps | examples |\n|---|---|---|\n")
    for name, pcs in sorted(arkime_only.items(), key=lambda kv: -len(kv[1])):
        out.write(f"| {name} | {len(pcs)} | {', '.join(pcs[:5])} |\n")
    out.write("\n## Field values\n\nDistinct values per pcap, summed. \"tshark only\" are values tshark extracted that Arkime's golden does not contain.\n\n")
    out.write("| tshark field | arkime field | pcaps | both | tshark only | arkime only | tshark-only examples |\n|---|---|---|---|---|---|---|\n")
    for f, path, _ in FIELD_MAP:
        if not field_pcaps[f]:
            continue
        ex = "<br>".join(e.replace("|", "\\|") for e in field_examples[f])
        out.write(f"| {f} | {path} | {field_pcaps[f]} | {field_both[f]} | {field_tshark_only[f]} | {field_arkime_only[f]} | {ex} |\n")
    if proto_unknown:
        out.write("\n## tshark protocol tokens without a mapping (reported under their tshark name above)\n\n")
        out.write(", ".join(f"{k} ({v})" for k, v in sorted(proto_unknown.items(), key=lambda kv: -kv[1])) + "\n")


if __name__ == "__main__":
    main()
