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
plugins=zeeklog.so
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
| `@ssl.cert_chain_fps` | vector[string] | certificate hashes (SHA1, Zeek uses SHA256) |
| `@ssl.subject` `@ssl.issuer` | string | `CN=,O=,OU=` of the first non-CA certificate |
| `@ssh.version` | count | 1 or 2 from the banners |
| `@ssh.host_key_fingerprint` | string | MD5 of the host key |
| `@krb.client` | string | `cname/REALM` |
| `@tunnel.type` `@tunnel.action` | enum | Tunnel::GRE/VXLAN/GTPv1/GENEVE and Tunnel::DISCOVER |
| `@dns.*` | | `trans_id rtt query qclass qclass_name qtype qtype_name rcode rcode_name AA TC RD RA Z answers TTLs rejected opcode opcode_name`, need `iterate=dns` |
| `@x509.*` | | `fingerprint serial subject issuer not_valid_before not_valid_after key_alg key_type curve san_dns ca host_cert`, need `iterate=cert` |
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

Measured with `compare.py` over every pcap in `tests/pcap` against Zeek 9.0. These are the Zeek
columns that Zeek fills and the plugin never can, because Arkime does not keep the data. The
"Always unset" lists under each schema below are the complete per-log view; this table groups
them by the parser work that would close the gap.

| log | missing columns | what Arkime would need |
|---|---|---|
| conn | `local_orig` `local_resp` | a local networks setting (Zeek `Site::local_nets`) |
| conn | `tunnel_parents` `missed_bytes` | an id for the outer tunnel session, a count of bytes lost in TCP gaps |
| conn | `history` letters `T` `c` `g` `w` `i` `q` `^` | retransmit, bad checksum, gap, zero window and flip tracking per packet |
| http | `status_msg` `request_body_len` `response_body_len` `info_code` `info_msg` `origin` `proxied` | the http parser keeps the status code but not the reason phrase, body sizes, 1xx responses or proxy headers |
| http | `orig_fuids` `resp_fuids` `orig_filenames` `resp_filenames` `orig_mime_types` | per direction body hashes and magic (today `http.md5`/`http.bodymagic` mix both directions) |
| ssl | `curve` `next_protocol` `last_alert` `ssl_history` `client_cert_chain_fps` | the tls parser already walks the extensions for JA3/JA4 but does not store the negotiated group, server ALPN, alerts or client certificates |
| ssl | `established` (approximate) | detection of the Finished / ChangeCipherSpec exchange |
| x509 | `certificate.version` `certificate.sig_alg` `certificate.key_length` `certificate.exponent` `basic_constraints.path_len` `san.uri` `san.email` `san.ip` | more of the certificate parsed in certs.c; `fingerprint` and `cert_chain_fps` are SHA1 where Zeek uses SHA256 |
| ssh | `server` `cipher_alg` `mac_alg` `compression_alg` `kex_alg` `host_key_alg` `auth_success` `auth_attempts` `direction` | the ssh parser merges both banners into `ssh.ver` and parses KEXINIT for HASSH without storing the chosen algorithms |
| smtp | `date` `reply_to` `first_received` `second_received` `path` `last_reply` | Received hops, the Date/Reply-To headers and the last server reply; `from`/`to`/`subject` are stored decoded, Zeek keeps the raw header text |
| ftp | `password` `reply_msg` `arg` `data_channel.*` | per command rows, the password is intentionally not stored |
| kerberos | `request_type` `till` `forwardable` `renewable` `success` | message type and ticket flags; `cipher` lists every offered etype where Zeek logs the selected one |
| ntlm | `server_nb_computer_name` `server_dns_computer_name` `server_tree_name` `success` | the AV pairs of the Type 2 challenge and the final status |
| dhcp | `lease_time` `server_addr` `domain` `client_fqdn` `client_message` `server_message` | options 51, 54, 15, 81, 56; `msg_types` is unordered because it is a set |
| dns | `answers` for NSEC/CAA/HTTPS records | Zeek's text form of those rdata |
| smb | `smb_cmd.log`, `smb_files` `action` `size` `times.*`, `smb_mapping` `service` `share_type` `native_file_system` | per command tracking in the smb parser |
| dce_rpc | `operation` `rtt` `named_pipe` | an opnum to operation name table and request/response timing |
| snmp | `get_requests` `get_bulk_requests` `get_responses` `set_requests` `up_since` `display_string` | PDU counters |
| socks | `version` `status` `bound.host` `bound_p` | the reply |
| ldap | `message_id` `version` `opcode` `result` `diagnostic_message` `argument` and all of `ldap_search.log` | per message parsing, today only bind name and auth type |
| rdp sip ntp quic radius mqtt postgresql modbus irc mysql | see the schema tables | per message fields these parsers do not extract |
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
| method | `http.method` |
| host | `host.http` |
| uri | `http.uri.path` |
| referrer | `http.request.referer` |
| version | `http.version.src` |
| user_agent | `http.user-agent` |
| origin | `@none:string` |
| request_body_len | `@none:count` |
| response_body_len | `@none:count` |
| status_code | `http.statuscode` |
| status_msg | `@none:string` |
| info_code | `@none:count` |
| info_msg | `@none:string` |
| tags | `@empty:set[enum]` |
| username | `http.user` |
| password | `@none:string` |
| proxied | `@none:set[string]` |
| orig_fuids | `@none:vector[string]` |
| orig_filenames | `@none:vector[string]` |
| orig_mime_types | `@none:vector[string]` |
| resp_fuids | `@none:vector[string]` |
| resp_filenames | `@none:vector[string]` |
| resp_mime_types | `http.bodymagic:vector[string]` |

Always unset: `origin`, `request_body_len`, `response_body_len`, `status_msg`, `info_code`, `info_msg`, `password`, `proxied`, `orig_fuids`, `orig_filenames`, `orig_mime_types`, `resp_fuids`, `resp_filenames`

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
| cipher | `tls.cipher` |
| curve | `@none:string` |
| server_name | `host.http` |
| resumed | `@tls.resumed` |
| last_alert | `@none:string` |
| next_protocol | `@none:string` |
| established | `@tls.established` |
| ssl_history | `@none:string` |
| cert_chain_fps | `@ssl.cert_chain_fps` |
| client_cert_chain_fps | `@empty:vector[string]` |
| sni_matches_cert | `@tls.sni_matches_cert` |

Always unset: `curve`, `last_alert`, `next_protocol`, `ssl_history`

### x509.log

Trigger: `tls,dtls`, one row per certificate.

| column | source |
|---|---|
| ts | `@ts` |
| fingerprint | `@x509.fingerprint` |
| certificate.version | `@none:count` |
| certificate.serial | `@x509.serial` |
| certificate.subject | `@x509.subject` |
| certificate.issuer | `@x509.issuer` |
| certificate.not_valid_before | `@x509.not_valid_before` |
| certificate.not_valid_after | `@x509.not_valid_after` |
| certificate.key_alg | `@x509.key_alg` |
| certificate.sig_alg | `@none:string` |
| certificate.key_type | `@x509.key_type` |
| certificate.key_length | `@none:count` |
| certificate.exponent | `@none:string` |
| certificate.curve | `@x509.curve` |
| san.dns | `@x509.san_dns` |
| san.uri | `@none:vector[string]` |
| san.email | `@none:vector[string]` |
| san.ip | `@none:vector[addr]` |
| basic_constraints.ca | `@x509.ca` |
| basic_constraints.path_len | `@none:count` |
| host_cert | `@x509.host_cert` |
| client_cert | `@false` |

Always unset: `certificate.version`, `certificate.sig_alg`, `certificate.key_length`, `certificate.exponent`, `san.uri`, `san.email`, `san.ip`, `basic_constraints.path_len`

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
| client | `ssh.ver` |
| server | `@none:string` |
| cipher_alg | `@none:string` |
| mac_alg | `@none:string` |
| compression_alg | `@none:string` |
| kex_alg | `@none:string` |
| host_key_alg | `@none:string` |
| host_key_fingerprint | `@ssh.host_key_fingerprint` |

Always unset: `auth_success`, `auth_attempts`, `direction`, `server`, `cipher_alg`, `mac_alg`, `compression_alg`, `kex_alg`, `host_key_alg`

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
| helo | `email.smtp-hello` |
| mailfrom | `email.src` |
| rcptto | `email.dst` |
| date | `@none:string` |
| from | `email.src` |
| to | `email.dst` |
| cc | `@none:set[string]` |
| reply_to | `@none:string` |
| msg_id | `email.message-id` |
| in_reply_to | `@none:string` |
| subject | `email.subject` |
| x_originating_ip | `ip.email` |
| first_received | `@none:string` |
| second_received | `@none:string` |
| last_reply | `@none:string` |
| path | `@none:vector[addr]` |
| user_agent | `email.x-mailer` |
| tls | `protocols?tls` |
| fuids | `@empty:vector[string]` |

Always unset: `date`, `cc`, `reply_to`, `in_reply_to`, `first_received`, `second_received`, `last_reply`, `path`

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
| user | `user` |
| password | `@none:string` |
| command | `ftp.command` |
| arg | `ftp.filename` |
| mime_type | `@none:string` |
| file_size | `@none:count` |
| reply_code | `ftp.responseCode` |
| reply_msg | `@none:string` |
| data_channel.passive | `@none:bool` |
| data_channel.orig_h | `@none:addr` |
| data_channel.resp_h | `@none:addr` |
| data_channel.resp_p | `@none:port` |
| fuid | `@none:string` |

Always unset: `password`, `mime_type`, `file_size`, `reply_msg`, `data_channel.passive`, `data_channel.orig_h`, `data_channel.resp_h`, `data_channel.resp_p`, `fuid`

### dhcp.log

Trigger: `dhcp`, one row per session.

| column | source |
|---|---|
| ts | `@ts` |
| uids | `@uid:set[string]` |
| client_addr | `dhcp.yiaddr` |
| server_addr | `@resp_h` |
| mac | `dhcp.mac` |
| host_name | `dhcp.host` |
| client_fqdn | `@none:string` |
| domain | `@none:string` |
| requested_addr | `dhcp.requestIp` |
| assigned_addr | `dhcp.yiaddr` |
| lease_time | `@none:interval` |
| client_message | `@none:string` |
| server_message | `@none:string` |
| msg_types | `dhcp.type:vector[string]` |
| duration | `@duration` |

Always unset: `client_fqdn`, `domain`, `lease_time`, `client_message`, `server_message`

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
| request_type | `@none:string` |
| client | `@krb.client` |
| service | `krb5.sname` |
| success | `@none:bool` |
| error_msg | `krb5.error` |
| from | `@none:time` |
| till | `@none:time` |
| cipher | `krb5.etype` |
| forwardable | `@none:bool` |
| renewable | `@none:bool` |
| client_cert_subject | `@none:string` |
| client_cert_fuid | `@none:string` |
| server_cert_subject | `@none:string` |
| server_cert_fuid | `@none:string` |

Always unset: `request_type`, `success`, `from`, `till`, `forwardable`, `renewable`, `client_cert_subject`, `client_cert_fuid`, `server_cert_subject`, `server_cert_fuid`

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
| username | `user` |
| hostname | `ntlm.host` |
| domainname | `ntlm.domain` |
| server_nb_computer_name | `@none:string` |
| server_dns_computer_name | `@none:string` |
| server_tree_name | `@none:string` |
| success | `@none:bool` |

Always unset: `server_nb_computer_name`, `server_dns_computer_name`, `server_tree_name`, `success`

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
| path | `smb.share` |
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
| path | `smb.share` |
| name | `smb.fn` |
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
| endpoint | `dcerpc.interface` |
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
| security_protocol | `rdp.selected-protocol` |
| client_channels | `@none:vector[string]` |
| keyboard_layout | `rdp.keyboard-layout` |
| client_build | `rdp.client-build:string` |
| client_name | `rdp.client-name` |
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
| method | `sip.method` |
| uri | `@none:string` |
| date | `@none:string` |
| request_from | `sip.from` |
| request_to | `sip.to` |
| response_from | `@none:string` |
| response_to | `@none:string` |
| reply_to | `@none:string` |
| call_id | `sip.callid` |
| seq | `@none:string` |
| subject | `@none:string` |
| request_path | `sip.via:vector[string]` |
| response_path | `@none:vector[string]` |
| user_agent | `sip.user-agent` |
| status_code | `sip.statuscode` |
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
| community | `snmp.community` |
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
| version | `@none:count` |
| user | `socks.user` |
| password | `@none:string` |
| status | `@none:string` |
| request.host | `ip.socks` |
| request.name | `host.socks` |
| request_p | `port.socks` |
| bound.host | `@none:addr` |
| bound.name | `@none:string` |
| bound_p | `@none:port` |

Always unset: `version`, `password`, `status`, `bound.host`, `bound.name`, `bound_p`

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
| user | `postgresql.user` |
| database | `postgresql.db` |
| application_name | `postgresql.app` |
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
| nick | `irc.nick` |
| user | `@none:string` |
| command | `@none:string` |
| value | `irc.channel` |
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
| tid | `modbus.transactionid` |
| unit | `modbus.unitid` |
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
| fc_request | `dnp3.funcName` |
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
| version | `ntp.version` |
| mode | `ntp.mode:count` |
| stratum | `ntp.stratum:count` |
| poll | `@none:interval` |
| precision | `@none:interval` |
| root_delay | `@none:interval` |
| root_disp | `@none:interval` |
| ref_id | `ntp.ref-id` |
| ref_time | `@none:time` |
| org_time | `@none:time` |
| rec_time | `@none:time` |
| xmt_time | `@none:time` |
| num_exts | `@none:count` |

Always unset: `poll`, `precision`, `root_delay`, `root_disp`, `ref_time`, `org_time`, `rec_time`, `xmt_time`, `num_exts`

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
| username | `radius.user` |
| mac | `radius.mac` |
| framed_addr | `radius.framed-ip` |
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
| version | `quic.version` |
| client_initial_dcid | `@none:string` |
| client_scid | `@none:string` |
| server_scid | `@none:string` |
| server_name | `host.quic` |
| client_protocol | `@none:string` |
| history | `@none:string` |

Always unset: `client_initial_dcid`, `client_scid`, `server_scid`, `client_protocol`, `history`

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
| opcode | `@none:string` |
| result | `@none:string` |
| diagnostic_message | `@none:string` |
| object | `ldap.bindname` |
| argument | `@none:string` |

Always unset: `message_id`, `version`, `opcode`, `result`, `diagnostic_message`, `argument`

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
| proto_version | `mqtt.version` |
| client_id | `mqtt.clientId` |
| connect_status | `mqtt.connackCode:string` |
| will_topic | `mqtt.willTopic` |
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
| topic | `mqtt.topic` |
| payload | `@none:string` |
| payload_len | `@none:count` |

Always unset: `from_client`, `retain`, `status`, `payload`, `payload_len`

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
| host | `host.http` |
| uri | `http.uri.path` |
| user_agent | `http.user-agent` |
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
| arg | `mysql.user` |
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

