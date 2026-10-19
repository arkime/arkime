# zeeklog plugin

Writes Zeek style log files (`conn.log`, `dns.log`, `http.log`, `ssl.log`, `x509.log`, ...)
from Arkime sessions. The files use the same TSV layout, headers, escaping and column
names as Zeek's ASCII writer, or Zeek's JSON layout, so anything that already consumes
Zeek logs (zeek-cut, SIEM parsers, Security Onion, Malcolm, ...) can consume them.

Every time Arkime saves a session the plugin emits a row in each enabled log whose
trigger protocol the session carries. Long sessions that Arkime saves in several pieces
produce several rows with the same `uid`, the same way Zeek logs long connections.

## Enabling

```ini
[default]
plugins=zeeklog.so                    # list it last so fields set by other plugins' save hooks are logged
zeekLogDir=/opt/arkime/logs/zeek      # directory for the .log files, default .
zeekLogFormat=ascii                   # ascii (Zeek TSV) or json (one object per line)
zeekLogs=*                            # which logs to write, * = all builtin logs
zeekLogRotateSeconds=3600             # 0 (default) never rotates
zeekLogUtf8=true                      # false escapes every byte >= 0x80 as \xHH
```

`zeekLogs` is a `;` separated list. `*` expands to every builtin log, so
`zeekLogs=conn;dns;http` writes only those three and `zeekLogs=*;mylog` adds a custom log.

Rotation renames `conn.log` to `conn.<open-timestamp>.log`, which is what Zeek's ASCII writer
does with leftover logs. A non-empty `conn.log` found at startup is renamed the same way.

## Customizing columns

Each log has a `[zeeklog-<path>]` config section that works like a Zeek `Log::Filter`
plus a `redef record ... += {}`:

```ini
[zeeklog-conn]
# add columns, the value is where the data comes from
vlan=vlan
community_id=communityId
node=@node
orig_country=country.src
# or replace a builtin column's source
service=protocols
# drop columns
exclude=local_orig,local_resp,tunnel_parents
# or keep only these (in this order)
#include=ts,uid,id.orig_h,id.orig_p,id.resp_h,id.resp_p,proto,service,duration
# turn the whole log off
#enabled=false
# change which sessions produce rows
#protocols=tcp,udp
```

Column values are `source[+source...][:type]`:

* `@name` a computed special (list below)
* `@none` always unset, use with an explicit `:type` to keep a Zeek column in the header
* `ip.src`, `host.http`, `dns.query.type`... any Arkime field expression, multi valued
  fields become Zeek sets/vectors or, for a scalar Zeek column, a `,` joined value
* `expr?VALUE` a bool that is `T` when the field contains VALUE, e.g. `tls=protocols?tls`
* `a+b` concatenates several sources into one column
* `:type` forces the Zeek type in the `#types` header (`string`, `count`, `int`, `double`,
  `time`, `interval`, `bool`, `addr`, `port`, `enum`, `vector[x]`, `set[x]`), otherwise it is
  inferred from the Arkime field or special

A section whose name is not a builtin log defines a new log:

```ini
[default]
zeekLogs=*;ja4

[zeeklog-ja4]
protocols=tls
ts=@ts
uid=@uid
id.orig_h=@orig_h
id.orig_p=@orig_p
id.resp_h=@resp_h
id.resp_p=@resp_p
server_name=host.http
ja3=tls.ja3
ja4=tls.ja4
```

`iterate=` picks what a row is: `session` (default), `dns` (one row per DNS query, enables the
`@dns.*` specials), `cert` (one row per certificate, `@x509.*`) or `file` (one row per
HTTP/SMTP body hash, `@file.*`).

Run capture with `--debug` to print the final schema of every log at startup.

## Specials

| special | type | value |
|---|---|---|
| `@ts` | time | first packet of the session |
| `@uid` | string | Zeek style `C...` id derived from the Arkime session id, stable across mid-session saves |
| `@orig_h` `@orig_p` `@resp_h` `@resp_p` | addr/port | session 4-tuple |
| `@proto` | enum | tcp, udp, icmp or unknown_transport |
| `@service` | string | Arkime protocols minus the transports, renamed to Zeek names (tls→ssl, krb5→krb, dcerpc→dce_rpc) |
| `@duration` | interval | last packet - first packet |
| `@orig_bytes` `@resp_bytes` | count | payload bytes per direction |
| `@orig_pkts` `@resp_pkts` | count | packets per direction |
| `@orig_ip_bytes` `@resp_ip_bytes` | count | wire bytes minus the Ethernet (and 802.1Q) header |
| `@conn_state` | string | S0/S1/SF/REJ/S2/RSTO/RSTR/RSTOS0/RSTRH/SH/SHR/OTH derived from Arkime's TCP flag counters |
| `@history` | string | ShADadFfR style string derived from flag counters, directions are approximate |
| `@missed_bytes` | count | always 0 |
| `@ip_proto` | count | IP protocol number |
| `@trans_depth` | count | always 1 |
| `@node` | string | Arkime node name |
| `@session_id` | string | Arkime session id |
| `@true` `@false` `@zero` | bool/count | constants |
| `@empty` | set | present but empty, prints `(empty)` instead of `-` |
| `@tls.resumed` | bool | client and server TLS session ids match |
| `@tls.established` | bool | a server cipher was seen |
| `@tls.version` | string | Arkime tls.version spelled the Zeek way (TLSv1.2 → TLSv12) |
| `@tls.sni_matches_cert` | bool | SNI matches the leaf certificate CN or alt names |
| `@ssl.cert_chain_fps` | vector[string] | SHA256 certificate fingerprints |
| `@ssl.subject` `@ssl.issuer` | string | `CN=,O=,OU=` of the first non-CA certificate |
| `@ssh.version` | count | 1 or 2 from the banners |
| `@ssh.host_key_fingerprint` | string | MD5 of the host key |
| `@krb.client` | string | `cname/REALM` |
| `@krb.request_type` | string | AS or TGS from the Kerberos message types |
| `@http.status_code` `@http.info_code` | count | final (2xx+) and informational (1xx) HTTP status codes |
| `@ntp.mode` | count | NTP mode name turned back into its number |
| `@ntp.poll` `@ntp.precision` `@ntp.root_delay` `@ntp.root_disp` | interval | Arkime's seconds, nanoseconds and microseconds as Zeek seconds |
| `@socks.status` | string | Arkime's short status token spelled out the way Zeek does |
| `@ldap.result` | string | Arkime's camelCase result code as Zeek's spaced words |
| `@tunnel.type` `@tunnel.action` | enum | Tunnel::GRE/VXLAN/GTPv1/GENEVE and Tunnel::DISCOVER |
| `@dns.*` | | `trans_id rtt query qclass qclass_name qtype qtype_name rcode rcode_name AA TC RD RA Z answers TTLs rejected opcode opcode_name`, need `iterate=dns` |
| `@x509.*` | | `fingerprint serial subject issuer not_valid_before not_valid_after key_alg key_type curve san_dns san_email san_uri san_ip ca host_cert version sig_alg key_length exponent path_len`, need `iterate=cert` |
| `@file.*` | | `fuid source mime_type md5 sha256 filename is_orig`, need `iterate=file` |

## Differences from Zeek

* Zeek logs one row per protocol transaction (each HTTP request, each FTP command, each
  SMB file); Arkime stores per-session aggregates, so most logs have one row per session
  with multi valued fields joined by `,`. `dns.log` and `x509.log` are per query/certificate
  because Arkime keeps those as objects.
* `conn_state` and `history` are reconstructed from Arkime's per-session flag counters, not a
  packet-by-packet state machine, so direction letters and edge cases can differ.
* `dns.log` rows use the query packet time and the query/response flags like Zeek; other logs
  use the session's first packet time.
* No per-transaction timing: `rtt`, `request_body_len`, `status_msg`, Zeek file ids and
  similar columns are left unset. The "always unset" lists below say exactly which.
* `uid`/`fuid` values are stable hashes, not Zeek's random ids, so they never match Zeek's.

## Missing fields

Measured with `compare.py` over every pcap in `tests/pcap` against Zeek 9.0 (the generated report is not checked in, rerun the command below to refresh it). These are the Zeek
columns that Zeek fills and the plugin never can, because Arkime does not keep the data. The
"Always unset" lists under each schema below are the complete per-log view; this table groups
them by the parser work that would close the gap.

| log | missing columns | what Arkime would need |
|---|---|---|
| conn | `local_orig` `local_resp` | a local networks setting (Zeek `Site::local_nets`) |
| conn | `tunnel_parents` `missed_bytes` | an id for the outer tunnel session, a count of bytes lost in TCP gaps |
| conn | `history` letters `T` `c` `g` `w` `i` `q` `^` | retransmit, bad checksum, gap, zero window and flip tracking per packet |
| http | `status_msg` `origin` `proxied` | the bundled http_parser does not deliver the reason phrase; Origin and proxy headers are not captured |
| http | `orig_fuids` `resp_fuids` `orig_filenames` `resp_filenames` `orig_mime_types` | per direction body hashes and magic (today `http.md5`/`http.bodymagic` mix both directions) |
| ssl | `ssl_history` `client_cert_chain_fps` `established` (approximate) | handshake state tracking and client certificates |
| ssh | `auth_success` `auth_attempts` `direction` | Zeek's packet size heuristics after the key exchange |
| smtp | `first_received` `second_received` `path` | the Received hops; `from`/`to`/`subject` are stored decoded, Zeek keeps the raw header text |
| ftp | `password` `data_channel.*` | the password is intentionally not stored, PORT/PASV channels are not tracked |
| kerberos | `till` `success` `client_cert_*` `server_cert_*` | the request till time and PKINIT certificates |
| ntlm | `success` | the final SMB/HTTP status after the authenticate message |
| dhcp | `client_fqdn` `client_message` `server_message` | options 81 (partially), 56 |
| dns | `answers` for NSEC/CAA/HTTPS records | Zeek's text form of those rdata |
| smb | `smb_cmd.log`, `smb_files` `action` `size` `times.*`, `smb_mapping` `service` `share_type` `native_file_system` | per command tracking in the smb parser |
| dce_rpc | `operation` `rtt` `named_pipe` | an opnum to operation name table and request/response timing |
| snmp | `get_requests` `get_bulk_requests` `get_responses` `set_requests` `up_since` `display_string` | PDU counters |
| ldap | `message_id` `version` `diagnostic_message` `argument` and all of `ldap_search.log` | per message parsing beyond the operation and bind result |
| rdp sip quic radius mqtt postgresql modbus irc mysql | see the schema tables | per message fields these parsers do not extract |
| weird analyzer notice software known_* syslog ocsp pe | whole logs | no Arkime equivalent |

The remaining differences are structural: Zeek logs one row per request, command or file while
Arkime keeps per-session aggregates, so `http.log`, `ftp.log`, `sip.log`, `irc.log`, `mysql.log` and
`modbus.log` carry `,` joined values in one row.

## Comparing with Zeek

`compare.py` runs both tools over pcaps and reports, per log and column, how often the
values agree and which Zeek columns we never fill:

```bash
cd tests
../capture/plugins/zeeklog/compare.py --out /tmp/zeekcmp --report /tmp/zeekcmp/report.md pcap/*.pcap
```

Use `--reuse` to re-report without re-running, `--logs conn,dns` to limit the report and
`--zeek-args policy/protocols/ssl/ssl-log-ext` to load extra Zeek scripts.

## Default schemas

Generated from the builtin table in `zeeklog.c`.

### conn.log

Trigger: `every session`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| proto | `@proto` |
| service | `@service` |
| duration | `@duration` |
| orig_bytes | `@orig_bytes` |
| resp_bytes | `@resp_bytes` |
| conn_state | `@conn_state` |
| local_orig | `@none:bool` |
| local_resp | `@none:bool` |
| missed_bytes | `@missed_bytes` |
| history | `@history` |
| orig_pkts | `@orig_pkts` |
| orig_ip_bytes | `@orig_ip_bytes` |
| resp_pkts | `@resp_pkts` |
| resp_ip_bytes | `@resp_ip_bytes` |
| tunnel_parents | `@none:set[string]` |
| ip_proto | `@ip_proto` |

Always unset: `local_orig`, `local_resp`, `tunnel_parents`

### dns.log

Trigger: `dns`, one row per DNS query.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| proto | `@proto` |
| trans_id | `@dns.trans_id` |
| rtt | `@dns.rtt` |
| query | `@dns.query` |
| qclass | `@dns.qclass` |
| qclass_name | `@dns.qclass_name` |
| qtype | `@dns.qtype` |
| qtype_name | `@dns.qtype_name` |
| rcode | `@dns.rcode` |
| rcode_name | `@dns.rcode_name` |
| AA | `@dns.AA` |
| TC | `@dns.TC` |
| RD | `@dns.RD` |
| RA | `@dns.RA` |
| Z | `@dns.Z` |
| answers | `@dns.answers` |
| TTLs | `@dns.TTLs` |
| rejected | `@dns.rejected` |
| opcode | `@dns.opcode` |
| opcode_name | `@dns.opcode_name` |

Always unset: none

### http.log

Trigger: `http`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| trans_depth | `@trans_depth` |
| method | `http.method:string` |
| host | `host.http:string` |
| uri | `http.uri.path:string` |
| referrer | `http.request.referer:string` |
| version | `http.version.src:string` |
| user_agent | `http.user-agent:string` |
| origin | `@none:string` |
| request_body_len | `http.bodylen.src:count` |
| response_body_len | `http.bodylen.dst:count` |
| status_code | `@http.status_code` |
| status_msg | `@none:string` |
| info_code | `@http.info_code` |
| info_msg | `@none:string` |
| tags | `@empty:set[enum]` |
| username | `http.user:string` |
| password | `@none:string` |
| proxied | `@none:set[string]` |
| orig_fuids | `@none:vector[string]` |
| orig_filenames | `@none:vector[string]` |
| orig_mime_types | `@none:vector[string]` |
| resp_fuids | `@none:vector[string]` |
| resp_filenames | `@none:vector[string]` |
| resp_mime_types | `http.bodymagic:vector[string]` |

Always unset: `origin`, `status_msg`, `info_msg`, `password`, `proxied`, `orig_fuids`, `orig_filenames`, `orig_mime_types`, `resp_fuids`, `resp_filenames`

### ssl.log

Trigger: `tls,dtls`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| version | `@tls.version` |
| cipher | `tls.cipher:string` |
| curve | `tls.group:string` |
| server_name | `host.http:string` |
| resumed | `@tls.resumed` |
| last_alert | `tls.alert:string` |
| next_protocol | `tls.alpn:string` |
| established | `@tls.established` |
| ssl_history | `@none:string` |
| cert_chain_fps | `@ssl.cert_chain_fps` |
| client_cert_chain_fps | `@empty:vector[string]` |
| sni_matches_cert | `@tls.sni_matches_cert` |

Always unset: `ssl_history`

### x509.log

Trigger: `tls,dtls`, one row per certificate.

| column | source |
|---|---|
| ts | `@ts` |
| fingerprint | `@x509.fingerprint` |
| certificate.version | `@x509.version` |
| certificate.serial | `@x509.serial` |
| certificate.subject | `@x509.subject` |
| certificate.issuer | `@x509.issuer` |
| certificate.not_valid_before | `@x509.not_valid_before` |
| certificate.not_valid_after | `@x509.not_valid_after` |
| certificate.key_alg | `@x509.key_alg` |
| certificate.sig_alg | `@x509.sig_alg` |
| certificate.key_type | `@x509.key_type` |
| certificate.key_length | `@x509.key_length` |
| certificate.exponent | `@x509.exponent` |
| certificate.curve | `@x509.curve` |
| san.dns | `@x509.san_dns` |
| san.uri | `@x509.san_uri` |
| san.email | `@x509.san_email` |
| san.ip | `@x509.san_ip` |
| basic_constraints.ca | `@x509.ca` |
| basic_constraints.path_len | `@x509.path_len` |
| host_cert | `@x509.host_cert` |
| client_cert | `@false` |

Always unset: none

### files.log

Trigger: `http,smtp`, one row per file hash.

| column | source |
|---|---|
| ts | `@ts` |
| fuid | `@file.fuid` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| source | `@file.source` |
| depth | `@zero` |
| analyzers | `@none:set[string]` |
| mime_type | `@file.mime_type` |
| filename | `@file.filename` |
| duration | `@none:interval` |
| local_orig | `@none:bool` |
| is_orig | `@file.is_orig` |
| seen_bytes | `@none:count` |
| total_bytes | `@none:count` |
| missing_bytes | `@none:count` |
| overflow_bytes | `@none:count` |
| timedout | `@false` |
| parent_fuid | `@none:string` |
| md5 | `@file.md5` |
| sha1 | `@none:string` |
| sha256 | `@file.sha256` |
| extracted | `@none:string` |
| extracted_cutoff | `@none:bool` |
| extracted_size | `@none:count` |

Always unset: `analyzers`, `duration`, `local_orig`, `seen_bytes`, `total_bytes`, `missing_bytes`, `overflow_bytes`, `parent_fuid`, `sha1`, `extracted`, `extracted_cutoff`, `extracted_size`

### ssh.log

Trigger: `ssh`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| version | `@ssh.version` |
| auth_success | `@none:bool` |
| auth_attempts | `@none:count` |
| direction | `@none:enum` |
| client | `ssh.ver.src:string` |
| server | `ssh.ver.dst:string` |
| cipher_alg | `ssh.cipher:string` |
| mac_alg | `ssh.mac:string` |
| compression_alg | `ssh.compression:string` |
| kex_alg | `ssh.kex:string` |
| host_key_alg | `ssh.hostKeyAlg:string` |
| host_key_fingerprint | `@ssh.host_key_fingerprint` |

Always unset: `auth_success`, `auth_attempts`, `direction`

### smtp.log

Trigger: `smtp`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| trans_depth | `@trans_depth` |
| helo | `email.smtp-hello:string` |
| mailfrom | `email.src:string` |
| rcptto | `email.dst` |
| date | `email.date:string` |
| from | `email.src:string` |
| to | `email.dst` |
| cc | `@none:set[string]` |
| reply_to | `email.reply-to:string` |
| msg_id | `email.message-id:string` |
| in_reply_to | `email.in-reply-to:string` |
| subject | `email.subject:string` |
| x_originating_ip | `ip.email:addr` |
| first_received | `@none:string` |
| second_received | `@none:string` |
| last_reply | `email.reply:string` |
| path | `@none:vector[addr]` |
| user_agent | `email.x-mailer:string` |
| tls | `protocols?tls` |
| fuids | `@empty:vector[string]` |

Always unset: `cc`, `first_received`, `second_received`, `path`

### ftp.log

Trigger: `ftp`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| user | `user:string` |
| password | `@none:string` |
| command | `ftp.command:string` |
| arg | `ftp.filename:string` |
| mime_type | `@none:string` |
| file_size | `@none:count` |
| reply_code | `ftp.responseCode:count` |
| reply_msg | `ftp.responseMsg:string` |
| data_channel.passive | `@none:bool` |
| data_channel.orig_h | `@none:addr` |
| data_channel.resp_h | `@none:addr` |
| data_channel.resp_p | `@none:port` |
| fuid | `@none:string` |

Always unset: `password`, `mime_type`, `file_size`, `data_channel.passive`, `data_channel.orig_h`, `data_channel.resp_h`, `data_channel.resp_p`, `fuid`

### dhcp.log

Trigger: `dhcp`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uids | `@uid:set[string]` |
| client_addr | `dhcp.yiaddr:addr` |
| server_addr | `dhcp.serverIp:addr` |
| mac | `dhcp.mac:string` |
| host_name | `dhcp.host:string` |
| client_fqdn | `@none:string` |
| domain | `dhcp.domain:string` |
| requested_addr | `dhcp.requestIp:addr` |
| assigned_addr | `dhcp.yiaddr:addr` |
| lease_time | `dhcp.leaseTime:interval` |
| client_message | `@none:string` |
| server_message | `@none:string` |
| msg_types | `dhcp.type:vector[string]` |
| duration | `@duration` |

Always unset: `client_fqdn`, `client_message`, `server_message`

### kerberos.log

Trigger: `krb5`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| request_type | `@krb.request_type` |
| client | `@krb.client` |
| service | `krb5.sname:string` |
| success | `@none:bool` |
| error_msg | `krb5.error:string` |
| from | `@none:time` |
| till | `@none:time` |
| cipher | `krb5.ticketEtype:string` |
| forwardable | `krb5.options?forwardable` |
| renewable | `krb5.options?renewable` |
| client_cert_subject | `@none:string` |
| client_cert_fuid | `@none:string` |
| server_cert_subject | `@none:string` |
| server_cert_fuid | `@none:string` |

Always unset: `success`, `from`, `till`, `client_cert_subject`, `client_cert_fuid`, `server_cert_subject`, `server_cert_fuid`

### ntlm.log

Trigger: `ntlm`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| username | `user:string` |
| hostname | `ntlm.host:string` |
| domainname | `ntlm.domain:string` |
| server_nb_computer_name | `ntlm.nbComputerName:string` |
| server_dns_computer_name | `ntlm.dnsComputerName:string` |
| server_tree_name | `ntlm.dnsTreeName:string` |
| success | `@none:bool` |

Always unset: `success`

### smb_mapping.log

Trigger: `smb`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| path | `smb.share:string` |
| service | `@none:string` |
| native_file_system | `@none:string` |
| share_type | `@none:string` |

Always unset: `service`, `native_file_system`, `share_type`

### smb_files.log

Trigger: `smb`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| fuid | `@none:string` |
| action | `@none:enum` |
| path | `smb.share:string` |
| name | `smb.fn:string` |
| size | `@none:count` |
| prev_name | `@none:string` |
| times.modified | `@none:time` |
| times.accessed | `@none:time` |
| times.created | `@none:time` |
| times.changed | `@none:time` |

Always unset: `fuid`, `action`, `size`, `prev_name`, `times.modified`, `times.accessed`, `times.created`, `times.changed`

### dce_rpc.log

Trigger: `dcerpc`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| rtt | `@none:interval` |
| named_pipe | `@none:string` |
| endpoint | `dcerpc.interface:string` |
| operation | `@none:string` |

Always unset: `rtt`, `named_pipe`, `operation`

### rdp.log

Trigger: `rdp`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| cookie | `@none:string` |
| result | `@none:string` |
| security_protocol | `rdp.selected-protocol:string` |
| client_channels | `@none:vector[string]` |
| keyboard_layout | `rdp.keyboard-layout:string` |
| client_build | `rdp.client-build:string` |
| client_name | `rdp.client-name:string` |
| client_dig_product_id | `@none:string` |
| desktop_width | `@none:count` |
| desktop_height | `@none:count` |
| requested_color_depth | `@none:string` |
| cert_type | `@none:string` |
| cert_count | `@none:count` |
| cert_permanent | `@none:bool` |
| encryption_level | `@none:string` |
| encryption_method | `@none:string` |

Always unset: `cookie`, `result`, `client_channels`, `client_dig_product_id`, `desktop_width`, `desktop_height`, `requested_color_depth`, `cert_type`, `cert_count`, `cert_permanent`, `encryption_level`, `encryption_method`

### sip.log

Trigger: `sip`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| trans_depth | `@trans_depth` |
| method | `sip.method:string` |
| uri | `@none:string` |
| date | `@none:string` |
| request_from | `sip.from:string` |
| request_to | `sip.to:string` |
| response_from | `@none:string` |
| response_to | `@none:string` |
| reply_to | `@none:string` |
| call_id | `sip.callid:string` |
| seq | `@none:string` |
| subject | `@none:string` |
| request_path | `sip.via:vector[string]` |
| response_path | `@none:vector[string]` |
| user_agent | `sip.user-agent:string` |
| status_code | `sip.statuscode:count` |
| status_msg | `@none:string` |
| warning | `@none:string` |
| request_body_len | `@none:count` |
| response_body_len | `@none:count` |
| content_type | `@none:string` |

Always unset: `uri`, `date`, `response_from`, `response_to`, `reply_to`, `seq`, `subject`, `response_path`, `status_msg`, `warning`, `request_body_len`, `response_body_len`, `content_type`

### snmp.log

Trigger: `snmp`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| duration | `@duration` |
| version | `snmp.version:string` |
| community | `snmp.community:string` |
| get_requests | `@none:count` |
| get_bulk_requests | `@none:count` |
| get_responses | `@none:count` |
| set_requests | `@none:count` |
| display_string | `@none:string` |
| up_since | `@none:time` |

Always unset: `get_requests`, `get_bulk_requests`, `get_responses`, `set_requests`, `display_string`, `up_since`

### socks.log

Trigger: `socks`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| version | `socks.version:count` |
| user | `socks.user:string` |
| password | `@none:string` |
| status | `@socks.status` |
| request.host | `ip.socks:addr` |
| request.name | `host.socks:string` |
| request_p | `port.socks:port` |
| bound.host | `socks.bound.ip:addr` |
| bound.name | `@none:string` |
| bound_p | `socks.bound.port:port` |

Always unset: `password`, `bound.name`

### postgresql.log

Trigger: `postgresql`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| user | `postgresql.user:string` |
| database | `postgresql.db:string` |
| application_name | `postgresql.app:string` |
| frontend | `@none:string` |
| frontend_arg | `@none:string` |
| backend | `@none:string` |
| backend_arg | `@none:string` |
| success | `@none:bool` |
| rows | `@none:count` |

Always unset: `frontend`, `frontend_arg`, `backend`, `backend_arg`, `success`, `rows`

### irc.log

Trigger: `irc`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| nick | `irc.nick:string` |
| user | `@none:string` |
| command | `@none:string` |
| value | `irc.channel:string` |
| addl | `@none:string` |

Always unset: `user`, `command`, `addl`

### modbus.log

Trigger: `modbus`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| tid | `modbus.transactionid:count` |
| unit | `modbus.unitid:count` |
| func | `modbus.funccode:string` |
| pdu_type | `@none:string` |
| exception | `modbus.exccode:string` |

Always unset: `pdu_type`

### dnp3.log

Trigger: `dnp3`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| fc_request | `dnp3.funcName:string` |
| fc_reply | `@none:string` |
| iin | `@none:count` |

Always unset: `fc_reply`, `iin`

### ntp.log

Trigger: `ntp`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| version | `ntp.version:count` |
| mode | `@ntp.mode` |
| stratum | `ntp.stratumNum:count` |
| poll | `@ntp.poll` |
| precision | `@ntp.precision` |
| root_delay | `@ntp.root_delay` |
| root_disp | `@ntp.root_disp` |
| ref_id | `ntp.ref-id:string` |
| ref_time | `ntp.refTime:time` |
| org_time | `ntp.orgTime:time` |
| rec_time | `ntp.recTime:time` |
| xmt_time | `ntp.xmtTime:time` |
| num_exts | `ntp.extensions:count` |

Always unset: none

### radius.log

Trigger: `radius`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| username | `radius.user:string` |
| mac | `radius.mac:string` |
| framed_addr | `radius.framed-ip:addr` |
| tunnel_client | `@none:string` |
| connect_info | `@none:string` |
| reply_msg | `@none:string` |
| result | `@none:string` |
| ttl | `@none:interval` |

Always unset: `tunnel_client`, `connect_info`, `reply_msg`, `result`, `ttl`

### quic.log

Trigger: `quic`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| version | `quic.version:string` |
| client_initial_dcid | `@none:string` |
| client_scid | `@none:string` |
| server_scid | `@none:string` |
| server_name | `host.quic:string` |
| client_protocol | `tls.alpn.src:string` |
| history | `@none:string` |

Always unset: `client_initial_dcid`, `client_scid`, `server_scid`, `history`

### ldap.log

Trigger: `ldap`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| message_id | `@none:int` |
| version | `@none:int` |
| opcode | `ldap.op:string` |
| result | `@ldap.result` |
| diagnostic_message | `@none:string` |
| object | `ldap.bindname:string` |
| argument | `@none:string` |

Always unset: `message_id`, `version`, `diagnostic_message`, `argument`

### mqtt_connect.log

Trigger: `mqtt`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| proto_name | `@none:string` |
| proto_version | `mqtt.version:string` |
| client_id | `mqtt.clientId:string` |
| connect_status | `mqtt.connackCode:string` |
| will_topic | `mqtt.willTopic:string` |
| will_payload | `@none:string` |

Always unset: `proto_name`, `will_payload`

### mqtt_publish.log

Trigger: `mqtt`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| from_client | `@none:bool` |
| retain | `@none:bool` |
| qos | `mqtt.qos:string` |
| status | `@none:string` |
| topic | `mqtt.topic:string` |
| payload | `@none:string` |
| payload_len | `mqtt.payloadLen:count` |

Always unset: `from_client`, `retain`, `status`, `payload`

### websocket.log

Trigger: `websocket`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| host | `host.http:string` |
| uri | `http.uri.path:string` |
| user_agent | `http.user-agent:string` |
| subprotocol | `@none:string` |
| client_protocols | `@none:vector[string]` |
| server_extensions | `@none:vector[string]` |
| client_extensions | `@none:vector[string]` |

Always unset: `subprotocol`, `client_protocols`, `server_extensions`, `client_extensions`

### mysql.log

Trigger: `mysql`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `@orig_h` |
| id.orig_p | `@orig_p` |
| id.resp_h | `@resp_h` |
| id.resp_p | `@resp_p` |
| cmd | `@none:string` |
| arg | `mysql.user:string` |
| success | `@none:bool` |
| rows | `@none:count` |
| response | `@none:string` |

Always unset: `cmd`, `success`, `rows`, `response`

### tunnel.log

Trigger: `gre,vxlan,gtp,geneve`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uid | `@uid` |
| id.orig_h | `outerip.src` |
| id.orig_p | `@zero:port` |
| id.resp_h | `outerip.dst` |
| id.resp_p | `@zero:port` |
| tunnel_type | `@tunnel.type` |
| action | `@tunnel.action` |

Always unset: none

