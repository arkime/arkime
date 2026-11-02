#!/usr/bin/env python3
"""Regenerates the shipped *.dashboard.json files in this directory, and the
plugin ones in capture/plugins, from the spec below, so they keep one layout
language. The JSON files are what ships and what users copy; edit the spec
here, then run
    python3 release/dashboards/generate.py
from the repository root. Not installed (the Makefile only copies *.dashboard.json)."""
import json, os, sys

OUT = sys.argv[1] if len(sys.argv) > 1 else 'release/dashboards'
PLUGIN_OUT = sys.argv[2] if len(sys.argv) > 2 else 'capture/plugins'

# ---- widget helpers -------------------------------------------------------
# the split lengths the widget editor offers (SPLIT_LENGTHS in viewModes.js);
# any other is shown as 5
SPLIT_LENGTHS = (3, 5, 10)
def split_length(length):
    assert length in SPLIT_LENGTHS, f'split length {length} is not one of {SPLIT_LENGTHS}'
    return length
def nav(): return {'viewMode': 'navigation', 'width': 4, 'height': 1}
def timeline(split, title, length=5, w=3, h=2, expression=None, lines=False, log=False, percent=False):
    d = {'viewMode': 'timeline', 'splitField': split, 'splitLength': split_length(length), 'barMode': 'stacked', 'width': w, 'height': h, 'title': title}
    if lines: d['seriesType'] = 'lines'
    if log: d['yScale'] = 'log'
    if percent: d['yScale'] = 'percent'  # each value's share of the plotted values in the bucket
    if expression: d['expression'] = expression
    return d
# a timeline drawn as filled areas: the split field's top values stacked into
# bands (or each value's share, percent), or overlaid on each other
def area(split, title, length=5, w=3, h=2, expression=None, percent=False, overlaid=False, metricType=None):
    d = {'viewMode': 'area', 'splitField': split, 'splitLength': split_length(length), 'barMode': 'grouped' if overlaid else 'stacked', 'width': w, 'height': h, 'title': title}
    if percent: d['yScale'] = 'percent'
    if metricType: d['metricType'] = metricType
    if expression: d['expression'] = expression
    return d
# a field's top values as bars of one metric, with a line of another on its own axis
def combo(field, bars='sessions', line='bytes', length=20, w=2, h=3, title=None, expression=None):
    d = {'field': field, 'viewMode': 'combo', 'width': w, 'height': h, 'length': length,
         'metrics': [bars, line], 'metricType': bars, 'sortMetric': bars}
    if title: d['title'] = title
    if expression: d['expression'] = expression
    return d
# a field's top values as points placed by two metrics, sized by a third (log
# scaled by default: network counts span orders of magnitude)
def scatter(field, x='sessions', y='bytes', size=None, length=50, w=2, h=3, title=None, expression=None, log=True):
    d = {'field': field, 'viewMode': 'scatter', 'width': w, 'height': h, 'length': length,
         'metrics': [x, y, size or ''], 'metricType': x, 'sortMetric': x}
    if log: d['yScale'] = 'log'
    if title: d['title'] = title
    if expression: d['expression'] = expression
    return d
# a week of hours colored by the metric, or one small week per top value of split
def timeofday(title, split=None, length=5, w=2, h=3, expression=None, metricType=None):
    d = {'viewMode': 'timeofday', 'width': w, 'height': h, 'title': title}
    if split: d['splitField'] = split; d['splitLength'] = split_length(length)
    if metricType: d['metricType'] = metricType
    if expression: d['expression'] = expression
    return d
def timeline_plain(title, w=3, h=2, expression=None, metricType=None, lines=False, log=False):
    d = {'viewMode': 'timeline', 'width': w, 'height': h, 'title': title}
    if metricType: d['metricType'] = metricType
    if lines: d['seriesType'] = 'lines'
    if log: d['yScale'] = 'log'
    if expression: d['expression'] = expression
    return d
def metric(title, metricType=None, w=1, h=1, expression=None, agg=None, sparkline=False):
    d = {'viewMode': 'metric', 'width': w, 'height': h, 'title': title}
    if metricType: d['metricType'] = metricType
    if agg: d['metricAgg'] = agg
    if sparkline: d['sparkline'] = True  # the metric over time under the number (a sum)
    if expression: d['expression'] = expression
    return d
def tagcloud(field, length=20, w=2, h=2, title=None, expression=None):
    d = {'field': field, 'viewMode': 'tagcloud', 'width': w, 'height': h, 'length': length}
    if title: d['title'] = title
    if expression: d['expression'] = expression
    return d
def pie(field, length=8, w=2, h=2, title=None, expression=None, other=True):
    d = {'field': field, 'viewMode': 'pie', 'width': w, 'height': h, 'length': length, 'other': other}
    if title: d['title'] = title
    if expression: d['expression'] = expression
    return d
def table(field, length=20, w=2, h=3, title=None, metrics=None, sortMetric=None, expression=None):
    d = {'field': field, 'viewMode': 'table', 'width': w, 'height': h, 'length': length}
    if metrics: d['metrics'] = metrics
    if sortMetric: d['sortMetric'] = sortMetric; d['metricType'] = sortMetric
    if title: d['title'] = title
    if expression: d['expression'] = expression
    return d
def bar(field, length=15, w=2, h=3, title=None, bucketInterval=None, split=None, expression=None, metricType=None, horizontal=False):
    d = {'field': field, 'viewMode': 'bar', 'width': w, 'height': h, 'length': length}
    if bucketInterval: d['bucketInterval'] = bucketInterval
    if split: d['splitField'] = split; d['splitLength'] = 5
    if metricType: d['metricType'] = metricType
    if horizontal: d['orientation'] = 'horizontal'
    if title: d['title'] = title
    if expression: d['expression'] = expression
    return d
def tree(title, src='ip.src', dst='ip.dst', length=500, depth=3, w=4, h=3, expression=None):
    d = {'field': src, 'fields': [src, dst], 'viewMode': 'tree', 'width': w, 'height': h, 'length': length, 'treeDepth': depth, 'title': title}
    if expression: d['expression'] = expression
    return d
def connections(src='ip.src', dst='ip.dst', length=500, w=4, h=3, title=None, expression=None):
    d = {'field': src, 'fields': [src, dst], 'viewMode': 'connections', 'width': w, 'height': h, 'length': length}
    if title: d['title'] = title
    if expression: d['expression'] = expression
    return d
def sankey(fields, length=10, w=4, h=3, title=None, expression=None):
    d = {'field': fields[0], 'fields': fields, 'viewMode': 'sankey', 'width': w, 'height': h, 'length': length}
    if title: d['title'] = title
    if expression: d['expression'] = expression
    return d
def sessions(fields, length=20, w=4, h=3, title=None, expression=None):
    assert len(fields) <= 7, fields
    d = {'field': fields[0], 'fields': fields, 'viewMode': 'sessions', 'width': w, 'height': h, 'length': length}
    if title: d['title'] = title
    if expression: d['expression'] = expression
    return d
def stats(w=2, h=1): return {'viewMode': 'stats', 'width': w, 'height': h}
def timew(w=2, h=1): return {'viewMode': 'time', 'width': w, 'height': h}
def gauge(title, metricType, agg='avg', w=1, h=1):
    return {'viewMode': 'gauge', 'metricType': metricType, 'metricAgg': agg, 'width': w, 'height': h, 'title': title}
def geomap(field, w=2, h=3, title=None, metricType=None, expression=None):
    d = {'field': field, 'viewMode': 'map', 'width': w, 'height': h}
    if metricType: d['metricType'] = metricType
    if title: d['title'] = title
    if expression: d['expression'] = expression
    return d
def heatmap(field, w=2, h=3, metricType=None):
    d = {'field': field, 'viewMode': 'heatmap', 'width': w, 'height': h}
    if metricType: d['metricType'] = metricType
    return d
def treemap(field, w=2, h=3, metricType=None):
    d = {'field': field, 'viewMode': 'treemap', 'width': w, 'height': h}
    if metricType: d['metricType'] = metricType
    return d
# the first field's values down the side, the second's across the top
def matrix(fields, length=10, w=2, h=3, title=None, expression=None):
    d = {'field': fields[0], 'fields': fields, 'viewMode': 'matrix', 'width': w, 'height': h, 'length': length}
    if title: d['title'] = title
    if expression: d['expression'] = expression
    return d
def intersection(fields, w=4, h=3, title=None, expression=None):
    d = {'field': fields[0], 'fields': fields, 'viewMode': 'intersection', 'width': w, 'height': h}
    if title: d['title'] = title
    if expression: d['expression'] = expression
    return d
def markdown(text, w=4, h=1, title=''):
    return {'viewMode': 'markdown', 'width': w, 'height': h, 'title': title, 'text': text}

# the two-up "who talks to whom" row every protocol dashboard ends with
def clients(): return bar('ip.src', title='Clients', length=15)
def servers(): return table('ip.dst:port', title='Servers', length=20, metrics=['sessions', 'bytes'], sortMetric='sessions')

# a protocol dashboard: the common frame around the protocol's own widgets
def protocol(name, proto, folder, own, cols, split, desc=None, expression=None, tree_title=None, extra_metrics=()):
    expression = expression or f'protocols == {proto}'
    widgets = [nav(),
               timeline(split, f'{name} sessions by {split}'),
               metric(f'{name} sessions'),
               metric(f'{name} bytes', 'bytes')]
    widgets += list(own)
    widgets += [clients(), servers()]
    if tree_title: widgets.append(tree(tree_title))
    widgets.append(sessions(cols))
    return {'name': name, 'description': desc or f'{name} sessions: {expression}',
            'data': {'version': 1, 'folder': folder, 'colorScheme': 'rainbow', 'expression': expression, 'widgets': widgets}}

def plain(name, desc, widgets, folder=None, expression=None, requires=None):
    data = {'version': 1}
    if folder: data['folder'] = folder
    data['colorScheme'] = 'rainbow'
    if expression: data['expression'] = expression
    data['widgets'] = widgets
    d = {'name': name, 'description': desc}
    # a plugin's dashboard stays hidden until the plugin's fields exist
    if requires: d['requires'] = requires
    d['data'] = data
    return d

P, A = 'Protocols', 'Analysis'
D = {}
# dashboards that ship beside a capture plugin, written to PLUGIN_OUT
PLUGINS = {}

# ---- top level ------------------------------------------------------------
D['overview'] = plain('Overview', 'The starter dashboard: capture statistics, top talkers, protocols and the session timeline', [
    nav(), stats(), timew(),
    timeline_plain('Sessions over time', w=4),
    timeofday('Sessions by hour of the week'),
    area('protocols', 'Protocol share over time', length=10, w=2, h=3, percent=True),
    geomap('country.src', title='Source countries'),
    bar('ip', length=15, title='Top IPs'),
    bar('protocols', length=12, title='Protocols'),
    table('ip.dst:port', w=2, h=3, metrics=['sessions', 'bytes', 'packets'], sortMetric='bytes', title='Destinations by bytes'),
    heatmap('ip.dst', metricType='packets'),
    treemap('ip.src', metricType='bytes'),
])

D['connections'] = plain('Connections', 'Sessions by endpoint, port, protocol and size, with the connection graph and tree', [
    nav(),
    area('protocols', 'Sessions by protocol', length=10, w=4),
    timeline('protocols', 'Protocol share over time', length=10, w=4, percent=True),
    stats(),
    gauge('Avg bytes/session', 'bytes'),
    gauge('Avg packets/session', 'packets'),
    table('ip.src', length=20, metrics=['sessions', 'bytes'], sortMetric='bytes', title='Sources by bytes'),
    table('ip.dst', length=20, metrics=['sessions', 'bytes'], sortMetric='bytes', title='Destinations by bytes'),
    combo('port.dst', title='Top destination ports: sessions and bytes'),
    bar('packets', length=20, bucketInterval=10, title='Sessions by packet count (ranges of 10)'),
    pie('ip.protocol', title='IP protocols'),
    bar('session.length', length=20, bucketInterval=10000, title='Session length in ms (ranges of 10 s)'),
    matrix(['ip.protocol', 'port.dst'], length=15, title='IP protocols by destination port'),
    metric('SYN with no SYN-ACK', expression='tcpflags.syn > 0 && tcpflags.syn-ack == 0', sparkline=True),
    gauge('Avg initial RTT, ms', 'initRTT'),
    metric('Bytes sent', 'bytes.src', sparkline=True), metric('Bytes received', 'bytes.dst', sparkline=True),
    table('oui.src', h=2, title='Source MAC vendors'), table('oui.dst', h=2, title='Destination MAC vendors'),
    geomap('country.src', h=2, metricType='bytes', title='Source countries by bytes'),
    geomap('country.dst', h=2, metricType='bytes', title='Destination countries by bytes'),
    scatter('ip.src', size='packets', w=4, title='Sources by sessions and bytes (point size: packets)'),
    sankey(['ip.src', 'port.dst', 'ip.dst'], title='Top paths: source, destination port, destination'),
    connections(title='Connection graph'),
    tree('Connection tree from the busiest source (click a host to re-root)'),
])

# ---- Protocols --------------------------------------------------------------
D['http'] = protocol('HTTP', 'http', P, [
    tagcloud('host.http'), pie('http.method'),
    pie('http.bodymagic', title='Response content types'), table('http.user', h=2, title='HTTP users'),
    table('http.uri'), table('http.user-agent'),
    pie('http.version', title='HTTP versions'), matrix(['http.method', 'http.statuscode'], title='Methods by status code'),
], ['ip.src', 'ip.dst', 'http.method', 'host.http', 'http.uri', 'http.statuscode'], 'http.statuscode')

D['dns'] = protocol('DNS', 'dns', P, [
    tagcloud('dns.query.host'), pie('dns.status'),
    table('dns.query.host'), table('ip.dns', title='Answer IPs'),
    table('dns.query.host', title='NXDOMAIN queries', expression='dns.status == NXDOMAIN'),
    combo('ip.dst', length=10, title='Resolvers: queries and bytes'),
    pie('dns.query.type', title='Query types'), pie('dns.answer.type', title='Answer types'),
    table('dns.puny', h=2, title='Punycode names'), table('dns.opcode', h=2, title='Opcodes'),
    matrix(['dns.query.type', 'dns.status'], w=4, h=2, title='Query types by status'),
], ['ip.src', 'ip.dst', 'dns.query.host', 'dns.query.type', 'dns.status', 'ip.dns'], 'dns.query.type')

D['tls'] = protocol('TLS', 'tls', P, [
    tagcloud('cert.subject.cn'), pie('tls.cipher'),
    pie('tls.alpn', title='ALPN'), pie('tls.version'),
    table('tls.ja4'), table('tls.ja3'),
    table('cert.issuer.on', title='Certificate issuers'),
    bar('cert.remainingDays', length=20, bucketInterval=30, title='Days until certificate expiry (ranges of 30)'),
    area('tls.version', 'TLS version share over time', w=4, percent=True),
], ['ip.src', 'ip.dst', 'tls.version', 'tls.cipher', 'cert.subject.cn', 'tls.ja4'], 'tls.version')

D['quic'] = protocol('QUIC', 'quic', P, [
    tagcloud('host.quic'), pie('quic.user-agent'),
    table('host.quic'), table('quic.version'),
], ['ip.src', 'ip.dst', 'quic.version', 'host.quic', 'quic.user-agent'], 'quic.version')

D['ssh'] = protocol('SSH', 'ssh', P, [
    tagcloud('ssh.hassh'), pie('ssh.cipher'),
    table('ssh.ver.src', title='Client versions'), table('ssh.ver.dst', title='Server versions'),
    table('ssh.key', title='Host keys'), table('ssh.kex'),
    table('ssh.hasshServer', h=2, title='Server HASSH'), table('ssh.hostKeyAlg', h=2, title='Host key algorithms'),
    table('ssh.mac', h=2, title='MAC algorithms'), table('ssh.compression', h=2, title='Compression'),
], ['ip.src', 'ip.dst', 'ssh.ver', 'ssh.hassh', 'ssh.key'], 'ssh.ver', tree_title='SSH tree: who reached what (click a host to re-root)')

D['smb'] = protocol('SMB', 'smb', P, [
    tagcloud('smb.share'), pie('smb.dialect'),
    table('smb.fn', title='Files'), table('smb.user', title='Users'),
    table('smb.os', title='Operating systems'), table('host.smb', title='Hosts'),
], ['ip.src', 'ip.dst', 'smb.user', 'smb.share', 'smb.fn'], 'smb.dialect', tree_title='SMB tree: who reached what (click a host to re-root)')

D['smtp'] = protocol('SMTP', 'smtp', P, [
    tagcloud('email.subject'), pie('email.content-type'),
    table('email.src', title='Senders'), table('email.dst', title='Recipients'),
    table('email.fn', title='Attachments'), table('email.x-mailer', title='Mailers'),
], ['ip.src', 'ip.dst', 'email.src', 'email.dst', 'email.subject', 'email.fn'], 'host.email')

D['dhcp'] = protocol('DHCP', 'dhcp', P, [
    tagcloud('dhcp.host'), pie('dhcp.oui'),
    table('dhcp.mac'), table('dhcp.id'),
    table('dhcp.yiaddr', title='Assigned IPs'), table('dhcp.serverIp', title='DHCP servers'),
    intersection(['dhcp.yiaddr', 'dhcp.mac', 'dhcp.host'], title='Assigned IPs by MAC and host name'),
], ['ip.src', 'ip.dst', 'dhcp.type', 'dhcp.host', 'dhcp.mac', 'dhcp.yiaddr'], 'dhcp.type')

D['krb5'] = protocol('Kerberos', 'krb5', P, [
    tagcloud('krb5.cname', title='Clients (cname)'), pie('krb5.etype'),
    table('krb5.sname', title='Services (sname)'), table('krb5.realm'),
    pie('krb5.msgType', title='Message types'), table('krb5.error', h=2, title='Errors'),
    pie('krb5.options', title='Ticket options'), pie('krb5.ticketEtype', title='Ticket encryption types'),
], ['ip.src', 'ip.dst', 'krb5.realm', 'krb5.cname', 'krb5.sname', 'krb5.error'], 'krb5.msgType', tree_title='Kerberos tree: clients to KDCs and services (click a host to re-root)')

D['ldap'] = protocol('LDAP', 'ldap', P, [
    tagcloud('ldap.bindname'), pie('ldap.authtype'),
    table('ldap.result'), table('ldap.op', title='Operations'),
], ['ip.src', 'ip.dst', 'ldap.op', 'ldap.authtype', 'ldap.bindname', 'ldap.result'], 'ldap.op')

D['mysql'] = protocol('MySQL', 'mysql', P, [
    tagcloud('mysql.user'), pie('mysql.ver'),
], ['ip.src', 'ip.dst', 'mysql.user', 'mysql.ver'], 'mysql.user')

D['postgresql'] = protocol('PostgreSQL', 'postgresql', P, [
    tagcloud('postgresql.db'), pie('postgresql.app'),
    table('postgresql.user', h=2, title='Users'), table('postgresql.db', h=2, title='Databases'),
], ['ip.src', 'ip.dst', 'postgresql.user', 'postgresql.db', 'postgresql.app'], 'postgresql.user')

D['rdp'] = protocol('RDP', 'rdp', P, [
    tagcloud('rdp.client-name'), pie('rdp.keyboard-layout'),
    table('rdp.client-build'), table('rdp.requested-protocols'),
], ['ip.src', 'ip.dst', 'rdp.client-name', 'rdp.client-build', 'rdp.selected-protocol'], 'rdp.selected-protocol', tree_title='RDP tree: who reached what (click a host to re-root)')

D['sip'] = protocol('SIP', 'sip', P, [
    tagcloud('sip.user-agent'), pie('sip.statuscode'),
    table('sip.from'), table('sip.to'),
], ['ip.src', 'ip.dst', 'sip.method', 'sip.from', 'sip.to', 'sip.statuscode'], 'sip.method')

D['snmp'] = protocol('SNMP', 'snmp', P, [
    tagcloud('snmp.community'), pie('snmp.type'),
    table('snmp.variable'), table('snmp.trap-oid'),
], ['ip.src', 'ip.dst', 'snmp.version', 'snmp.community', 'snmp.type'], 'snmp.version')

D['ftp'] = protocol('FTP', 'ftp', P, [
    tagcloud('ftp.filename', title='Files'), pie('ftp.responseCode', title='Reply codes'),
    table('ftp.command', title='Commands'), table('ftp.responseMsg', title='Replies'),
    table('ftp.banner', h=2, title='Server banners'), table('ftp.filename', h=2, title='Files'),
], ['ip.src', 'ip.dst', 'ftp.command', 'ftp.filename', 'ftp.responseCode'], 'ftp.command')

D['dcerpc'] = protocol('DCE/RPC', 'dcerpc', P, [
    tagcloud('dcerpc.interface', title='Interfaces'), pie('dcerpc.msgType', title='Message types'),
    table('dcerpc.uuid', title='Interface UUIDs'), table('dcerpc.opnum', title='Operation numbers'),
], ['ip.src', 'ip.dst', 'dcerpc.interface', 'dcerpc.opnum', 'dcerpc.msgType'], 'dcerpc.msgType', tree_title='DCE/RPC tree: who reached what (click a host to re-root)')

D['ntlm'] = protocol('NTLM', 'ntlm', P, [
    tagcloud('ntlm.host', title='Hosts'), pie('ntlm.domain', title='Domains'),
    table('ntlm.targetName', title='Target names'), table('ntlm.version', title='Versions'),
    table('ntlm.dnsDomainName', h=2, title='DNS domains'), table('ntlm.nbComputerName', h=2, title='NetBIOS computer names'),
], ['ip.src', 'ip.dst', 'ntlm.domain', 'ntlm.host', 'ntlm.targetName', 'ntlm.type'], 'ntlm.type', tree_title='NTLM tree: who authenticated where (click a host to re-root)')

D['ntp'] = protocol('NTP', 'ntp', P, [
    tagcloud('ntp.ref-id', title='Reference IDs'), pie('ntp.stratum'),
    pie('ntp.version', title='Versions'), bar('ntp.poll', h=2, title='Polling intervals'),
], ['ip.src', 'ip.dst', 'ntp.mode', 'ntp.stratum', 'ntp.version', 'ntp.ref-id'], 'ntp.mode')

D['radius'] = protocol('RADIUS', 'radius', P, [
    tagcloud('radius.user', title='Users'), pie('radius.msgType', title='Message types'),
    table('radius.nasIp', title='NAS IPs'), table('radius.framed-ip', title='Framed IPs'),
    table('radius.mac', h=2, title='Client MACs'), table('radius.endpoint-ip', h=2, title='Endpoint IPs'),
], ['ip.src', 'ip.dst', 'radius.user', 'radius.msgType', 'radius.nasIp', 'radius.framed-ip'], 'radius.msgType')

D['stun'] = protocol('STUN', 'stun', P, [
    tagcloud('stun.software', title='Software'), pie('stun.type', title='Message types'),
    table('stun.mapped-ip', title='Mapped IPs'), table('stun.xor-mapped-ip', title='XOR mapped IPs'),
    table('stun.username', h=2, title='Usernames'), table('stun.realm', h=2, title='Realms'),
], ['ip.src', 'ip.dst', 'stun.type', 'stun.mapped-ip', 'stun.xor-mapped-ip', 'stun.software'], 'stun.type')

D['mqtt'] = protocol('MQTT', 'mqtt', P, [
    tagcloud('mqtt.topic', title='Topics'), pie('mqtt.version', title='Versions'),
    table('mqtt.clientId', title='Client IDs'), table('mqtt.topic', title='Topics'),
    pie('mqtt.qos', title='QoS'), table('mqtt.connackCode', h=2, title='CONNACK codes'),
], ['ip.src', 'ip.dst', 'mqtt.clientId', 'mqtt.type', 'mqtt.topic'], 'mqtt.type')

D['websocket'] = protocol('WebSocket', 'websocket', P, [
    tagcloud('host.http', title='Hosts'), pie('websocket.closeCode', title='Close codes'),
    table('http.uri', title='URIs'), table('http.user-agent', title='User agents'),
    bar('websocket.frameCnt', h=2, bucketInterval=10, title='Frames per session (ranges of 10)'), table('websocket.textSample', h=2, title='Text samples'),
], ['ip.src', 'ip.dst', 'host.http', 'http.uri', 'websocket.opcode', 'websocket.frameCnt'], 'websocket.opcode')

D['irc'] = protocol('IRC', 'irc', P, [
    tagcloud('irc.nick', title='Nicks'), pie('irc.channel', title='Channels'),
    table('irc.channel', h=2, title='Channels'), table('irc.nick', h=2, title='Nicks'),
], ['ip.src', 'ip.dst', 'irc.channel', 'irc.nick'], 'irc.channel')

D['tftp'] = protocol('TFTP', 'tftp', P, [
    tagcloud('tftp.filename', title='Files'), pie('tftp.opcode', title='Opcodes'),
    table('tftp.filename', h=2, title='Files'), table('ip.dst', h=2, title='TFTP servers'),
], ['ip.src', 'ip.dst', 'tftp.opcode', 'tftp.filename'], 'tftp.opcode')

D['isakmp'] = protocol('IPsec IKE', 'isakmp', P, [
    tagcloud('isakmp.vendor-id', title='Vendor IDs'), pie('isakmp.version', title='IKE versions'),
    table('isakmp.encryption', title='Encryption'), table('isakmp.dh-group', title='DH groups'),
    table('isakmp.hash', h=2, title='Hashes'), table('isakmp.auth-method', h=2, title='Authentication methods'),
], ['ip.src', 'ip.dst', 'isakmp.version', 'isakmp.exchange-type', 'isakmp.encryption', 'isakmp.dh-group'], 'isakmp.exchange-type',
    desc='IPsec IKE / ISAKMP sessions: protocols == isakmp')

D['socks'] = protocol('SOCKS', 'socks', P, [
    tagcloud('host.socks', title='Requested hosts'), pie('socks.version', title='Versions'),
    table('ip.socks', title='Requested IPs'), table('port.socks', title='Requested ports'),
    table('socks.user', h=2, title='Users'), pie('socks.status', title='Results'),
], ['ip.src', 'ip.dst', 'socks.version', 'socks.user', 'host.socks', 'ip.socks', 'socks.status'], 'socks.status')

D['nbns'] = protocol('NetBIOS Names', 'nbns', P, [
    tagcloud('nbns.name', title='Names'), pie('nbns.queryType', title='Query types'),
    table('nbns.queryHost', h=2, title='Queried hosts'), table('nbns.ip', h=2, title='Answer IPs'),
], ['ip.src', 'ip.dst', 'nbns.queryHost', 'nbns.queryType', 'nbns.name', 'nbns.ip'], 'nbns.queryType',
    desc='NetBIOS name service sessions: protocols == nbns')

D['ssdp'] = protocol('SSDP', 'ssdp', P, [
    tagcloud('ssdp.server', title='Servers'), pie('ssdp.st', title='Search targets'),
    table('ssdp.location', title='Locations'), table('ssdp.usn', title='USNs'),
    table('ssdp.userAgent', h=2, title='User agents'), table('ssdp.nt', h=2, title='Notification types'),
], ['ip.src', 'ip.dst', 'ssdp.st', 'ssdp.server', 'ssdp.location', 'ssdp.usn'], 'ssdp.st',
    desc='SSDP / UPnP discovery sessions: protocols == ssdp')

D['adb'] = protocol('ADB', 'adb', P, [
    tagcloud('adb.serial', title='Device serials'), pie('adb.systemtype', title='System types'),
    table('adb.service', title='Services'), table('adb.sync.path', title='Synced paths'),
    table('adb.shell.exitcode', h=2, title='Shell exit codes'), table('adb.forward.remote', h=2, title='Forwarded remotes'),
], ['ip.src', 'ip.dst', 'adb.command', 'adb.serial', 'adb.service', 'adb.systemtype'], 'adb.command',
    desc='Android Debug Bridge sessions: protocols == adb')

D['diameter'] = protocol('Diameter', 'diameter', P, [
    tagcloud('diameter.user', title='Users'), pie('diameter.resultCode', title='Result codes'),
    table('diameter.origin-host', title='Origin hosts'), table('diameter.dest-host', title='Destination hosts'),
    table('diameter.app', h=2, title='Applications'), table('diameter.origin-realm', h=2, title='Origin realms'),
], ['ip.src', 'ip.dst', 'diameter.cmd', 'diameter.user', 'diameter.origin-host', 'diameter.resultCode'], 'diameter.cmd')

D['ss7'] = protocol('SS7', 'ss7', P, [
    tagcloud('camel.imsi', title='IMSIs'), pie('camel.op', title='CAMEL operations'),
    table('sccp.src', title='SCCP calling parties'), table('sccp.dst', title='SCCP called parties'),
    table('camel.calling', h=2, title='Calling numbers'), table('camel.called', h=2, title='Called numbers'),
    table('tcap.opcode', h=2, title='TCAP opcodes'), table('m3ua.si', h=2, title='M3UA service indicators'),
], ['ip.src', 'ip.dst', 'protocols', 'sccp.src', 'sccp.dst', 'camel.op', 'camel.imsi'], 'protocols',
    expression='protocols == [sccp, tcap, camel, m3ua, m2ua, isup]',
    desc='SS7 over IP (SIGTRAN) sessions: SCCP, TCAP, CAMEL, M3UA, M2UA and ISUP')

D['oracle'] = protocol('Oracle', 'oracle', P, [
    tagcloud('oracle.service', title='Services'), pie('oracle.user', title='Users'),
    table('oracle.host', h=2, title='Hosts'), table('oracle.user', h=2, title='Users'),
], ['ip.src', 'ip.dst', 'oracle.user', 'oracle.service', 'oracle.host'], 'oracle.user',
    desc='Oracle TNS sessions: protocols == oracle')

D['arp'] = plain('ARP', 'ARP sessions: protocols == arp', [
    nav(),
    timeline('arp.opcode', 'ARP sessions by arp.opcode (1 request, 2 reply)'),
    metric('ARP sessions'), metric('ARP packets', 'packets'),
    tagcloud('arp.ip', title='IPs'), pie('arp.oui', title='Vendors (OUI)'),
    table('arp.mac', title='MACs'), table('arp.ip', title='IPs'),
    bar('mac.src', title='Source MACs'), bar('mac.dst', title='Destination MACs'),
    sessions(['mac.src', 'mac.dst', 'arp.opcode', 'arp.ip', 'arp.mac', 'arp.oui']),
], folder=P, expression='protocols == arp')

# ---- Industrial protocols -----------------------------------------------------
D['modbus'] = protocol('Modbus', 'modbus', P, [
    tagcloud('modbus.unitid', title='Unit IDs'), pie('modbus.exccode', title='Exception codes'),
    table('modbus.funccode', title='Function codes'), table('modbus.transactionid', title='Transaction IDs'),
], ['ip.src', 'ip.dst', 'modbus.unitid', 'modbus.funccode', 'modbus.exccode'], 'modbus.funccode')

D['dnp3'] = protocol('DNP3', 'dnp3', P, [
    tagcloud('dnp3.src', title='DNP3 sources'), pie('dnp3.dst', title='DNP3 destinations'),
    table('dnp3.funcName', title='Functions'), table('dnp3.func', title='Function codes'),
], ['ip.src', 'ip.dst', 'dnp3.src', 'dnp3.dst', 'dnp3.funcName'], 'dnp3.funcName')

D['bacnet'] = protocol('BACnet', 'bacnet', P, [
    tagcloud('bacnet.object-name', title='Object names'), pie('bacnet.apdu-type', title='APDU types'),
    table('bacnet.function', title='Functions'), table('bacnet.service', title='Services'),
], ['ip.src', 'ip.dst', 'bacnet.service', 'bacnet.apdu-type', 'bacnet.object-name'], 'bacnet.service')

D['enip'] = protocol('EtherNet/IP', 'enip', P, [
    tagcloud('enip.product', title='Products'), pie('enip.status', title='Statuses'),
    table('enip.vendor', title='Vendors'), table('enip.device-type', title='Device types'),
    table('enip.service', h=2, title='Services'), table('enip.class', h=2, title='Classes'),
], ['ip.src', 'ip.dst', 'enip.command', 'enip.product', 'enip.vendor'], 'enip.command')

D['s7comm'] = protocol('S7comm', 's7comm', P, [
    tagcloud('s7comm.funcName', title='Functions'), pie('s7comm.rosctr', title='ROSCTR'),
    table('s7comm.opcode', title='Opcodes'), table('s7comm.errorClass', title='Error classes'),
], ['ip.src', 'ip.dst', 's7comm.funcName', 's7comm.rosctr', 's7comm.errorCode'], 's7comm.funcName')

D['opcua'] = protocol('OPC UA', 'opcua', P, [
    tagcloud('opcua.endpointUrl', title='Endpoint URLs'), pie('opcua.securityPolicyUri', title='Security policies'),
    table('opcua.endpointUrl', h=2, title='Endpoint URLs'), table('ip.dst', h=2, title='OPC UA servers'),
], ['ip.src', 'ip.dst', 'opcua.endpointUrl', 'opcua.securityPolicyUri'], 'opcua.securityPolicyUri')

D['synchrophasor'] = protocol('Synchrophasor', 'synchrophasor', P, [
    tagcloud('synchrophasor.station', title='Stations'), pie('synchrophasor.cmd', title='Commands'),
    table('synchrophasor.idcode', title='ID codes'), table('synchrophasor.dataRate', title='Data rates'),
    table('synchrophasor.numPMU', h=2, title='PMU counts'), table('synchrophasor.frameType', h=2, title='Frame types'),
], ['ip.src', 'ip.dst', 'synchrophasor.frameType', 'synchrophasor.station', 'synchrophasor.idcode', 'synchrophasor.cmd'], 'synchrophasor.frameType',
    desc='Synchrophasor (IEEE C37.118) sessions: protocols == synchrophasor')

D['c1222'] = protocol('C12.22', 'c1222', P, [
    tagcloud('c1222.callingApTitle', title='Calling AP titles'), pie('c1222.securityMode', title='Security modes'),
    table('c1222.calledApTitle', h=2, title='Called AP titles'), table('c1222.responseControl', h=2, title='Response controls'),
], ['ip.src', 'ip.dst', 'c1222.callingApTitle', 'c1222.calledApTitle', 'c1222.securityMode'], 'c1222.securityMode',
    desc='ANSI C12.22 smart meter sessions: protocols == c1222')

# ---- Analysis (cross-protocol) -------------------------------------------------
ICS = 'protocols == [modbus, dnp3, bacnet, enip, cip, s7comm, s7comm-plus, opcua, synchrophasor, c1222, omron-fins, ptp]'
D['ot'] = plain('OT', 'Operational technology (ICS) sessions: Modbus, DNP3, BACnet, EtherNet/IP, CIP, S7comm, OPC UA, Synchrophasor, C12.22, Omron FINS and PTP', [
    nav(),
    timeline('protocols', 'ICS sessions by protocol', length=10),
    metric('ICS sessions', sparkline=True), metric('ICS bytes', 'bytes', sparkline=True),
    timeofday('ICS sessions by hour of the week', h=2), geomap('country.src', h=2, title='Source countries'),
    pie('protocols', length=12, title='ICS protocols'),
    table('ip.src', title='ICS clients', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    table('ip.dst:port', title='ICS servers', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    pie('ptp.msgType', h=2, title='PTP message types', expression='protocols == ptp'),
    table('ptp.clockId', h=2, title='PTP clocks', expression='protocols == ptp'),
    tree('ICS tree: who reached what (click a host to re-root)'),
    sessions(['ip.src', 'ip.dst', 'port.dst', 'protocols', 'bytes', 'packets']),
], folder=A, expression=ICS)

D['certificates'] = plain('Certificates', 'Sessions that carried an X.509 certificate: subjects, issuers, algorithms and expiry', [
    nav(),
    timeline('cert.publicAlgorithm', 'Sessions with certificates by public key algorithm'),
    metric('Sessions with certificates'), metric('Expiring within 30 days or expired', expression='cert.remainingDays < 30'),
    tagcloud('cert.subject.cn', title='Subject common names'), pie('cert.signatureAlgorithm', title='Signature algorithms'),
    table('cert.issuer.on', title='Issuers'), table('cert.subject.on', title='Subject organizations'),
    bar('cert.keyLength', h=2, title='Key lengths'),
    bar('cert.remainingDays', length=20, h=2, bucketInterval=30, title='Days until expiry (ranges of 30)'),
    table('cert.hash', title='Certificate hashes'), table('cert.alt', title='Alternative names'),
    pie('cert.curve', title='Curves'),
    bar('cert.validfor', length=20, bucketInterval=365, title='Validity length in days (ranges of a year)'),
    sessions(['ip.src', 'ip.dst', 'port.dst', 'cert.subject.cn', 'cert.issuer.on', 'cert.notafter', 'cert.hash']),
], folder=A, expression='cert.cnt > 0')

TUNNELS = 'protocols == [gre, vxlan, vxlan-gpe, geneve, erspan, gtp, esp, ah, openvpn, wireguard, mpls, pppoe, ppp]'
D['tunnels'] = plain('Tunnels', 'Encapsulated sessions: GRE, VXLAN, Geneve, ERSPAN, GTP, IPsec ESP/AH, OpenVPN, WireGuard, MPLS and PPP', [
    nav(),
    timeline('protocols', 'Tunnel sessions by protocol', length=10),
    metric('Tunnel sessions'), metric('Tunnel bytes', 'bytes'),
    pie('protocols', length=12, title='Tunnel types'), tagcloud('vni', title='VNIs'),
    table('outerip.src', title='Outer sources', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    table('outerip.dst', title='Outer destinations', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    table('ip.src', title='Inner sources'), table('ip.dst:port', title='Inner destinations', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    tree('Tunnel endpoint tree (outer IPs, click a host to re-root)', src='outerip.src', dst='outerip.dst'),
    sessions(['outerip.src', 'outerip.dst', 'ip.src', 'ip.dst', 'protocols', 'vni', 'bytes']),
], folder=A, expression=TUNNELS)

# email.bodymagic (an encoded MIME part, in practice an attachment) rather than
# email.fn: searching by email fields needs the emailSearch permission, and a
# dashboard filter that fails takes every widget with it
FILES = 'http.md5 == EXISTS! || email.bodymagic == EXISTS! || smb.fn == EXISTS! || ftp.filename == EXISTS! || tftp.filename == EXISTS!'
EXE = 'http.bodymagic == [application/x-dosexec, application/x-executable, application/x-msdownload, application/vnd.microsoft.portable-executable, application/x-elf, application/x-mach-binary, application/zip, application/x-rar, application/x-7z-compressed, application/gzip, application/x-gzip]'
D['files'] = plain('Files', 'Sessions that carried a file: HTTP bodies, SMB and FTP files and email attachments', [
    nav(),
    timeline('protocols', 'Sessions with files by protocol', length=10),
    metric('Sessions with files'), metric('Executables and archives', expression=EXE),
    tagcloud('http.bodymagic', title='HTTP body types'), pie('email.bodymagic', title='Email attachment types'),
    table('smb.fn', title='SMB files'), table('email.fn', title='Email attachments'),
    table('ftp.filename', h=2, title='FTP files'), table('tftp.filename', h=2, title='TFTP files'),
    table('http.uri', title='Executable and archive URIs', expression=EXE), table('http.md5', title='HTTP body MD5s'),
    table('host.http', title='Hosts serving executables and archives', expression=EXE),
    table('ip.src', title='Clients fetching executables and archives', expression=EXE),
    sessions(['ip.src', 'ip.dst', 'protocols', 'http.uri', 'smb.fn', 'email.fn', 'ftp.filename']),
], folder=A, expression=FILES)

D['software'] = plain('Software', 'Client and server software seen on the wire: user agents, versions and operating systems', [
    nav(),
    timeline('http.user-agent', 'User agent share over time', w=4, percent=True),
    metric('HTTP user agents', expression='http.user-agent == EXISTS!'),
    metric('SSH versions', expression='ssh.ver == EXISTS!'),
    metric('SMB operating systems', expression='smb.os == EXISTS!'),
    metric('DHCP class IDs', expression='dhcp.classId == EXISTS!'),
    table('http.user-agent', title='HTTP user agents'), table('quic.user-agent', title='QUIC user agents'),
    table('ssh.ver.src', title='SSH clients'), table('ssh.ver.dst', title='SSH servers'),
    table('smb.os', h=2, title='SMB operating systems'), table('smb.ver', h=2, title='SMB versions'),
    table('dhcp.classId', h=2, title='DHCP class IDs'), table('rdp.client-build', h=2, title='RDP client builds'),
    table('sip.user-agent', h=2, title='SIP user agents'), table('ssdp.server', h=2, title='SSDP servers'),
    table('email.x-mailer', h=2, title='Mailers'), table('stun.software', h=2, title='STUN software'),
], folder=A)

LEGACY_TLS = 'tls.version == ["SSLv2", "SSLv3", "TLSv1", "TLSv1.1"]'
SMB1 = 'smb.dialect == ["NT LM 0.12", "NT LANMAN 1.0", "PC NETWORK PROGRAM 1.0", "LANMAN1.0", "LM1.2X002", "LANMAN2.1"]'
CLEARTEXT = 'http.authtype == basic || ftp.command == PASS || protocols == [telnet, pop3, imap]'
REMOTE = 'protocols == [ssh, rdp, smb, krb5, dcerpc, ntlm, winrm, vnc, telnet]'
# only core fields: intel (zeekintel, MISP) fields exist only where that plugin or source runs
D['security'] = plain('Security', 'Weak and legacy protocols, cleartext logins, failed lookups and remote access paths', [
    nav(),
    timeline('protocols', 'Remote access sessions by protocol', length=10, expression=REMOTE),
    metric('Legacy TLS', expression=LEGACY_TLS, sparkline=True), metric('SMBv1', expression=SMB1, sparkline=True),
    metric('Cleartext logins', expression=CLEARTEXT, sparkline=True), metric('SSH v1', expression='ssh.ver == "ssh-1.*"', sparkline=True),
    metric('NXDOMAIN lookups', expression='dns.status == NXDOMAIN', sparkline=True), metric('Remote access sessions', expression=REMOTE, sparkline=True),
    timeofday('Remote access by hour of the week', split='protocols', length=5, w=4, expression=REMOTE),
    matrix(['ip.src', 'protocols'], title='Remote access clients by protocol', expression=REMOTE),
    pie('tls.version', title='TLS versions', expression='protocols == tls'), bar('http.authtype', h=2, title='HTTP authentication types'),
    table('ip.src', title='Clients using legacy TLS', expression=LEGACY_TLS),
    table('ip.dst:port', title='Cleartext login servers', metrics=['sessions', 'bytes'], sortMetric='sessions', expression=CLEARTEXT),
    table('dns.query.host', title='NXDOMAIN queries', expression='dns.status == NXDOMAIN'),
    scatter('ip.src', title='Remote access clients by sessions and bytes', expression=REMOTE),
    tree('Remote access tree: SSH, RDP, SMB, Kerberos, DCE/RPC, NTLM, WinRM, VNC, Telnet (click a host to re-root)', expression=REMOTE),
    sessions(['ip.src', 'ip.dst', 'port.dst', 'protocols', 'user', 'http.authtype'], title='Cleartext login sessions', expression=CLEARTEXT),
], folder=A)

ROUTING = 'protocols == [ospf, bgp, isis]'
D['routing'] = plain('Routing', 'Routing protocol sessions: OSPF, BGP and IS-IS routers, areas and message types', [
    nav(),
    timeline('protocols', 'Routing sessions by protocol', length=3),
    metric('Routing sessions'), metric('Routers', 'ip.src', agg='cardinality'),
    pie('protocols', length=5, title='Routing protocols'), pie('ospf.msgType', title='OSPF message types'),
    table('ospf.routerId', h=2, title='OSPF router IDs'), table('ospf.areaId', h=2, title='OSPF areas'),
    pie('bgp.type', h=2, title='BGP message types'), pie('isis.msgType', h=2, title='IS-IS message types'),
    table('ip.src', title='Routers', metrics=['sessions', 'packets'], sortMetric='sessions'),
    table('ip.dst', title='Peers and groups', metrics=['sessions', 'packets'], sortMetric='sessions'),
    connections(title='Routing adjacencies', w=4, h=3),
    sessions(['ip.src', 'ip.dst', 'protocols', 'ospf.msgType', 'ospf.routerId', 'ospf.areaId', 'bgp.type']),
], folder=A, expression=ROUTING)

MULTICAST = 'ip.dst == [224.0.0.0/4, ff00::/8]'
D['multicast'] = plain('Multicast', 'Sessions sent to IPv4 and IPv6 multicast groups: who sends to which group, over what', [
    nav(),
    timeline('protocols', 'Multicast sessions by protocol', length=10),
    metric('Multicast sessions'), metric('Multicast senders', 'ip.src', agg='cardinality'),
    bar('ip.dst', length=20, h=3, horizontal=True, title='Groups'),
    table('ip.src', title='Senders', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    pie('protocols', length=10, title='Protocols'), table('port.dst', h=2, title='Destination ports'),
    table('dns.query.host', h=2, title='mDNS and LLMNR names', expression='protocols == [mdns, llmnr]'),
    table('ssdp.st', h=2, title='SSDP search targets', expression='protocols == ssdp'),
    matrix(['ip.src', 'ip.dst'], w=4, title='Senders by group'),
    sankey(['ip.src', 'ip.dst'], title='Senders to groups'),
    sessions(['ip.src', 'ip.dst', 'port.dst', 'protocols', 'bytes', 'packets']),
], folder=A, expression=MULTICAST)

DATABASES = 'protocols == [mysql, postgresql, oracle, tds, mongo, redis, cassandra, aerospike, kafka]'
D['databases'] = plain('Databases', 'Database sessions: MySQL, PostgreSQL, Oracle, MSSQL (TDS), MongoDB, Redis, Cassandra, Aerospike and Kafka', [
    nav(),
    timeline('protocols', 'Database sessions by protocol', length=10),
    metric('Database sessions', sparkline=True), metric('Database bytes', 'bytes', sparkline=True),
    timeofday('Database sessions by hour of the week'),
    pie('protocols', length=9, title='Database protocols'),
    combo('ip.dst:port', title='Database servers: sessions and bytes'),
    table('ip.src', title='Clients', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    table('mysql.user', h=2, title='MySQL users'), table('postgresql.user', h=2, title='PostgreSQL users'),
    table('oracle.user', h=2, title='Oracle users'), table('postgresql.db', h=2, title='PostgreSQL databases'),
    tree('Database tree: clients to servers (click a host to re-root)'),
    sessions(['ip.src', 'ip.dst', 'port.dst', 'protocols', 'mysql.user', 'postgresql.user', 'oracle.user']),
], folder=A, expression=DATABASES)

D['hosts'] = plain('Hosts', 'The hosts on the wire: how many there are, their MAC vendors, VLANs and networks (ASNs), and the services they reach', [
    nav(),
    area('oui.src', 'Sessions by source MAC vendor', length=10, w=4),
    timeofday('Activity by hour of the week', split='oui.src', length=5, w=4),
    metric('Source hosts', 'ip.src', agg='cardinality'), metric('Destination hosts', 'ip.dst', agg='cardinality'),
    metric('MAC vendors', 'oui.src', agg='cardinality'), metric('VLANs', 'vlan', agg='cardinality'),
    table('oui.src', title='Source MAC vendors', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    table('oui.dst', title='Destination MAC vendors', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    table('mac.src', h=2, title='Source MACs'), table('mac.dst', h=2, title='Destination MACs'),
    table('vlan', h=2, title='VLANs'), table('node', h=2, title='Capture nodes'),
    table('asn.src', title='Source networks (ASN)', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    table('asn.dst', title='Destination networks (ASN)', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    table('ip.dst:port', w=2, title='Services', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    scatter('ip.src', size='packets', title='Hosts by sessions and bytes (point size: packets)'),
    matrix(['oui.src', 'port.dst'], w=4, title='MAC vendors by destination port'),
    sankey(['oui.src', 'port.dst'], title='MAC vendors to destination ports'),
    sessions(['ip.src', 'mac.src', 'oui.src', 'ip.dst', 'port.dst', 'vlan', 'asn.dst']),
], folder=A)

# ---- plugin dashboards -------------------------------------------------------------
PLUGINS['suricata'] = plain('Suricata', 'Sessions with a Suricata alert (suricata plugin): signatures, categories, severities and who was involved', [
    nav(),
    timeline('suricata.category', 'Alerted sessions by category', length=5),
    metric('Alerted sessions'), metric('Signatures', 'suricata.signature', agg='cardinality'),
    tagcloud('suricata.signature', title='Signatures'), pie('suricata.severity', title='Severity'),
    bar('suricata.category', length=15, h=3, horizontal=True, title='Categories'),
    table('suricata.signature', title='Top signatures', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    pie('suricata.action', h=2, title='Actions'), table('suricata.signatureId', h=2, title='Signature IDs'),
    table('ip.src', title='Alert sources', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    table('ip.dst:port', title='Alert targets', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    geomap('country.src', h=2, title='Source countries'), geomap('country.dst', h=2, title='Destination countries'),
    tree('Alert tree: sources to targets (click a host to re-root)'),
    sessions(['ip.src', 'ip.dst', 'port.dst', 'suricata.signature', 'suricata.category', 'suricata.severity', 'suricata.action']),
], folder=A, expression='suricata.signature == EXISTS!', requires=['suricata.signature'])

# the ja4plus plugin's dashboard is generated in its own repository with these helpers

PLUGINS['zeekintel'] = plain('Zeek Intel', 'Sessions that matched a Zeek Intel framework indicator (zeekintel plugin): indicators, types, sources and the hosts involved', [
    nav(),
    timeline('zeekintel.type', 'Intel hits by indicator type', length=5),
    metric('Sessions with intel hits'), metric('Indicators', 'zeekintel.indicator', agg='cardinality'),
    tagcloud('zeekintel.indicator', title='Indicators'), pie('zeekintel.type', title='Indicator types'),
    bar('zeekintel.source', length=15, h=3, horizontal=True, title='Intel sources'),
    table('zeekintel.indicator', title='Top indicators', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    pie('zeekintel.where', h=2, title='Where seen'), table('zeekintel.desc', h=2, title='Descriptions'),
    table('ip.src', title='Hosts with intel hits', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    table('ip.dst:port', title='Intel destinations', metrics=['sessions', 'bytes'], sortMetric='sessions'),
    table('zeekintel.url', w=4, h=2, title='Intel URLs'),
    tree('Intel tree: hosts to what they reached (click a host to re-root)'),
    sessions(['ip.src', 'ip.dst', 'port.dst', 'zeekintel.indicator', 'zeekintel.type', 'zeekintel.source', 'zeekintel.desc']),
], folder=A, expression='zeekintel.indicator == EXISTS!', requires=['zeekintel.indicator'])

# ---- write -------------------------------------------------------------------------
def write(out, dashes):
    """Writes each dashboard to <out>/<key>.dashboard.json, numbering its widgets."""
    os.makedirs(out, exist_ok=True)
    for key, dash in dashes.items():
        for i, w in enumerate(dash['data']['widgets']):
            dash['data']['widgets'][i] = {'id': f'w{i}', **w}
        with open(os.path.join(out, f'{key}.dashboard.json'), 'w') as f:
            f.write(json.dumps(dash, indent=2, ensure_ascii=False) + '\n')
    print(f'wrote {len(dashes)} dashboards to {out}')

# importable (a plugin's own generator reuses the helpers), the files are only written when run
if __name__ == '__main__':
    write(OUT, D)
    write(PLUGIN_OUT, PLUGINS)
