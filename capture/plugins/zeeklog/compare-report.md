# zeeklog vs Zeek comparison

Pcaps: 199

## Logs

| log | zeek rows | arkime rows | matched rows | columns only in zeek | columns only in arkime |
|---|---|---|---|---|---|
| analyzer | 18 | 0 | 0 | analyzer_kind, analyzer_name, failure_data, failure_reason, fuid, id.orig_h, id.orig_p, id.resp_h, id.resp_p, proto, ts, uid | - |
| conn | 2391 | 995 | 742 | - | - |
| dce_rpc | 12 | 22 | 11 | - | - |
| dhcp | 10 | 8 | 8 | - | - |
| dnp3 | 3 | 4 | 2 | - | - |
| dns | 699 | 598 | 564 | - | - |
| files | 94 | 86 | 68 | - | - |
| ftp | 13 | 3 | 3 | - | - |
| http | 95 | 121 | 78 | - | - |
| irc | 12 | 6 | 5 | dcc_file_name, dcc_file_size, dcc_mime_type, fuid | - |
| kerberos | 4 | 4 | 3 | - | - |
| ldap | 13 | 10 | 8 | - | - |
| ldap_search | 2 | 0 | 0 | attributes, base_object, deref_aliases, diagnostic_message, filter, id.orig_h, id.orig_p, id.resp_h, id.resp_p, message_id, result, result_count, scope, ts, uid | - |
| modbus | 598 | 10 | 8 | - | - |
| mqtt_connect | 2 | 14 | 2 | - | - |
| mqtt_publish | 7 | 14 | 4 | - | - |
| mqtt_subscribe | 1 | 0 | 0 | ack, action, granted_qos_level, id.orig_h, id.orig_p, id.resp_h, id.resp_p, qos_levels, topics, ts, uid | - |
| mysql | 3 | 6 | 2 | - | - |
| ntlm | 17 | 25 | 17 | - | - |
| ntp | 11 | 7 | 7 | - | - |
| ocsp | 1 | 0 | 0 | certStatus, hashAlgorithm, id, issuerKeyHash, issuerNameHash, nextUpdate, revokereason, revoketime, serialNumber, thisUpdate, ts | - |
| packet_filter | 199 | 0 | 0 | failure_reason, filter, init, node, success, ts | - |
| postgresql | 9 | 5 | 5 | - | - |
| quic | 10 | 12 | 6 | - | - |
| radius | 4 | 7 | 3 | - | - |
| rdp | 2 | 3 | 2 | - | - |
| reporter | 1 | 0 | 0 | level, location, message, ts | - |
| sip | 3 | 10 | 3 | - | - |
| smb_files | 0 | 25 | 0 | - | action, fuid, id.orig_h, id.orig_p, id.resp_h, id.resp_p, name, path, prev_name, size, times.accessed, times.changed, times.created, times.modified, ts, uid |
| smb_mapping | 7 | 25 | 6 | - | - |
| smtp | 24 | 29 | 24 | - | - |
| snmp | 9 | 18 | 8 | - | - |
| socks | 8 | 16 | 8 | - | - |
| ssh | 9 | 13 | 9 | - | - |
| ssl | 37 | 51 | 30 | - | - |
| syslog | 5 | 0 | 0 | facility, id.orig_h, id.orig_p, id.resp_h, id.resp_p, message, proto, severity, ts, uid | - |
| tunnel | 57 | 44 | 4 | - | - |
| websocket | 1 | 3 | 1 | - | - |
| weird | 89 | 0 | 0 | addl, id.orig_h, id.orig_p, id.resp_h, id.resp_p, name, notice, peer, source, ts, uid | - |
| x509 | 44 | 54 | 40 | - | - |

## Columns Zeek fills that we never fill

| log | column | type | zeek rows set | example |
|---|---|---|---|---|
| analyzer | ts | time | 18 |  |
| analyzer | analyzer_kind | string | 18 |  |
| analyzer | analyzer_name | string | 18 |  |
| analyzer | uid | string | 18 |  |
| analyzer | id.orig_h | addr | 18 |  |
| analyzer | id.orig_p | port | 18 |  |
| analyzer | id.resp_h | addr | 18 |  |
| analyzer | id.resp_p | port | 18 |  |
| analyzer | proto | enum | 18 |  |
| analyzer | failure_reason | string | 18 |  |
| analyzer | failure_data | string | 1 |  |
| conn | local_orig | bool | 2391 | 6-4-gre-ppp-udp-4-dns: zeek='T' ours=- |
| conn | local_resp | bool | 2391 | 6-4-gre-ppp-udp-4-dns: zeek='F' ours=- |
| conn | tunnel_parents | set[string] | 43 | 6-4-gre-ppp-udp-4-dns: zeek='CnRBUr34MKaMjUszD2' ours=- |
| dce_rpc | rtt | interval | 12 | dcerpc_synthetic: zeek='1.000000' ours=- |
| dce_rpc | named_pipe | string | 1 |  |
| dce_rpc | operation | string | 12 | dcerpc_synthetic: zeek='EfsRpcQueryRecoveryAgents' ours=- |
| dnp3 | fc_reply | string | 3 | dnp3_synthetic: zeek='RESPONSE' ours=- |
| dnp3 | iin | count | 3 | dnp3_synthetic: zeek='0' ours=- |
| files | analyzers | set[string] | 1 | 80211http: zeek='(empty)' ours=- |
| files | duration | interval | 94 | 80211http: zeek='0.000000' ours=- |
| files | local_orig | bool | 94 | 80211http: zeek='F' ours=- |
| files | seen_bytes | count | 94 | 80211http: zeek='1478' ours=- |
| files | total_bytes | count | 59 | 80211http: zeek='29' ours=- |
| files | missing_bytes | count | 94 | 80211http: zeek='0' ours=- |
| files | overflow_bytes | count | 94 | 80211http: zeek='0' ours=- |
| ftp | password | string | 3 | arkime_synthetic: zeek='secretpassword' ours=- |
| http | origin | string | 4 | websocket: zeek='file://' ours=- |
| http | status_msg | string | 80 | 80211http: zeek='OK' ours=- |
| http | info_msg | string | 2 | http-100-continue: zeek='Continue' ours=- |
| http | proxied | set[string] | 2 | arkime_synthetic: zeek='X-FORWARDED-FOR -> 192.168.1.100\\x2c 10.0.0.50' ours=- |
| http | orig_fuids | vector[string] | 16 | arkime_synthetic: zeek='FNviaf2rTihd5beJg' ours=- |
| http | orig_filenames | vector[string] | 1 |  |
| http | orig_mime_types | vector[string] | 12 | arkime_synthetic: zeek='text/plain' ours=- |
| http | resp_fuids | vector[string] | 55 | 80211http: zeek='FeQetr4eawmYl8gWi8' ours=- |
| irc | user | string | 5 |  |
| irc | command | string | 12 | arkime_synthetic: zeek='NICK' ours=- |
| irc | addl | string | 5 | arkime_synthetic: zeek='0 * JohnDoe ' ours=- |
| kerberos | success | bool | 1 | arkime_synthetic: zeek='F' ours=- |
| kerberos | till | time | 3 | arkime_synthetic: zeek='1924991999.000000' ours=- |
| ldap | message_id | int | 13 | arkime_synthetic: zeek='1' ours=- |
| ldap | version | int | 11 | arkime_synthetic: zeek='3' ours=- |
| ldap | diagnostic_message | string | 2 | ntlm_synthetic: zeek='see matchedDN' ours=- |
| ldap | argument | string | 7 | arkime_synthetic: zeek='REDACTED' ours=- |
| ldap_search | ts | time | 2 |  |
| ldap_search | uid | string | 2 |  |
| ldap_search | id.orig_h | addr | 2 |  |
| ldap_search | id.orig_p | port | 2 |  |
| ldap_search | id.resp_h | addr | 2 |  |
| ldap_search | id.resp_p | port | 2 |  |
| ldap_search | message_id | int | 2 |  |
| ldap_search | scope | string | 2 |  |
| ldap_search | deref_aliases | string | 2 |  |
| ldap_search | base_object | string | 1 |  |
| ldap_search | result_count | count | 2 |  |
| ldap_search | result | string | 2 |  |
| ldap_search | filter | string | 2 |  |
| modbus | pdu_type | string | 598 | arkime_synthetic: zeek='REQ' ours=- |
| mqtt_connect | proto_name | string | 2 | arkime_synthetic: zeek='MQTT' ours=- |
| mqtt_connect | will_payload | string | 2 | arkime_synthetic: zeek='offline' ours=- |
| mqtt_publish | from_client | bool | 7 | arkime_synthetic: zeek='T' ours=- |
| mqtt_publish | retain | bool | 7 | arkime_synthetic: zeek='F' ours=- |
| mqtt_publish | status | string | 7 | arkime_synthetic: zeek='ok' ours=- |
| mqtt_publish | payload | string | 7 | arkime_synthetic: zeek='{"temp": 22.5, "unit": "C"}' ours=- |
| mqtt_subscribe | ts | time | 1 |  |
| mqtt_subscribe | uid | string | 1 |  |
| mqtt_subscribe | id.orig_h | addr | 1 |  |
| mqtt_subscribe | id.orig_p | port | 1 |  |
| mqtt_subscribe | id.resp_h | addr | 1 |  |
| mqtt_subscribe | id.resp_p | port | 1 |  |
| mqtt_subscribe | action | enum | 1 |  |
| mqtt_subscribe | topics | vector[string] | 1 |  |
| mqtt_subscribe | qos_levels | vector[count] | 1 |  |
| mqtt_subscribe | granted_qos_level | count | 1 |  |
| mqtt_subscribe | ack | bool | 1 |  |
| mysql | cmd | string | 3 | mysql-allow: zeek='login' ours=- |
| mysql | success | bool | 3 | mysql-allow: zeek='T' ours=- |
| mysql | rows | count | 2 | mysql-allow: zeek='0' ours=- |
| mysql | response | string | 1 | mysql-deny: zeek="Access denied for user 'user0'@'host123.local' (using password: NO)" ours=- |
| ntlm | success | bool | 5 | smb-smb1-ascii: zeek='T' ours=- |
| ocsp | ts | time | 1 |  |
| ocsp | id | string | 1 |  |
| ocsp | hashAlgorithm | string | 1 |  |
| ocsp | issuerNameHash | string | 1 |  |
| ocsp | issuerKeyHash | string | 1 |  |
| ocsp | serialNumber | string | 1 |  |
| ocsp | certStatus | string | 1 |  |
| ocsp | thisUpdate | time | 1 |  |
| ocsp | nextUpdate | time | 1 |  |
| packet_filter | ts | time | 199 |  |
| packet_filter | node | string | 199 |  |
| packet_filter | filter | string | 199 |  |
| packet_filter | init | bool | 199 |  |
| packet_filter | success | bool | 199 |  |
| postgresql | frontend | string | 7 | arkime_synthetic: zeek='startup' ours=- |
| postgresql | backend | string | 6 | arkime_synthetic: zeek='auth_ok' ours=- |
| postgresql | backend_arg | string | 4 | postgres-badpass: zeek='N' ours=- |
| postgresql | success | bool | 5 | arkime_synthetic: zeek='T' ours=- |
| quic | client_initial_dcid | string | 10 | arkime_synthetic: zeek='5354c48bc75080b2' ours=- |
| quic | history | string | 10 | arkime_synthetic: zeek='IIS' ours=- |
| radius | result | string | 4 | arkime_synthetic: zeek='unknown' ours=- |
| radius | ttl | interval | 1 | arkime_synthetic: zeek='1.000000' ours=- |
| rdp | cookie | string | 2 | arkime_synthetic: zeek='rdpuser' ours=- |
| rdp | result | string | 1 | arkime_synthetic: zeek='encrypted' ours=- |
| rdp | cert_count | count | 2 | arkime_synthetic: zeek='0' ours=- |
| reporter | ts | time | 1 |  |
| reporter | level | enum | 1 |  |
| reporter | message | string | 1 |  |
| reporter | location | string | 1 |  |
| sip | uri | string | 3 | arkime_synthetic: zeek='sip:bob@example.com' ours=- |
| sip | response_from | string | 2 | gtp-iphone: zeek='<tel:+88270006>' ours=- |
| sip | response_to | string | 2 | gtp-iphone: zeek='<tel:+88270006>;tag=659592208' ours=- |
| sip | seq | string | 3 | arkime_synthetic: zeek='1 INVITE' ours=- |
| sip | response_path | vector[string] | 2 | arkime_synthetic: zeek='(empty)' ours=- |
| sip | status_msg | string | 2 | gtp-iphone: zeek='Method Not Allowed' ours=- |
| sip | request_body_len | count | 2 | gtp-iphone: zeek='0' ours=- |
| sip | response_body_len | count | 2 | gtp-iphone: zeek='0' ours=- |
| sip | content_type | string | 1 | gtp-iphone: zeek='application/vnd.3gpp.sms' ours=- |
| smb_mapping | service | string | 6 | smb-smb1-ascii: zeek='IPC' ours=- |
| smb_mapping | native_file_system | string | 4 | smb-smbclient: zeek='NTFS' ours=- |
| smb_mapping | share_type | string | 7 | smb-smb1-ascii: zeek='PIPE' ours=- |
| smtp | first_received | string | 9 | smtp-originating: zeek='from [10.000.000.002] (helo=[10.000.00.03])\\x09by xxxxxxxxxxxxxxxxxxxx.xxxxxxxxx.net with esmtpa (Exim 4.67)\\x09(envelope-from <xxxxxxxx@xxxxxxxxx.net>)\\x09id xxxxxxxxxxxxxxxx\\x09for xxxxxxxx.xxxxxxx@xxxxxxxxxxxxxx.com; Mon, 13 Jan 2014 10:01:13 -0500' ours=- |
| smtp | second_received | string | 5 | smtp-subject-8859-multi: zeek='from xxxxxxxxxxxxx.xxx.com (localhost.localdomain [127.0.0.1])\\x09by xxxxxxxxxxxxx.xxx.com (8.14.4/8.14.4) with ESMTP id rBCJivEK007998\\x09for <xxxxxxxxx@xxxxxxx.com>; Thu, 12 Dec 2013 14:44:57 -0500' ours=- |
| smtp | path | vector[addr] | 24 | arkime_synthetic: zeek='10.0.5.1,10.0.5.100' ours=- |
| smtp | fuids | vector[string] | 16 | arkime_synthetic: zeek='FpL4Bh1lgHWfa5CMV1' ours='(empty)' |
| snmp | get_requests | count | 9 | SNMPv2c_get_requests: zeek='1' ours=- |
| snmp | get_bulk_requests | count | 9 | SNMPv2c_get_requests: zeek='0' ours=- |
| snmp | get_responses | count | 9 | SNMPv2c_get_requests: zeek='1' ours=- |
| snmp | set_requests | count | 9 | SNMPv2c_get_requests: zeek='0' ours=- |
| snmp | up_since | time | 1 | SNMPv2c_get_requests: zeek='1221599145.195237' ours=- |
| ssh | auth_success | bool | 1 | tcpdump-geneve: zeek='T' ours=- |
| ssh | auth_attempts | count | 9 | arkime_synthetic: zeek='0' ours=- |
| ssh | direction | enum | 1 | gre-sample: zeek='INBOUND' ours=- |
| ssl | ssl_history | string | 36 | arkime_synthetic: zeek='Cs' ours=- |
| ssl | client_cert_chain_fps | vector[string] | 1 | ssl-selfsign: zeek='28ab6f96a4eb1bba1c72fb980c07fc83d5816a6629ee1ebd9613b9e62d4a6805' ours='(empty)' |
| syslog | ts | time | 5 |  |
| syslog | uid | string | 5 |  |
| syslog | id.orig_h | addr | 5 |  |
| syslog | id.orig_p | port | 5 |  |
| syslog | id.resp_h | addr | 5 |  |
| syslog | id.resp_p | port | 5 |  |
| syslog | proto | enum | 5 |  |
| syslog | facility | string | 5 |  |
| syslog | severity | string | 5 |  |
| syslog | message | string | 5 |  |
| websocket | client_extensions | vector[string] | 1 | websocket: zeek='permessage-deflate; client_max_window_bits' ours=- |
| weird | ts | time | 89 |  |
| weird | uid | string | 76 |  |
| weird | id.orig_h | addr | 82 |  |
| weird | id.orig_p | port | 82 |  |
| weird | id.resp_h | addr | 82 |  |
| weird | id.resp_p | port | 82 |  |
| weird | name | string | 89 |  |
| weird | addl | string | 20 |  |
| weird | notice | bool | 89 |  |
| weird | peer | string | 89 |  |
| weird | source | string | 63 |  |

## conn.log

pcaps with zeek rows: 192, matched rows: 742

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 99% | 1% | 0% | 0% | bacnet-stack-services: zeek='1159067843.140235' ours='1159067992.853289'<br>bacnet-stack-services: zeek='1159067992.853289' ours='1159068136.387442'<br>dnssec_synthetic: zeek='1700021700.010000' ours='1700021700.000000' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | 6-4-gre-ppp-udp-4-dns: zeek='C8DHm91c6JNoIWAfDg' ours='C0cOhKF3McFKnYHmV8'<br>80211http: zeek='CbeFfuGvMxOz7E5kf' ours='C22P72dlzcpIMAyCUf'<br>CVE-2018-6794: zeek='Crd8KE3gNa3aiaQW0f' ours='CKy4v0V7mUTJdlCQyl' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| proto | enum | 100% | 100% | 100% | 0% | 0% | 0% | gtp-iphone: zeek='udp' ours='tcp' |
| service | string | 38% | 80% | 31% | 15% | 1% | 39% | arkime_synthetic: zeek='http' ours='http,adcs-web'<br>arkime_synthetic: zeek='http' ours='http,adcs-web'<br>arkime_synthetic: zeek='http' ours='http,adcs-web' |
| duration | interval | 24% | 100% | 54% | 13% | 0% | 33% | CVE-2018-6794: zeek='0.005377' ours='18.897878'<br>CVE-2018-6794: zeek='0.457120' ours='19.364190'<br>CVE-2018-6794: zeek='17.624393' ours='17.624504' |
| orig_bytes | count | 24% | 100% | 61% | 6% | 0% | 33% | ah-esp: zeek='0' ours='760'<br>arkime_synthetic: zeek='74' ours='39'<br>bacnet-stack-services: zeek='33' ours='346' |
| resp_bytes | count | 24% | 100% | 63% | 4% | 0% | 33% | aerospike: zeek='84' ours='0'<br>ah-esp: zeek='0' ours='760'<br>arkime_synthetic: zeek='45' ours='0' |
| conn_state | string | 100% | 100% | 86% | 14% | 0% | 0% | adb: zeek='RSTO' ours='RSTR'<br>arkime_synthetic: zeek='S0' ours='SF'<br>arkime_synthetic: zeek='SH' ours='OTH' |
| local_orig | bool | 100% | 0% | 0% | 0% | 100% | 0% | 6-4-gre-ppp-udp-4-dns: zeek='T' ours=-<br>80211http: zeek='F' ours=-<br>CVE-2018-6794: zeek='T' ours=- |
| local_resp | bool | 100% | 0% | 0% | 0% | 100% | 0% | 6-4-gre-ppp-udp-4-dns: zeek='F' ours=-<br>80211http: zeek='F' ours=-<br>CVE-2018-6794: zeek='T' ours=- |
| missed_bytes | count | 100% | 100% | 98% | 2% | 0% | 0% | aerospike: zeek='84' ours='0'<br>arkime_synthetic: zeek='35' ours='0'<br>kafka: zeek='71' ours='0' |
| history | string | 95% | 96% | 54% | 42% | 0% | 4% | 80211http: zeek='ShADadT' ours='ShADad'<br>CVE-2018-6794: zeek='ShdfADFT' ours='ShADadFfR'<br>CVE-2018-6794: zeek='ShdfADFT' ours='ShADadFfR' |
| orig_pkts | count | 100% | 100% | 87% | 13% | 0% | 0% | CVE-2018-6794: zeek='8' ours='10'<br>CVE-2018-6794: zeek='8' ours='10'<br>arkime_synthetic: zeek='1' ours='5' |
| orig_ip_bytes | count | 100% | 100% | 75% | 25% | 0% | 0% | 6-4-gre-ppp-udp-4-dns: zeek='99' ours='179'<br>80211http: zeek='5539' ours='6525'<br>CVE-2018-6794: zeek='2377' ours='2866' |
| resp_pkts | count | 100% | 100% | 88% | 12% | 0% | 0% | arkime_synthetic: zeek='0' ours='1'<br>arkime_synthetic: zeek='1' ours='0'<br>arkime_synthetic: zeek='0' ours='1' |
| resp_ip_bytes | count | 100% | 100% | 78% | 22% | 0% | 0% | 6-4-gre-ppp-udp-4-dns: zeek='174' ours='250'<br>80211http: zeek='4421' ours='4943'<br>CVE-2018-6794: zeek='205' ours='217' |
| tunnel_parents | set[string] | 2% | 0% | 0% | 0% | 3% | 0% | 6-4-gre-ppp-udp-4-dns: zeek='CnRBUr34MKaMjUszD2' ours=-<br>arkime_synthetic: zeek='Cvi3Sg3vRKEqd3XUn8' ours=-<br>gre-erspan-vxlan: zeek='CMXXWY2Ix0fP30Lvp1' ours=- |
| ip_proto | count | 100% | 100% | 100% | 0% | 0% | 0% | gtp-iphone: zeek='17' ours='6' |

## dce_rpc.log

pcaps with zeek rows: 2, matched rows: 11

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 0% | 100% | 0% | 0% | dcerpc_synthetic: zeek='1700000053.000000' ours='1700000048.000000'<br>dcerpc_synthetic: zeek='1700000060.000000' ours='1700000055.000000'<br>dcerpc_synthetic: zeek='1700000067.000000' ours='1700000062.000000' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | dcerpc_synthetic: zeek='CDq92B32skHCP7Bd8' ours='Cb6wDaVYWXhKPxapBD'<br>dcerpc_synthetic: zeek='CDPm8A1cjtPwIIs8wg' ours='CgHpnAWhuUc4ZlWyCj'<br>dcerpc_synthetic: zeek='ChRpP3FLBqWXyIvw6' ours='CcXKWjGuyyq7stLHSq' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| rtt | interval | 100% | 0% | 0% | 0% | 100% | 0% | dcerpc_synthetic: zeek='1.000000' ours=-<br>dcerpc_synthetic: zeek='1.000000' ours=-<br>dcerpc_synthetic: zeek='1.000000' ours=- |
| named_pipe | string | 8% | 0% | 0% | 0% | 0% | 0% |  |
| endpoint | string | 100% | 86% | 45% | 45% | 9% | 0% | dcerpc_synthetic: zeek='efsrpc2' ours='efsrpc'<br>dcerpc_synthetic: zeek='unknown-c54119df-89fe-794e-bf10-463657acf44d' ours='efsr'<br>dcerpc_synthetic: zeek='ITaskSchedulerService' ours='itaskscheduler' |
| operation | string | 100% | 0% | 0% | 0% | 100% | 0% | dcerpc_synthetic: zeek='EfsRpcQueryRecoveryAgents' ours=-<br>dcerpc_synthetic: zeek='unknown-8' ours=-<br>dcerpc_synthetic: zeek='LsarClearAuditLog' ours=- |

## dhcp.log

pcaps with zeek rows: 3, matched rows: 8

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 100% | 0% | 0% | 0% |  |
| uids | set[string] | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='Cv5OaC1Ggf3dye4MDk,C9yiErjEcxSLR0iI' ours='COWioiCv3lY3wOK35n'<br>feather_synthetic: zeek='C17hIh47OQUdc9JCF,CQJEmA3l9MIMCY7Xrj' ours='CPCteBILl562x509JA'<br>feather_synthetic: zeek='C17hIh47OQUdc9JCF,CyZBFa33EolXzjiSbd' ours='CTMqUeMdkWm6MwPRfp' |
| client_addr | addr | 80% | 88% | 88% | 0% | 0% | 0% |  |
| server_addr | addr | 80% | 25% | 25% | 0% | 62% | 0% | feather_synthetic: zeek='10.177.0.1' ours=-<br>feather_synthetic: zeek='10.177.0.1' ours=-<br>feather_synthetic: zeek='10.177.0.1' ours=- |
| mac | string | 100% | 100% | 100% | 0% | 0% | 0% |  |
| host_name | string | 80% | 88% | 88% | 0% | 0% | 0% |  |
| client_fqdn | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| domain | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| requested_addr | addr | 80% | 88% | 75% | 12% | 0% | 0% | wireshark-dhcp: zeek='0.0.0.0' ours='0.0.0.0,192.168.0.10' |
| assigned_addr | addr | 80% | 88% | 88% | 0% | 0% | 0% |  |
| lease_time | interval | 30% | 25% | 25% | 0% | 0% | 0% |  |
| client_message | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| server_message | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| msg_types | vector[string] | 100% | 100% | 88% | 12% | 0% | 0% | wireshark-dhcp: zeek='DISCOVER,OFFER' ours='ACK,OFFER,DISCOVER,REQUEST' |
| duration | interval | 100% | 100% | 88% | 12% | 0% | 0% | wireshark-dhcp: zeek='0.000295' ours='0.070345' |

## dnp3.log

pcaps with zeek rows: 1, matched rows: 2

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 0% | 100% | 0% | 0% | dnp3_synthetic: zeek='1770163205.000000' ours='1770163200.000000'<br>dnp3_synthetic: zeek='1770163212.000000' ours='1770163211.000000' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | dnp3_synthetic: zeek='Ccx0qH2RbhGvfnZ5K7' ours='CziEcmLyJHhARe9r2U'<br>dnp3_synthetic: zeek='CB4bxk3WaruYsAlpXj' ours='Ch7M66uipf7Al4zVxq' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| fc_request | string | 100% | 100% | 0% | 100% | 0% | 0% | dnp3_synthetic: zeek='READ' ours='Write,Response,Read'<br>dnp3_synthetic: zeek='READ' ours='ColdRestart,Response,Read' |
| fc_reply | string | 100% | 0% | 0% | 0% | 100% | 0% | dnp3_synthetic: zeek='RESPONSE' ours=-<br>dnp3_synthetic: zeek='RESPONSE' ours=- |
| iin | count | 100% | 0% | 0% | 0% | 100% | 0% | dnp3_synthetic: zeek='0' ours=-<br>dnp3_synthetic: zeek='0' ours=- |

## dns.log

pcaps with zeek rows: 25, matched rows: 564

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 14% | 86% | 0% | 0% | arkime_synthetic: zeek='1778361740.002000' ours='1700000020.000000'<br>arkime_synthetic: zeek='1778361740.002000' ours='1700000020.050000'<br>arkime_synthetic: zeek='1778361745.001000' ours='1700009200.000000' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | 6-4-gre-ppp-udp-4-dns: zeek='C8DHm91c6JNoIWAfDg' ours='C0cOhKF3McFKnYHmV8'<br>arkime_synthetic: zeek='CammOIYN3be2Qroyk' ours='CKfUKvUX49OFJxXGKv'<br>arkime_synthetic: zeek='CqfXS34strodI8CDu4' ours='CbWosSDG5a623WoOGn' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| proto | enum | 100% | 100% | 100% | 0% | 0% | 0% |  |
| trans_id | count | 100% | 100% | 100% | 0% | 0% | 0% |  |
| rtt | interval | 8% | 14% | 10% | 0% | 0% | 4% | arkime_synthetic: zeek='1.000000' ours=- |
| query | string | 98% | 99% | 100% | 0% | 0% | 0% |  |
| qclass | count | 89% | 100% | 100% | 0% | 0% | 0% |  |
| qclass_name | string | 89% | 100% | 100% | 0% | 0% | 0% |  |
| qtype | count | 89% | 100% | 100% | 0% | 0% | 0% |  |
| qtype_name | string | 89% | 100% | 100% | 0% | 0% | 0% |  |
| rcode | count | 22% | 19% | 13% | 0% | 0% | 1% | arkime_synthetic: zeek='0' ours=- |
| rcode_name | string | 22% | 19% | 13% | 0% | 0% | 1% | arkime_synthetic: zeek='NOERROR' ours=- |
| AA | bool | 100% | 100% | 98% | 2% | 0% | 0% | arkime_synthetic: zeek='F' ours='T'<br>arkime_synthetic: zeek='F' ours='T'<br>arkime_synthetic: zeek='F' ours='T' |
| TC | bool | 100% | 100% | 100% | 0% | 0% | 0% |  |
| RD | bool | 100% | 100% | 100% | 0% | 0% | 0% |  |
| RA | bool | 100% | 100% | 96% | 4% | 0% | 0% | 6-4-gre-ppp-udp-4-dns: zeek='F' ours='T'<br>arkime_synthetic: zeek='F' ours='T'<br>arkime_synthetic: zeek='F' ours='T' |
| Z | count | 100% | 100% | 99% | 1% | 0% | 0% | cloudshark-dtls1: zeek='2' ours='0'<br>dns-caa: zeek='2' ours='0'<br>dns-dnskey: zeek='1' ours='0' |
| answers | vector[string] | 18% | 15% | 8% | 1% | 0% | 4% | arkime_synthetic: zeek='NSEC example.org next.example.org' ours='NSEC'<br>arkime_synthetic: zeek='93.184.216.34' ours=-<br>cloudshark-dtls1: zeek='a.root-servers.net,b.root-servers.net,c.root-servers.net,d.root-servers.net,e.root-servers.net,f.root-servers.net,g.root-servers.net,h.root-servers.net,i.root-servers.net,j.root-servers.net,k.root-servers.net,l.root-servers.net,m.root-servers.net,RRSIG 2 <Root>' ours='a.root-servers.net,b.root-servers.net,c.root-servers.net,d.root-servers.net,e.root-servers.net,f.root-servers.net,g.root-servers.net,h.root-servers.net,i.root-servers.net,j.root-servers.net,k.root-servers.net,l.root-servers.net,m.root-servers.net' |
| TTLs | vector[interval] | 18% | 15% | 9% | 0% | 0% | 4% | arkime_synthetic: zeek='60.000000' ours=-<br>dnssec_synthetic: zeek='300.000000' ours='(empty)'<br>dnssec_synthetic: zeek='300.000000' ours='(empty)' |
| rejected | bool | 100% | 100% | 99% | 1% | 0% | 0% | arkime_synthetic: zeek='F' ours='T'<br>dnssec_synthetic: zeek='F' ours='T'<br>dnssec_synthetic: zeek='F' ours='T' |
| opcode | count | 100% | 100% | 100% | 0% | 0% | 0% |  |
| opcode_name | string | 100% | 100% | 100% | 0% | 0% | 0% |  |

## files.log

pcaps with zeek rows: 44, matched rows: 68

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 1% | 99% | 0% | 0% | 80211http: zeek='1523241953.985407' ours='1523241953.739049'<br>80211http: zeek='1523241954.432790' ours='1523241953.739049'<br>80211http: zeek='1523241975.662871' ours='1523241953.739049' |
| fuid | string | 100% | 100% | 0% | 100% | 0% | 0% | 80211http: zeek='FeQetr4eawmYl8gWi8' ours='Fa2lFzzd8JUGSil7Ax'<br>80211http: zeek='FP9ziX2uVzWkJkjsX9' ours='FIZi3S1UpnBIQwDUOj'<br>80211http: zeek='FHTgiZ1yydJq9UrGX3' ours='Fyrqy72WzKG92T4smX' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | 80211http: zeek='CbeFfuGvMxOz7E5kf' ours='C22P72dlzcpIMAyCUf'<br>80211http: zeek='CbeFfuGvMxOz7E5kf' ours='C22P72dlzcpIMAyCUf'<br>80211http: zeek='CbeFfuGvMxOz7E5kf' ours='C22P72dlzcpIMAyCUf' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| source | string | 100% | 100% | 100% | 0% | 0% | 0% |  |
| depth | count | 100% | 100% | 96% | 4% | 0% | 0% | smtp-subject-gb2312-b: zeek='2' ours='0'<br>smtp-subject-utf8-q: zeek='2' ours='0'<br>smtp-zip: zeek='2' ours='0' |
| analyzers | set[string] | 1% | 0% | 0% | 0% | 100% | 0% | 80211http: zeek='(empty)' ours=-<br>80211http: zeek='(empty)' ours=-<br>80211http: zeek='(empty)' ours=- |
| mime_type | string | 83% | 37% | 28% | 10% | 46% | 0% | 80211http: zeek='text/html' ours=-<br>80211http: zeek='text/html' ours=-<br>80211http: zeek='text/plain' ours=- |
| filename | string | 3% | 3% | 0% | 0% | 1% | 1% | http-post-upload: zeek='123.doc(5).exe' ours=- |
| duration | interval | 100% | 0% | 0% | 0% | 100% | 0% | 80211http: zeek='0.000000' ours=-<br>80211http: zeek='0.000000' ours=-<br>80211http: zeek='0.000000' ours=- |
| local_orig | bool | 100% | 0% | 0% | 0% | 100% | 0% | 80211http: zeek='F' ours=-<br>80211http: zeek='F' ours=-<br>80211http: zeek='F' ours=- |
| is_orig | bool | 100% | 6% | 4% | 0% | 96% | 0% | 80211http: zeek='F' ours=-<br>80211http: zeek='F' ours=-<br>80211http: zeek='T' ours=- |
| seen_bytes | count | 100% | 0% | 0% | 0% | 100% | 0% | 80211http: zeek='1478' ours=-<br>80211http: zeek='1478' ours=-<br>80211http: zeek='29' ours=- |
| total_bytes | count | 63% | 0% | 0% | 0% | 81% | 0% | 80211http: zeek='29' ours=-<br>80211http: zeek='575' ours=-<br>CVE-2018-6794: zeek='20' ours=- |
| missing_bytes | count | 100% | 0% | 0% | 0% | 100% | 0% | 80211http: zeek='0' ours=-<br>80211http: zeek='0' ours=-<br>80211http: zeek='0' ours=- |
| overflow_bytes | count | 100% | 0% | 0% | 0% | 100% | 0% | 80211http: zeek='0' ours=-<br>80211http: zeek='0' ours=-<br>80211http: zeek='0' ours=- |
| timedout | bool | 100% | 100% | 100% | 0% | 0% | 0% |  |
| parent_fuid | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| md5 | string | 0% | 100% | 0% | 0% | 0% | 100% |  |
| sha1 | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| sha256 | string | 0% | 62% | 0% | 0% | 0% | 60% |  |
| extracted | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| extracted_cutoff | bool | 0% | 0% | 0% | 0% | 0% | 0% |  |
| extracted_size | count | 0% | 0% | 0% | 0% | 0% | 0% |  |

## ftp.log

pcaps with zeek rows: 2, matched rows: 3

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='1737752643.000000' ours='1737752637.000000'<br>arkime_synthetic: zeek='1778361745.001000' ours='1700022600.000000'<br>ftp: zeek='1454301530.992076' ours='1454301530.932342' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='CBc7Se3UnxRhQLNWff' ours='CgEZy4DW4hQ9xcKbR3'<br>arkime_synthetic: zeek='CkTNYPbUTX6Q8w4w6' ours='ChlETSboW5QGtD6x2L'<br>ftp: zeek='CpeysC4CVr31L8UqNb' ours='CgqU8ftWjyi7MfQFoE' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| user | string | 100% | 100% | 67% | 33% | 0% | 0% | arkime_synthetic: zeek='<unknown>' ours='ftpuser' |
| password | string | 23% | 0% | 0% | 0% | 67% | 0% | arkime_synthetic: zeek='secretpassword' ours=-<br>ftp: zeek='ftp@example.com' ours=- |
| command | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='PASS' ours='PASS,USER'<br>arkime_synthetic: zeek='<init>' ours='STOR,HELO,PASS,USER,RETR'<br>ftp: zeek='PASS' ours='PASS,USER' |
| arg | string | 54% | 33% | 0% | 0% | 67% | 33% | arkime_synthetic: zeek='secretpassword' ours=-<br>ftp: zeek='ftp@example.com' ours=- |
| mime_type | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| file_size | count | 0% | 0% | 0% | 0% | 0% | 0% |  |
| reply_code | count | 62% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='230' ours='331,230,220'<br>arkime_synthetic: zeek='220' ours='331,226,230,220,500'<br>ftp: zeek='230' ours='331,230,220' |
| reply_msg | string | 62% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='Login successful' ours='331 Password required,230 Login successful,220 Welcome to Example FTP Server'<br>arkime_synthetic: zeek='ftp.example FTP server ready' ours='220 ftp.example FTP server ready,230 Logged in,331 Password required,500 Unknown command'<br>ftp: zeek='Login successful.' ours='331 Please specify the password.,230 Login successful.,220 Welcome to mirrors.rit.edu.' |
| data_channel.passive | bool | 0% | 0% | 0% | 0% | 0% | 0% |  |
| data_channel.orig_h | addr | 0% | 0% | 0% | 0% | 0% | 0% |  |
| data_channel.resp_h | addr | 0% | 0% | 0% | 0% | 0% | 0% |  |
| data_channel.resp_p | port | 0% | 0% | 0% | 0% | 0% | 0% |  |
| fuid | string | 0% | 0% | 0% | 0% | 0% | 0% |  |

## http.log

pcaps with zeek rows: 37, matched rows: 78

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 1% | 99% | 0% | 0% | 80211http: zeek='1523241953.976290' ours='1523241953.739049'<br>CVE-2018-6794: zeek='1516186786.707512' ours='1516186786.703072'<br>CVE-2018-6794: zeek='1516186788.621134' ours='1516186788.618106' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | 80211http: zeek='CbeFfuGvMxOz7E5kf' ours='C22P72dlzcpIMAyCUf'<br>CVE-2018-6794: zeek='Crd8KE3gNa3aiaQW0f' ours='CKy4v0V7mUTJdlCQyl'<br>CVE-2018-6794: zeek='CJrGS227ZxeghNrT71' ours='CvPY9dFOoRHED5ju10' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| trans_depth | count | 100% | 100% | 100% | 0% | 0% | 0% |  |
| method | string | 97% | 91% | 87% | 4% | 5% | 3% | 80211http: zeek='GET' ours='GET,POST'<br>arkime_synthetic: zeek='RPC_IN_DATA' ours=-<br>arkime_synthetic: zeek='RPC_OUT_DATA' ours=- |
| host | string | 86% | 87% | 83% | 4% | 4% | 4% | arkime_synthetic: zeek='mail.example.com' ours=-<br>arkime_synthetic: zeek='mail.example.com' ours=-<br>arkime_synthetic: zeek='mail.example.com' ours=- |
| uri | string | 97% | 92% | 76% | 17% | 4% | 3% | 80211http: zeek='/' ours='/static/css/index.css,/,/favicon.ico,/post.php,/static/images/banner.jpg,/static/css/base.css'<br>arkime_synthetic: zeek='/certsrv/mscep/mscep.dll?operation=GetCACert&message=foo' ours='/certsrv/mscep/mscep.dll'<br>arkime_synthetic: zeek='/rpc/rpcproxy.dll?MAIL.example.com:6001' ours=- |
| referrer | string | 16% | 17% | 9% | 0% | 0% | 4% |  |
| version | string | 84% | 91% | 74% | 3% | 6% | 17% | CVE-2018-6794: zeek='1.1' ours=-<br>arkime_synthetic: zeek='1.1' ours=-<br>arkime_synthetic: zeek='1.1' ours=- |
| user_agent | string | 68% | 53% | 62% | 0% | 4% | 4% | arkime_synthetic: zeek='MSRPC' ours=-<br>arkime_synthetic: zeek='MSRPC' ours=-<br>arkime_synthetic: zeek='MSRPC' ours=- |
| origin | string | 4% | 0% | 0% | 0% | 1% | 0% | websocket: zeek='file://' ours=- |
| request_body_len | count | 100% | 92% | 88% | 5% | 6% | 0% | 80211http: zeek='0' ours='0,39,29'<br>CVE-2018-6794: zeek='0' ours=-<br>arkime_synthetic: zeek='0' ours=- |
| response_body_len | count | 100% | 71% | 69% | 12% | 19% | 0% | 80211http: zeek='1478' ours='0,708,575'<br>gre-erspan-vxlan: zeek='10' ours='8'<br>http-100-continue: zeek='445' ours='0,445' |
| status_code | count | 84% | 75% | 78% | 4% | 1% | 3% | 80211http: zeek='200' ours='200,405,304'<br>ntlm_synthetic: zeek='401' ours='401,200'<br>ntlm_synthetic: zeek='401' ours='401,200' |
| status_msg | string | 84% | 0% | 0% | 0% | 83% | 0% | 80211http: zeek='OK' ours=-<br>CVE-2018-6794: zeek='OK' ours=-<br>CVE-2018-6794: zeek='OK' ours=- |
| info_code | count | 2% | 4% | 3% | 0% | 0% | 0% |  |
| info_msg | string | 2% | 0% | 0% | 0% | 3% | 0% | http-100-continue: zeek='Continue' ours=-<br>websocket: zeek='Switching Protocols' ours=- |
| tags | set[enum] | 0% | 0% | 100% | 0% | 0% | 0% |  |
| username | string | 2% | 4% | 3% | 0% | 0% | 1% |  |
| password | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| proxied | set[string] | 2% | 0% | 0% | 0% | 3% | 0% | arkime_synthetic: zeek='X-FORWARDED-FOR -> 192.168.1.100\\x2c 10.0.0.50' ours=-<br>http-xff: zeek='X-FORWARDED-FOR -> 10.000.000.03' ours=- |
| orig_fuids | vector[string] | 17% | 0% | 0% | 0% | 13% | 0% | arkime_synthetic: zeek='FNviaf2rTihd5beJg' ours=-<br>arkime_synthetic: zeek='FwBqui3svoZyUSYcD5' ours=-<br>arkime_synthetic: zeek='FEzHVL3wVm79PRr3F3' ours=- |
| orig_filenames | vector[string] | 1% | 0% | 0% | 0% | 0% | 0% |  |
| orig_mime_types | vector[string] | 13% | 0% | 0% | 0% | 9% | 0% | arkime_synthetic: zeek='text/plain' ours=-<br>arkime_synthetic: zeek='text/plain' ours=-<br>arkime_synthetic: zeek='text/plain' ours=- |
| resp_fuids | vector[string] | 58% | 0% | 0% | 0% | 59% | 0% | 80211http: zeek='FeQetr4eawmYl8gWi8' ours=-<br>CVE-2018-6794: zeek='FCH5FwXNKkAa4ubqj' ours=-<br>CVE-2018-6794: zeek='F5oVVj4yWd6glkNU7' ours=- |
| resp_filenames | vector[string] | 0% | 0% | 0% | 0% | 0% | 0% |  |
| resp_mime_types | vector[string] | 53% | 32% | 29% | 9% | 17% | 3% | 80211http: zeek='text/html' ours='text/html,application/x-gzip'<br>CVE-2018-6794: zeek='text/plain' ours=-<br>CVE-2018-6794: zeek='text/plain' ours=- |

## irc.log

pcaps with zeek rows: 2, matched rows: 5

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='1737752421.000000' ours='1737752417.000000'<br>arkime_synthetic: zeek='1737752587.000000' ours='1737752583.000000'<br>arkime_synthetic: zeek='1737752681.000000' ours='1737752677.000000' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='Cp67IW28qEnBFH3REh' ours='CdJ9yPJJJJo0qckJ9u'<br>arkime_synthetic: zeek='CIYgiZeAacdtfDHdh' ours='Ca6Hd44b9BaGIPtMG6'<br>arkime_synthetic: zeek='CrKbax2tuyVUBCzxG8' ours='C04JiEBHbCbKsrTZQf' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| nick | string | 58% | 83% | 0% | 0% | 20% | 80% | arkime_synthetic: zeek='johndoe' ours=- |
| user | string | 42% | 0% | 0% | 0% | 0% | 0% |  |
| command | string | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='NICK' ours=-<br>arkime_synthetic: zeek='NICK' ours=-<br>arkime_synthetic: zeek='NICK' ours=- |
| value | string | 100% | 83% | 0% | 80% | 20% | 0% | arkime_synthetic: zeek='testnick' ours='#test'<br>arkime_synthetic: zeek='testuser' ours='#testchannel'<br>arkime_synthetic: zeek='johndoe' ours='#programming,#security' |
| addl | string | 42% | 0% | 0% | 0% | 40% | 0% | arkime_synthetic: zeek='0 * JohnDoe ' ours=-<br>irc: zeek='+iw xxxxx XxxxxxXxxx ' ours=- |

## kerberos.log

pcaps with zeek rows: 3, matched rows: 3

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 67% | 33% | 0% | 0% | krb5-tcp: zeek='1466265698.251011' ours='1466265698.248990' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='CmFTrb11dX2nXryA9f' ours='C2xOVRJu6wl2gR1pq9'<br>krb5-tcp: zeek='C35pOt120hupjxmrm6' ours='CSbtaXZSHmlBMA5rLO'<br>krb5-udp: zeek='C8KlOv1MtlaTds3ZYa' ours='CzADEDsUQeg7imt9jN' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| request_type | string | 100% | 100% | 100% | 0% | 0% | 0% |  |
| client | string | 100% | 100% | 100% | 0% | 0% | 0% |  |
| service | string | 75% | 100% | 100% | 0% | 0% | 0% |  |
| success | bool | 25% | 0% | 0% | 0% | 33% | 0% | arkime_synthetic: zeek='F' ours=- |
| error_msg | string | 25% | 25% | 33% | 0% | 0% | 0% |  |
| from | time | 0% | 0% | 0% | 0% | 0% | 0% |  |
| till | time | 75% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='1924991999.000000' ours=-<br>krb5-tcp: zeek='2136422885.000000' ours=-<br>krb5-udp: zeek='1466288055.000000' ours=- |
| cipher | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| forwardable | bool | 75% | 100% | 100% | 0% | 0% | 0% |  |
| renewable | bool | 75% | 100% | 100% | 0% | 0% | 0% |  |
| client_cert_subject | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| client_cert_fuid | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| server_cert_subject | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| server_cert_fuid | string | 0% | 0% | 0% | 0% | 0% | 0% |  |

## ldap.log

pcaps with zeek rows: 5, matched rows: 8

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 25% | 75% | 0% | 0% | arkime_synthetic: zeek='1737752602.000000' ours='1737752599.000000'<br>arkime_synthetic: zeek='1737752675.000000' ours='1737752672.000000'<br>ldap-simpleauth: zeek='1463256456.042522' ours='1463256456.040464' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='CXwOoJ2RyY0CGcIFo' ours='CHWadKRtbhuDim6V5w'<br>arkime_synthetic: zeek='C5EmZJ1sfaJrmn0Isd' ours='CEqfglTutc37L8C02r'<br>ldap-and-search: zeek='CfEXMq1YTalyxI75pf' ours='CwJGECzNeNOBTTbzXS' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| message_id | int | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='1' ours=-<br>arkime_synthetic: zeek='1' ours=-<br>ldap-and-search: zeek='1' ours=- |
| version | int | 85% | 0% | 0% | 0% | 88% | 0% | arkime_synthetic: zeek='3' ours=-<br>arkime_synthetic: zeek='3' ours=-<br>ldap-and-search: zeek='3' ours=- |
| opcode | string | 100% | 90% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='bind simple' ours='bind'<br>arkime_synthetic: zeek='bind simple' ours='bind_response,bind'<br>ldap-and-search: zeek='bind simple' ours='search_result,bind_response,search,bind' |
| result | string | 77% | 60% | 25% | 38% | 12% | 12% | ldap-ssl: zeek='success' ours=-<br>ntlm_synthetic: zeek='success' ours='saslBindInProgress,success'<br>ntlm_synthetic: zeek='SASL bind in progress' ours='saslBindInProgress,success' |
| diagnostic_message | string | 15% | 0% | 0% | 0% | 12% | 0% | ntlm_synthetic: zeek='see matchedDN' ours=- |
| object | string | 46% | 80% | 25% | 25% | 12% | 38% | ldap-simpleauth: zeek='xxxxxxxxxxx@xx.xxx.xxxxx.net' ours='xxxxxxxxxxx@xx.xxx.xxxxx.net,CN=xxxxxxxx\\x2cOU=Users\\x2cOU=Accounts\\x2cDC=xx\\x2cDC=xxx\\x2cDC=xxxxx\\x2cDC=net'<br>ldap-ssl: zeek='1.3.6.1.4.1.1466.20037 (StartTLS)' ours=-<br>ntlm_synthetic: zeek='cn=anon' ours='<ROOT>,cn=anon' |
| argument | string | 54% | 0% | 0% | 0% | 38% | 0% | arkime_synthetic: zeek='REDACTED' ours=-<br>ldap-simpleauth: zeek='REDACTED' ours=-<br>ntlm_synthetic: zeek='NTLM' ours=- |

## modbus.log

pcaps with zeek rows: 2, matched rows: 8

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='1737752449.000000' ours='1737752446.000000'<br>arkime_synthetic: zeek='1737752624.000000' ours='1737752621.000000'<br>arkime_synthetic: zeek='1778361745.001000' ours='1700011000.000000' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='C1sDpl3PQIGWj1lzl5' ours='CAnW1qR4oQJIZgtSOq'<br>arkime_synthetic: zeek='Cisgwt3NURfQaLNT27' ours='CgET3VWdEAXKuT2TW3'<br>arkime_synthetic: zeek='C8vYcvjPnVqt8hEDj' ours='CbCqs9fdhZL7bjgjh9' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| tid | count | 100% | 90% | 88% | 12% | 0% | 0% | modbus: zeek='1' ours='139,93,232,47,186,140,94,233,48,187,1,2,141,95,234,49,188,3,142,96,235,50,189,4,143,97,236,51,190,5,144,98,237,52,191,6,145,99,238,53,192,7,146,100,239,54,193,8,147,101,240,55,194,9,148,102,241,56,195,10,149,103,242,57,196,11,150,104,243,58,197,12,151,105,244,59,198,13,152,106,245,60,199,14,153,107,246,61,200,15,154,108,247,62,201,16,155,109,248,63,202,17,156,110,249,64,203,18,157,111,250,65,204,19,158,112,251,66,205,20,159,113,252,67,206,21,160,114,253,68,207,22,161,115,254,69,208,23,162,116,255,70,209,24,163,117,256,71,210,25,164,118,72,211,26,165,119,73,212,27,166,120,74,213,28,167,121,75,214,29,168,122,76,215,30,169,123,77,216,31,170,124,78,217,32,171,125,79,218,33,172,126,80,219,34,173,127,81,220,35,174,128,82,221,36,175,129,83,222,37,176,130,84,223,38,177,131,85,224,39,178,132,86,225,40,179,133,87,226,41,180,134,88,227,42,181,135,89,228,43,182,136,90,229,44,183,137,91,230,45,184,138,92,231,46,185' |
| unit | count | 100% | 100% | 100% | 0% | 0% | 0% |  |
| func | string | 100% | 90% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='READ_HOLDING_REGISTERS' ours='3'<br>arkime_synthetic: zeek='READ_HOLDING_REGISTERS' ours='3'<br>arkime_synthetic: zeek='READ_HOLDING_REGISTERS' ours='3' |
| pdu_type | string | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='REQ' ours=-<br>arkime_synthetic: zeek='REQ' ours=-<br>arkime_synthetic: zeek='REQ' ours=- |
| exception | string | 4% | 50% | 0% | 0% | 0% | 38% |  |

## mqtt_connect.log

pcaps with zeek rows: 2, matched rows: 2

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='1737753003.000000' ours='1737753000.000000'<br>mqtt_big: zeek='1769778003.000000' ours='1769778000.000000' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='CGJOxFpcqxl4uhdTl' ours='ComWEYWv6ci3lRBrIP'<br>mqtt_big: zeek='CNG20tW4RspCrmR19' ours='CYBgfAouTME30JF3NC' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| proto_name | string | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='MQTT' ours=-<br>mqtt_big: zeek='MQTT' ours=- |
| proto_version | string | 100% | 86% | 100% | 0% | 0% | 0% |  |
| client_id | string | 100% | 86% | 100% | 0% | 0% | 0% |  |
| connect_status | string | 100% | 14% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='Connection Accepted' ours='0'<br>mqtt_big: zeek='Connection Accepted' ours='0' |
| will_topic | string | 100% | 21% | 100% | 0% | 0% | 0% |  |
| will_payload | string | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='offline' ours=-<br>mqtt_big: zeek='goodbye' ours=- |

## mqtt_publish.log

pcaps with zeek rows: 3, matched rows: 4

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='1737753005.000000' ours='1737753000.000000'<br>mqtt_big: zeek='1769778007.000000' ours='1769778000.000000'<br>mqtt_synthetic: zeek='1700000003.200000' ours='1700000000.000000' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='CGJOxFpcqxl4uhdTl' ours='ComWEYWv6ci3lRBrIP'<br>mqtt_big: zeek='CNG20tW4RspCrmR19' ours='CYBgfAouTME30JF3NC'<br>mqtt_synthetic: zeek='C3Ja5233QqWm6oMGSk' ours='CHkagcXqwBF585bGJS' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| from_client | bool | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='T' ours=-<br>mqtt_big: zeek='T' ours=-<br>mqtt_synthetic: zeek='T' ours=- |
| retain | bool | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='F' ours=-<br>mqtt_big: zeek='F' ours=-<br>mqtt_synthetic: zeek='F' ours=- |
| qos | string | 100% | 50% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='at least once' ours='1'<br>mqtt_big: zeek='at most once' ours='0,1,2'<br>mqtt_synthetic: zeek='at most once' ours='0,1' |
| status | string | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='ok' ours=-<br>mqtt_big: zeek='ok' ours=-<br>mqtt_synthetic: zeek='ok' ours=- |
| topic | string | 100% | 64% | 25% | 75% | 0% | 0% | mqtt_big: zeek='test/small' ours='test/small,server/interrupt,test/qos2,test/afterbig,test/bigdata,test/topic1,test/topic2,test/qos1,test/#,server/bigdata,test/interleaved'<br>mqtt_synthetic: zeek='sensors/temperature' ours='sensors/temperature,sensors/humidity,commands/+,sensors/#'<br>mqtt_synthetic: zeek='public/news' ours='public/weather,public/news,public/stocks' |
| payload | string | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='{"temp": 22.5, "unit": "C"}' ours=-<br>mqtt_big: zeek='Hello MQTT!' ours=-<br>mqtt_synthetic: zeek='25.5' ours=- |
| payload_len | count | 100% | 50% | 25% | 75% | 0% | 0% | mqtt_big: zeek='11' ours='12000,11,13,16,11000,15000,15'<br>mqtt_synthetic: zeek='4' ours='4,3'<br>mqtt_synthetic: zeek='13' ours='13,5' |

## mysql.log

pcaps with zeek rows: 2, matched rows: 2

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 0% | 100% | 0% | 0% | mysql-allow: zeek='1398217358.963625' ours='1398217358.961255'<br>mysql-deny: zeek='1398195861.495972' ours='1398195861.493255' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | mysql-allow: zeek='CzQkCR2PQ0pUSwQmZ6' ours='CJL9smqOH5wEKTQvOp'<br>mysql-deny: zeek='CuYi0u26dlUMROOmNk' ours='CemgG8p7XDwEpVZEX6' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| cmd | string | 100% | 0% | 0% | 0% | 100% | 0% | mysql-allow: zeek='login' ours=-<br>mysql-deny: zeek='login' ours=- |
| arg | string | 100% | 83% | 100% | 0% | 0% | 0% |  |
| success | bool | 100% | 0% | 0% | 0% | 100% | 0% | mysql-allow: zeek='T' ours=-<br>mysql-deny: zeek='F' ours=- |
| rows | count | 67% | 0% | 0% | 0% | 50% | 0% | mysql-allow: zeek='0' ours=- |
| response | string | 33% | 0% | 0% | 0% | 50% | 0% | mysql-deny: zeek="Access denied for user 'user0'@'host123.local' (using password: NO)" ours=- |

## ntlm.log

pcaps with zeek rows: 5, matched rows: 17

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 0% | 100% | 0% | 0% | ntlm_synthetic: zeek='1700000000.030000' ours='1700000000.010000'<br>ntlm_synthetic: zeek='1700000000.606000' ours='1700000000.586000'<br>ntlm_synthetic: zeek='1700000001.194000' ours='1700000001.174000' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | ntlm_synthetic: zeek='CrmOQLwRu2fnGsrDb' ours='C0epbNOgLImHGeDtjd'<br>ntlm_synthetic: zeek='CLbKnV2q880Txd56kl' ours='CV5dIoif3da0xbWBcN'<br>ntlm_synthetic: zeek='Cy440m4PvyDnTw5U4b' ours='CtkJvgrlGwjDcVcbum' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| username | string | 76% | 84% | 76% | 0% | 0% | 12% |  |
| hostname | string | 82% | 96% | 82% | 0% | 0% | 18% |  |
| domainname | string | 76% | 88% | 76% | 0% | 0% | 12% |  |
| server_nb_computer_name | string | 59% | 40% | 59% | 0% | 0% | 0% |  |
| server_dns_computer_name | string | 59% | 40% | 59% | 0% | 0% | 0% |  |
| server_tree_name | string | 6% | 4% | 6% | 0% | 0% | 0% |  |
| success | bool | 29% | 0% | 0% | 0% | 29% | 0% | smb-smb1-ascii: zeek='T' ours=-<br>smbtorture-ntlmssp-moloch-crash: zeek='T' ours=-<br>smbtorture-ntlmssp: zeek='T' ours=- |

## ntp.log

pcaps with zeek rows: 3, matched rows: 7

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 100% | 0% | 0% | 0% |  |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='CySLcw30X0Rauz62ie' ours='CU4v7OjJt3y07mxCmc'<br>arkime_synthetic: zeek='Cnw4Dv2bJ4oc13GPT9' ours='CM87HSrtAdQ1riixnc'<br>gre-sample: zeek='CzQWuK2S8Rza5rHRP1' ours='C6h8lGdJRRM2r1cCVg' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| version | count | 100% | 100% | 100% | 0% | 0% | 0% |  |
| mode | count | 100% | 100% | 43% | 57% | 0% | 0% | arkime_synthetic: zeek='3' ours='3,4'<br>gre-sample: zeek='3' ours='3,4'<br>gre-sample: zeek='3' ours='3,4' |
| stratum | count | 100% | 100% | 43% | 57% | 0% | 0% | arkime_synthetic: zeek='0' ours='0,1'<br>gre-sample: zeek='11' ours='2,11'<br>gre-sample: zeek='11' ours='11,3' |
| poll | interval | 100% | 100% | 100% | 0% | 0% | 0% |  |
| precision | interval | 100% | 100% | 71% | 29% | 0% | 0% | gre-sample: zeek='0.000008' ours='0.000004,0.000008'<br>gre-sample: zeek='0.000008' ours='0.000488,0.000008' |
| root_delay | interval | 100% | 100% | 57% | 43% | 0% | 0% | gre-sample: zeek='0.000000' ours='0.014587,0.000000'<br>gre-sample: zeek='0.000000' ours='0.034409,0.000000'<br>gre-sample: zeek='0.000000' ours='0.022308,0.000000' |
| root_disp | interval | 100% | 100% | 57% | 43% | 0% | 0% | gre-sample: zeek='0.011078' ours='0.011078,0.011292'<br>gre-sample: zeek='0.011368' ours='0.033829,0.011368'<br>gre-sample: zeek='0.011414' ours='0.047394,0.011414' |
| ref_id | string | 100% | 86% | 29% | 57% | 14% | 0% | arkime_synthetic: zeek='\\x00\\x00\\x00\\x00' ours=-<br>arkime_synthetic: zeek='\\x00\\x00\\x00\\x00' ours='GPS'<br>gre-sample: zeek='127.127.1.1' ours='cdbcb921,7f7f0101' |
| ref_time | time | 100% | 43% | 0% | 43% | 57% | 0% | arkime_synthetic: zeek='0.000000' ours=-<br>arkime_synthetic: zeek='0.000000' ours=-<br>gre-sample: zeek='1055289964.850213' ours='1055289964.850212,1055289880.891366' |
| org_time | time | 100% | 43% | 0% | 43% | 57% | 0% | arkime_synthetic: zeek='0.000000' ours=-<br>arkime_synthetic: zeek='0.000000' ours=-<br>gre-sample: zeek='1055289927.417650' ours='1055289927.417650,1055289973.849817' |
| rec_time | time | 100% | 43% | 0% | 43% | 57% | 0% | arkime_synthetic: zeek='0.000000' ours=-<br>arkime_synthetic: zeek='0.000000' ours=-<br>gre-sample: zeek='1055289910.992244' ours='1055289910.992243,1055289993.233992' |
| xmt_time | time | 100% | 71% | 29% | 43% | 29% | 0% | arkime_synthetic: zeek='0.000000' ours=-<br>arkime_synthetic: zeek='0.000000' ours=-<br>gre-sample: zeek='1055289973.849818' ours='1055289973.849817,1055289993.234260' |
| num_exts | count | 100% | 100% | 100% | 0% | 0% | 0% |  |

## postgresql.log

pcaps with zeek rows: 4, matched rows: 5

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='1737752606.000000' ours='1737752603.000000'<br>arkime_synthetic: zeek='1737752659.000000' ours='1737752656.000000'<br>postgres-badpass: zeek='1399312748.535522' ours='1399312748.531029' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='CKIwOIrzc5lKZrGR4' ours='C1LLi4ScomA1zNAi9P'<br>arkime_synthetic: zeek='CjKqZq31njX5YcQhwi' ours='Cl77NDan3MQ2YLngUb'<br>postgres-badpass: zeek='CKDrW91zdA6vDpHKth' ours='CgLjRsyr3PvJjbfsjF' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| user | string | 78% | 100% | 60% | 0% | 0% | 40% |  |
| database | string | 67% | 100% | 60% | 0% | 0% | 40% |  |
| application_name | string | 33% | 80% | 20% | 0% | 0% | 60% |  |
| frontend | string | 78% | 0% | 0% | 0% | 80% | 0% | arkime_synthetic: zeek='startup' ours=-<br>postgres-badpass: zeek='ssl_request' ours=-<br>postgres-good: zeek='ssl_request' ours=- |
| frontend_arg | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| backend | string | 67% | 0% | 0% | 0% | 80% | 0% | arkime_synthetic: zeek='auth_ok' ours=-<br>postgres-badpass: zeek='ssl_reply' ours=-<br>postgres-good: zeek='ssl_reply' ours=- |
| backend_arg | string | 44% | 0% | 0% | 0% | 40% | 0% | postgres-badpass: zeek='N' ours=-<br>postgres-good: zeek='N' ours=- |
| success | bool | 56% | 0% | 0% | 0% | 80% | 0% | arkime_synthetic: zeek='T' ours=-<br>postgres-badpass: zeek='F' ours=-<br>postgres-good: zeek='F' ours=- |
| rows | count | 0% | 0% | 0% | 0% | 0% | 0% |  |

## quic.log

pcaps with zeek rows: 3, matched rows: 6

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 50% | 50% | 0% | 0% | arkime_synthetic: zeek='1778361745.001000' ours='1700021500.000000'<br>arkime_synthetic: zeek='1778361745.001000' ours='1700022400.000000'<br>arkime_synthetic: zeek='1778361745.001000' ours='1700023600.000000' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='Cwp5iq1doFQTFRhOG4' ours='C00gJXQMWLFC7986KO'<br>arkime_synthetic: zeek='Cp7QgQ6f7ingGXWaa' ours='CwHICOWYW05KSUFFWr'<br>arkime_synthetic: zeek='ClIJJndBhoyFtoNmg' ours='CtLZO6VR9en9vEWNxB' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| version | string | 100% | 42% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='1' ours=-<br>arkime_synthetic: zeek='1' ours=-<br>arkime_synthetic: zeek='1' ours=- |
| client_initial_dcid | string | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='5354c48bc75080b2' ours=-<br>arkime_synthetic: zeek='8394c8f03e515708' ours=-<br>arkime_synthetic: zeek='5a5b5c5d5e5f6061' ours=- |
| client_scid | string | 0% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='(empty)' ours=-<br>arkime_synthetic: zeek='(empty)' ours=-<br>arkime_synthetic: zeek='(empty)' ours=- |
| server_scid | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| server_name | string | 60% | 50% | 0% | 0% | 67% | 0% | arkime_synthetic: zeek='config.edge.skype.com' ours=-<br>arkime_synthetic: zeek='quic-v2.example' ours=-<br>quicietf: zeek='ssl.gstatic.com' ours=- |
| client_protocol | string | 60% | 50% | 67% | 0% | 0% | 33% |  |
| history | string | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='IIS' ours=-<br>arkime_synthetic: zeek='I' ours=-<br>arkime_synthetic: zeek='I' ours=- |

## radius.log

pcaps with zeek rows: 1, matched rows: 3

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 67% | 33% | 0% | 0% | arkime_synthetic: zeek='1778361745.001000' ours='1700013000.599999' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='C3huT14p0mmSDgnI6g' ours='CklYaDaQfBTGFzoI81'<br>arkime_synthetic: zeek='CbXpkg3bFPk9UZJB5' ours='CjpAPsVWbaNH4Kj2eN'<br>arkime_synthetic: zeek='CNehDz3eZoqK5Gkny7' ours='C1NsVtjVLJKG9VYQsL' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| username | string | 100% | 100% | 100% | 0% | 0% | 0% |  |
| mac | string | 0% | 14% | 0% | 0% | 0% | 0% |  |
| framed_addr | addr | 0% | 57% | 0% | 0% | 0% | 33% |  |
| tunnel_client | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| connect_info | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| reply_msg | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| result | string | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='unknown' ours=-<br>arkime_synthetic: zeek='success' ours=-<br>arkime_synthetic: zeek='unknown' ours=- |
| ttl | interval | 25% | 0% | 0% | 0% | 33% | 0% | arkime_synthetic: zeek='1.000000' ours=- |

## rdp.log

pcaps with zeek rows: 2, matched rows: 2

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='1737752971.000000' ours='1737752970.000000'<br>socks5-rdp: zeek='1386644256.760443' ours='1386644255.859306' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='CY5rpi3onh2RMADdx2' ours='CclUC8UQyck2BrMFeG'<br>socks5-rdp: zeek='CMCYdj2XkTXcbcLmSc' ours='CbBwFKZ9EiEEGPCJjQ' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| cookie | string | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='rdpuser' ours=-<br>socks5-rdp: zeek='XXX' ours=- |
| result | string | 50% | 0% | 0% | 0% | 50% | 0% | arkime_synthetic: zeek='encrypted' ours=- |
| security_protocol | string | 100% | 67% | 100% | 0% | 0% | 0% |  |
| client_channels | vector[string] | 0% | 0% | 0% | 0% | 0% | 0% |  |
| keyboard_layout | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| client_build | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| client_name | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| client_dig_product_id | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| desktop_width | count | 0% | 0% | 0% | 0% | 0% | 0% |  |
| desktop_height | count | 0% | 0% | 0% | 0% | 0% | 0% |  |
| requested_color_depth | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| cert_type | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| cert_count | count | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='0' ours=-<br>socks5-rdp: zeek='0' ours=- |
| cert_permanent | bool | 0% | 0% | 0% | 0% | 0% | 0% |  |
| encryption_level | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| encryption_method | string | 0% | 0% | 0% | 0% | 0% | 0% |  |

## sip.log

pcaps with zeek rows: 2, matched rows: 3

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 33% | 67% | 0% | 0% | gtp-iphone: zeek='1415985445.984189' ours='1415985412.100117'<br>gtp-iphone: zeek='1415985512.340163' ours='1415985444.983920' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='CyzcxU1sOmTJllC8Ue' ours='CccBHIessMYGZnIkwc'<br>gtp-iphone: zeek='CK1hqw1VjUhic3OP4f' ours='CNCTZ2849wH5z9gySj'<br>gtp-iphone: zeek='Co8rc9rNr18kaqCG' ours='CNCTZ2849wH5z9gySj' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| trans_depth | count | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='0' ours='1'<br>gtp-iphone: zeek='0' ours='1'<br>gtp-iphone: zeek='0' ours='1' |
| method | string | 100% | 100% | 33% | 67% | 0% | 0% | gtp-iphone: zeek='SUBSCRIBE' ours='REGISTER'<br>gtp-iphone: zeek='MESSAGE' ours='REGISTER' |
| uri | string | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='sip:bob@example.com' ours=-<br>gtp-iphone: zeek='tel:+88270006' ours=-<br>gtp-iphone: zeek='tel:+7' ours=- |
| date | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| request_from | string | 100% | 100% | 33% | 67% | 0% | 0% | gtp-iphone: zeek='<tel:+88270006>' ours='<tel:263734>;tag=cVKbrkhG0L'<br>gtp-iphone: zeek='<tel:+88270006>' ours='<tel:263734>;tag=1QGDmSrhTP' |
| request_to | string | 100% | 100% | 33% | 67% | 0% | 0% | gtp-iphone: zeek='<tel:+88270006>' ours='<tel:263734>'<br>gtp-iphone: zeek='<tel:+7>' ours='<tel:263734>;tag=497094864,<tel:263734>' |
| response_from | string | 67% | 0% | 0% | 0% | 67% | 0% | gtp-iphone: zeek='<tel:+88270006>' ours=-<br>gtp-iphone: zeek='<tel:+88270006>' ours=- |
| response_to | string | 67% | 0% | 0% | 0% | 67% | 0% | gtp-iphone: zeek='<tel:+88270006>;tag=659592208' ours=-<br>gtp-iphone: zeek='<tel:+7>;tag=210896220' ours=- |
| reply_to | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| call_id | string | 100% | 100% | 33% | 67% | 0% | 0% | gtp-iphone: zeek='5E6Cykks9Q8hoKaqHXRaeWJr' ours='5PC5dBCGDQ0csFmFPQXQjE57'<br>gtp-iphone: zeek='Ibayc7qnh1PkQzsM6S0TeFJ6' ours='5PC5dBCGDQ0csFmFPQXQjE57' |
| seq | string | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='1 INVITE' ours=-<br>gtp-iphone: zeek='1 SUBSCRIBE' ours=-<br>gtp-iphone: zeek='2 MESSAGE' ours=- |
| subject | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| request_path | vector[string] | 100% | 100% | 33% | 67% | 0% | 0% | gtp-iphone: zeek='SIP/2.0/UDP [fd00:183:1:1:1886:9040:8605:32b8]:5060' ours='SIP/2.0/TCP [fd00:183:1:1:1886:9040:8605:32b8]:5060;branch=z9hG4bKVT3F3PbXtxKWEdp;rport=5060;received=fd00:183:1:1:1886:9040:8605:32b8,SIP/2.0/TCP [fd00:183:1:1:1886:9040:8605:32b8]:5060;branch=z9hG4bKVT3F3PbXtxKWEdp;rport'<br>gtp-iphone: zeek='SIP/2.0/UDP [fd00:183:1:1:1886:9040:8605:32b8]:5060' ours='SIP/2.0/TCP [fd00:183:1:1:1886:9040:8605:32b8]:5060;branch=z9hG4bK0iTZW12JDCCkuJn;rport,SIP/2.0/TCP [fd00:183:1:1:1886:9040:8605:32b8]:5060;branch=z9hG4bK0iTZW12JDCCkuJn;rport=5060;received=fd00:183:1:1:1886:9040:8605:32b8' |
| response_path | vector[string] | 67% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='(empty)' ours=-<br>gtp-iphone: zeek='SIP/2.0/UDP [fd00:183:1:1:1886:9040:8605:32b8]:5060' ours=-<br>gtp-iphone: zeek='SIP/2.0/UDP [fd00:183:1:1:1886:9040:8605:32b8]:5060' ours=- |
| user_agent | string | 67% | 50% | 67% | 0% | 0% | 0% |  |
| status_code | count | 67% | 70% | 0% | 67% | 0% | 0% | gtp-iphone: zeek='405' ours='401'<br>gtp-iphone: zeek='202' ours='200' |
| status_msg | string | 67% | 0% | 0% | 0% | 67% | 0% | gtp-iphone: zeek='Method Not Allowed' ours=-<br>gtp-iphone: zeek='Accepted' ours=- |
| warning | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| request_body_len | count | 67% | 0% | 0% | 0% | 67% | 0% | gtp-iphone: zeek='0' ours=-<br>gtp-iphone: zeek='20' ours=- |
| response_body_len | count | 67% | 0% | 0% | 0% | 67% | 0% | gtp-iphone: zeek='0' ours=-<br>gtp-iphone: zeek='13' ours=- |
| content_type | string | 33% | 0% | 0% | 0% | 33% | 0% | gtp-iphone: zeek='application/vnd.3gpp.sms' ours=- |

## smb_mapping.log

pcaps with zeek rows: 5, matched rows: 6

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 0% | 100% | 0% | 0% | smb-smb1-ascii: zeek='1429783875.385768' ours='1429783872.765752'<br>smb-smbclient: zeek='1387494788.143537' ours='1387494788.141593'<br>smbtorture-ntlmssp-moloch-crash: zeek='1132371118.591099' ours='1132371118.535687' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | smb-smb1-ascii: zeek='CtEjKEu45VzVTPOM7' ours='Cv87LldY9qsCutp5m5'<br>smb-smbclient: zeek='CTzUhI2hudKBVPdpgi' ours='CJpAH8mCtQBJSzBZds'<br>smbtorture-ntlmssp-moloch-crash: zeek='CIyqZT3jHJ1YhnjYY2' ours='C7wnXIrokU80bUXtcP' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| path | string | 100% | 32% | 83% | 17% | 0% | 0% | smb-smb1-ascii: zeek='\\\\\\\\10.10.0.2\\\\IPC$' ours='\\\\\\\\10.10.0.2\\\\IPC$\\x00\\x00\\x00\\x00\\x00\\x00\\x00,\\\\\\\\10.10.0.2\\\\ADMIN$\\x00\\x00\\x00\\x00\\x00\\x00\\x00' |
| service | string | 86% | 0% | 0% | 0% | 83% | 0% | smb-smb1-ascii: zeek='IPC' ours=-<br>smb-smbclient: zeek='A:' ours=-<br>smbtorture-ntlmssp-moloch-crash: zeek='A:' ours=- |
| native_file_system | string | 57% | 0% | 0% | 0% | 50% | 0% | smb-smbclient: zeek='NTFS' ours=-<br>smbtorture-ntlmssp-moloch-crash: zeek='NTFS' ours=-<br>smbtorture-ntlmssp: zeek='NTFS' ours=- |
| share_type | string | 100% | 0% | 0% | 0% | 100% | 0% | smb-smb1-ascii: zeek='PIPE' ours=-<br>smb-smbclient: zeek='DISK' ours=-<br>smbtorture-ntlmssp-moloch-crash: zeek='DISK' ours=- |

## smtp.log

pcaps with zeek rows: 20, matched rows: 24

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 4% | 96% | 0% | 0% | arkime_synthetic: zeek='1737752415.000000' ours='1737752412.000000'<br>arkime_synthetic: zeek='1737752560.000000' ours='1737752557.000000'<br>smtp-data-250: zeek='1386338020.579419' ours='1386338020.379409' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='CvJ5UO1A5nIHKqA7qh' ours='CEjOgiGAilD40AUIlN'<br>arkime_synthetic: zeek='CF14i72ipDe7auxu2a' ours='CUTGcJMj2xq1wRRv9a'<br>smtp-data-250: zeek='CvFqAGm1kL2Gc1Onc' ours='C8uGy9Qx0SxKEURGlE' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| trans_depth | count | 100% | 100% | 100% | 0% | 0% | 0% |  |
| helo | string | 96% | 93% | 96% | 0% | 4% | 0% | smtp-nospaces: zeek='(empty)' ours=- |
| mailfrom | string | 75% | 83% | 71% | 4% | 0% | 21% | smtp-moloch-bof: zeek='ibmuser@zpdt.example.com' ours='matt@zpdt.example.com,ibmuser@zpdt.example.com' |
| rcptto | set[string] | 75% | 79% | 75% | 0% | 0% | 17% |  |
| date | string | 46% | 52% | 46% | 0% | 0% | 17% |  |
| from | string | 67% | 83% | 21% | 46% | 0% | 29% | smtp-data-250: zeek='Xxxxx Xxxxxxxx <12345678@aol.com>' ours='12345678@aol.com'<br>smtp-data-521: zeek='"xxxxxxxxx 123456789" <1234567899@aol.com>' ours='1234567899@aol.com'<br>smtp-moloch-bof: zeek='matt@zpdt.example.com' ours='matt@zpdt.example.com,ibmuser@zpdt.example.com' |
| to | set[string] | 62% | 79% | 46% | 17% | 0% | 29% | smtp-data-521: zeek='"xxxxx-xxxx" <xxxxx-xxxx@xxxx.xxx.xx.jp>' ours='xxxxx-xxxx@xxxx.xxx.xx.jp'<br>smtp-originating: zeek='xxxxxxxx xxxxxxx <xxxxxxxx.xxxxxxx@xxxxxxxxxxxxxx.com>' ours='xxxxxxxx.xxxxxxx@xxxxxxxxxxxxxx.com'<br>smtp-subject-8859-b: zeek='Xxxxxxxxxxx <xxxxxxxx@gmail.com>' ours='xxxxxxxx@gmail.com' |
| cc | set[string] | 0% | 0% | 0% | 0% | 0% | 0% |  |
| reply_to | string | 4% | 3% | 4% | 0% | 0% | 0% |  |
| msg_id | string | 42% | 48% | 0% | 42% | 0% | 17% | arkime_synthetic: zeek='<abc123@example.com>' ours='abc123@example.com'<br>smtp-data-250: zeek='<xxxxxxxxxxxxxxxxxxxxxxxx.xxxxxxxxxxxxx@xxxxx.xxxxxxx.xxx>' ours='xxxxxxxxxxxxxxxxxxxxxxxx.xxxxxxxxxxxxx@xxxxx.xxxxxxx.xxx'<br>smtp-originating: zeek='<xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx@xxxxxxxxx.net>' ours='xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx@xxxxxxxxx.net' |
| in_reply_to | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| subject | string | 67% | 72% | 29% | 38% | 0% | 17% | smtp-subject-8859-b: zeek='=?iso-8859-1?B?dGVzdCDlIOkg7iD4IPw=?=' ours='test å é î ø ü'<br>smtp-subject-8859-multi: zeek='4spaces    =?iso-8859-1?B?5SDpIO4g+CA=?= =?iso-8859-1?Q?=FC?=     5spaces' ours='4spaces    å é î ø ü     5spaces'<br>smtp-subject-8859-q: zeek='=?iso-8859-1?Q?=E9_=EE_=F8_=FC____________?= =?iso-8859-1?Q?_________________________________________________________?= =?iso-8859-1?Q?_________________________________________________________?= =?iso-8859-1?Q?_____?= =?iso-8859-1?Q?_?=                                                     test' ours='é î ø ü                                                                                                                                                                                         test' |
| x_originating_ip | addr | 12% | 41% | 0% | 12% | 0% | 38% | smtp-originating: zeek='10.0.0.4' ours='10.0.0.3,10.0.0.2,10.0.0.4'<br>smtp-subject-8859-b: zeek='10.0.0.4' ours='169.254.4.195,10.180.121.108,10.0.0.4'<br>smtp-subject-gb2312-b: zeek='10.0.0.4' ours='10.180.121.151,169.254.4.195,10.0.0.4' |
| first_received | string | 38% | 0% | 0% | 0% | 38% | 0% | smtp-originating: zeek='from [10.000.000.002] (helo=[10.000.00.03])\\x09by xxxxxxxxxxxxxxxxxxxx.xxxxxxxxx.net with esmtpa (Exim 4.67)\\x09(envelope-from <xxxxxxxx@xxxxxxxxx.net>)\\x09id xxxxxxxxxxxxxxxx\\x09for xxxxxxxx.xxxxxxx@xxxxxxxxxxxxxx.com; Mon, 13 Jan 2014 10:01:13 -0500' ours=-<br>smtp-subject-8859-b: zeek='from xxxxxxxxxxxxxxxxx2.xxxxx.net ([169.254.4.195]) by xxxxxxxxxxxxxxxxx1.xxxxx.net ([10.180.121.108]) with mapi id 14.03.0123.003; Fri, 6 Dec 2013 14:31:10 -0500' ours=-<br>smtp-subject-8859-multi: zeek='(from xxxxx@localhost)\\x09by xxxxxxxxxxxxx.xxx.com (8.14.4/8.14.4/Submit) id rBCJivm4007995\\x09for xxxxxxxxx@xxxxxxx.com; Thu, 12 Dec 2013 14:44:57 -0500' ours=- |
| second_received | string | 21% | 0% | 0% | 0% | 21% | 0% | smtp-subject-8859-multi: zeek='from xxxxxxxxxxxxx.xxx.com (localhost.localdomain [127.0.0.1])\\x09by xxxxxxxxxxxxx.xxx.com (8.14.4/8.14.4) with ESMTP id rBCJivEK007998\\x09for <xxxxxxxxx@xxxxxxx.com>; Thu, 12 Dec 2013 14:44:57 -0500' ours=-<br>smtp-subject-8859-q: zeek='from xxxxxxxxxxxxx.xxx.com (localhost.localdomain [127.0.0.1])\\x09by xxxxxxxxxxxxx.xxx.com (8.14.4/8.14.4) with ESMTP id rBC38jeK000728\\x09for <xxxxxxxxx@xxxxxxx.com>; Wed, 11 Dec 2013 22:08:45 -0500' ours=-<br>smtp-subject-utf8-mixed: zeek='from xxxxxxxxxxxxx.xxx.com (localhost.localdomain [127.0.0.1])\\x09by xxxxxxxxxxxxx.xxx.com (8.14.4/8.14.4) with ESMTP id rBC3Cc5r001721\\x09for <xxxxxxxxx@xxxxxxx.com>; Wed, 11 Dec 2013 22:12:38 -0500' ours=- |
| last_reply | string | 96% | 48% | 50% | 8% | 38% | 0% | arkime_synthetic: zeek='220 mail.example.com ESMTP' ours=-<br>arkime_synthetic: zeek='354 End data with <CR><LF>.<CR><LF>' ours=-<br>smtp-decrypted: zeek='250 2.6.0 <964329.815327318-sendEmail@intra-mx2> [InternalId=3835405795330, Hostname=PCIAT-MX.shewa.intradef.gouv.fr] 17802 bytes in 0.102, 170,277 KB/sec Queued mail for delivery\\x0d\\x0a221 2.0.0 Service closing transmission channel\\x0d\\x0a' ours='250 2.6.0 <964329.815327318-sendEmail@intra-mx2> [InternalId=3835405795330, Hostname=PCIAT-MX.shewa.intradef.gouv.fr] 17802 bytes in 0.102, 170,277 KB/sec Queued mail for delivery' |
| path | vector[addr] | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='10.0.5.1,10.0.5.100' ours=-<br>arkime_synthetic: zeek='10.2.5.1,10.2.5.100' ours=-<br>smtp-data-250: zeek='64.12.168.40,10.0.0.1' ours=- |
| user_agent | string | 33% | 34% | 33% | 0% | 0% | 8% |  |
| tls | bool | 100% | 100% | 83% | 17% | 0% | 0% | smtp-decrypted: zeek='T' ours='F'<br>smtp-decrypted: zeek='T' ours='F'<br>smtp-decrypted: zeek='T' ours='F' |
| fuids | vector[string] | 67% | 0% | 33% | 67% | 0% | 0% | arkime_synthetic: zeek='FpL4Bh1lgHWfa5CMV1' ours='(empty)'<br>smtp-data-250: zeek='FCWk6I3ifhHdJM5ywk' ours='(empty)'<br>smtp-data-521: zeek='F0GuQC1nopRqKMqRj6' ours='(empty)' |

## snmp.log

pcaps with zeek rows: 3, matched rows: 8

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 100% | 0% | 0% | 0% |  |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | SNMPv2c_get_requests: zeek='Cl8ejm0efwu9vbhV3' ours='CtAcGNIv3H5CornMee'<br>SNMPv2c_get_requests: zeek='CDy0BC1PdHGzHZ8jbk' ours='CfXPEBPNS21CKeOi8v'<br>SNMPv2c_get_requests: zeek='CP6aDX1KTwBRaysz5' ours='Ce3hsDDFTvxBTt3pBc' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| duration | interval | 100% | 100% | 100% | 0% | 0% | 0% |  |
| version | string | 100% | 100% | 25% | 75% | 0% | 0% | SNMPv2c_get_requests: zeek='2c' ours='2'<br>SNMPv2c_get_requests: zeek='2c' ours='2'<br>SNMPv2c_get_requests: zeek='2c' ours='2' |
| community | string | 100% | 83% | 100% | 0% | 0% | 0% |  |
| get_requests | count | 100% | 0% | 0% | 0% | 100% | 0% | SNMPv2c_get_requests: zeek='1' ours=-<br>SNMPv2c_get_requests: zeek='1' ours=-<br>SNMPv2c_get_requests: zeek='1' ours=- |
| get_bulk_requests | count | 100% | 0% | 0% | 0% | 100% | 0% | SNMPv2c_get_requests: zeek='0' ours=-<br>SNMPv2c_get_requests: zeek='0' ours=-<br>SNMPv2c_get_requests: zeek='0' ours=- |
| get_responses | count | 100% | 0% | 0% | 0% | 100% | 0% | SNMPv2c_get_requests: zeek='1' ours=-<br>SNMPv2c_get_requests: zeek='1' ours=-<br>SNMPv2c_get_requests: zeek='1' ours=- |
| set_requests | count | 100% | 0% | 0% | 0% | 100% | 0% | SNMPv2c_get_requests: zeek='0' ours=-<br>SNMPv2c_get_requests: zeek='0' ours=-<br>SNMPv2c_get_requests: zeek='0' ours=- |
| display_string | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| up_since | time | 11% | 0% | 0% | 0% | 12% | 0% | SNMPv2c_get_requests: zeek='1221599145.195237' ours=- |

## socks.log

pcaps with zeek rows: 8, matched rows: 8

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='1737752617.000000' ours='1737752612.000000'<br>socks-http-example: zeek='1386004317.981218' ours='1386004317.979913'<br>socks-http-pass: zeek='1386090534.426708' ours='1386090534.425633' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='CKr2Tu1oY4e013iTIg' ours='CwEYY2C73OYBkRKIWU'<br>socks-http-example: zeek='C2dso91xGZpIlyr66b' ours='CTMA2IkyGu0ETDCbdB'<br>socks-http-pass: zeek='CaFNJb3hRwXsIykiJ' ours='CiTXcaFcTv7GfWAJt1' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| version | count | 100% | 88% | 100% | 0% | 0% | 0% |  |
| user | string | 25% | 25% | 25% | 0% | 0% | 0% |  |
| password | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| status | string | 100% | 88% | 100% | 0% | 0% | 0% |  |
| request.host | addr | 50% | 50% | 50% | 0% | 0% | 0% |  |
| request.name | string | 50% | 44% | 50% | 0% | 0% | 0% |  |
| request_p | port | 100% | 94% | 100% | 0% | 0% | 0% |  |
| bound.host | addr | 100% | 56% | 100% | 0% | 0% | 0% |  |
| bound.name | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| bound_p | port | 100% | 56% | 100% | 0% | 0% | 0% |  |

## ssh.log

pcaps with zeek rows: 6, matched rows: 9

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='1737752411.000000' ours='1737752408.000000'<br>arkime_synthetic: zeek='1737752578.000000' ours='1737752575.000000'<br>arkime_synthetic: zeek='1737752650.000000' ours='1737752647.000000' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='CofyXN2NoeVFXnGoxd' ours='CEQqia27ArV0wCngrp'<br>arkime_synthetic: zeek='CHUvQL1riLybzL96Yg' ours='CtI8EWUJXTeBELRo3k'<br>arkime_synthetic: zeek='C00U0j42f9eSELFi2h' ours='CzgltW8GnrWIs2AEig' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| version | count | 89% | 92% | 89% | 0% | 0% | 11% |  |
| auth_success | bool | 11% | 0% | 0% | 0% | 11% | 0% | tcpdump-geneve: zeek='T' ours=- |
| auth_attempts | count | 100% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='0' ours=-<br>arkime_synthetic: zeek='0' ours=-<br>arkime_synthetic: zeek='0' ours=- |
| direction | enum | 11% | 0% | 0% | 0% | 11% | 0% | gre-sample: zeek='INBOUND' ours=- |
| client | string | 89% | 77% | 89% | 0% | 0% | 0% |  |
| server | string | 100% | 92% | 100% | 0% | 0% | 0% |  |
| cipher_alg | string | 56% | 38% | 56% | 0% | 0% | 0% |  |
| mac_alg | string | 56% | 38% | 56% | 0% | 0% | 0% |  |
| compression_alg | string | 56% | 38% | 56% | 0% | 0% | 0% |  |
| kex_alg | string | 56% | 38% | 56% | 0% | 0% | 0% |  |
| host_key_alg | string | 56% | 38% | 56% | 0% | 0% | 0% |  |
| host_key_fingerprint | string | 22% | 31% | 0% | 22% | 0% | 22% | gre-sample: zeek='SHA256:uBYqruQMTBP9p69Sqq9z5wFvMBG/M9+u' ours='20:7c:e5:96:b0:4e:ce:a4:db:e4:aa:29:e8:90:98:07'<br>tcpdump-geneve: zeek='SHA256:/87DY7fgzSYLsYFspqpJ1oO++rK5v74T' ours='f4:c9:98:4e:74:48:52:74:38:f4:29:a7:52:1d:e6:65' |

## ssl.log

pcaps with zeek rows: 28, matched rows: 30

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 13% | 87% | 0% | 0% | arkime_synthetic: zeek='1737752573.000000' ours='1737752570.000000'<br>arkime_synthetic: zeek='1737752635.000000' ours='1737752632.000000'<br>arkime_synthetic: zeek='1737753023.000000' ours='1737753020.000000' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='Cx9I9H3QFwH2nOWWf3' ours='CSjpcxeryyHJMfp9Mc'<br>arkime_synthetic: zeek='CPxkAx1G7HQfAiQTo2' ours='Ce7IwPlS10bDNtq5A1'<br>arkime_synthetic: zeek='Cc08mu4PLu1AgP2DJ6' ours='CSaco5cU5T12Z0LjGV' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| version | string | 76% | 80% | 87% | 0% | 7% | 0% | cloudshark-dtls1: zeek='DTLSv10' ours=-<br>wireshark-dtls12: zeek='DTLSv12' ours=- |
| cipher | string | 76% | 80% | 87% | 0% | 7% | 0% | cloudshark-dtls1: zeek='TLS_RSA_WITH_AES_256_CBC_SHA' ours=-<br>wireshark-dtls12: zeek='unknown-49407' ours=- |
| curve | string | 35% | 35% | 43% | 0% | 0% | 0% |  |
| server_name | string | 51% | 43% | 40% | 0% | 3% | 0% | arkime_synthetic: zeek='test.example.com' ours=- |
| resumed | bool | 100% | 100% | 93% | 7% | 0% | 0% | curl-enabled-tls13: zeek='F' ours='T'<br>tls-split-client-hello: zeek='F' ours='T' |
| last_alert | string | 3% | 2% | 3% | 0% | 0% | 0% |  |
| next_protocol | string | 5% | 4% | 7% | 0% | 0% | 0% |  |
| established | bool | 100% | 100% | 67% | 33% | 0% | 0% | arkime_synthetic: zeek='F' ours='T'<br>arkime_synthetic: zeek='F' ours='T'<br>arkime_synthetic: zeek='F' ours='T' |
| ssl_history | string | 97% | 0% | 0% | 0% | 100% | 0% | arkime_synthetic: zeek='Cs' ours=-<br>arkime_synthetic: zeek='Csx' ours=-<br>arkime_synthetic: zeek='C' ours=- |
| cert_chain_fps | vector[string] | 57% | 55% | 67% | 3% | 0% | 0% | arkime_synthetic: zeek='17e82c76564443c3780c7a2f6c28f319d234f7e4fa6f1863f084e3e6447a12b1' ours='f4576088c7406a7a837953f77c361c4c54aa017bb9ad2097756e37ecac5742f6' |
| client_cert_chain_fps | vector[string] | 3% | 0% | 67% | 3% | 0% | 30% | ssl-selfsign: zeek='28ab6f96a4eb1bba1c72fb980c07fc83d5816a6629ee1ebd9613b9e62d4a6805' ours='(empty)' |
| sni_matches_cert | bool | 16% | 16% | 20% | 0% | 0% | 0% |  |

## tunnel.log

pcaps with zeek rows: 20, matched rows: 4

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 75% | 25% | 0% | 0% | gre-sample: zeek='1055289968.793044' ours='1055289987.055189' |
| uid | string | 77% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='CVRhFLyrsL5quI7Z9' ours='CU1fUJ6tlGn4xyHlMZ'<br>arkime_synthetic: zeek='C4LcZBxelYlTFaurk' ours='CYbWvcuB0vgCHG1wO9'<br>gre-erspan: zeek='CXW9Wb3ESSYHXMr5l2' ours='CTwcNiRQMAe218ZMYX' |
| id.orig_h | addr | 100% | 84% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 84% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| tunnel_type | enum | 100% | 100% | 100% | 0% | 0% | 0% |  |
| action | enum | 100% | 100% | 100% | 0% | 0% | 0% |  |

## websocket.log

pcaps with zeek rows: 1, matched rows: 1

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 0% | 100% | 0% | 0% | websocket: zeek='1475155930.826840' ours='1475155930.793984' |
| uid | string | 100% | 100% | 0% | 100% | 0% | 0% | websocket: zeek='CdYJSt3DD0ZBZ4zrCa' ours='C8RvodavVtFHZGE2Yl' |
| id.orig_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.orig_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_h | addr | 100% | 100% | 100% | 0% | 0% | 0% |  |
| id.resp_p | port | 100% | 100% | 100% | 0% | 0% | 0% |  |
| host | string | 100% | 100% | 0% | 100% | 0% | 0% | websocket: zeek='192.168.43.135:12345' ours='192.168.43.135:12345,192.168.43.135' |
| uri | string | 100% | 100% | 100% | 0% | 0% | 0% |  |
| user_agent | string | 100% | 33% | 100% | 0% | 0% | 0% |  |
| subprotocol | string | 0% | 0% | 0% | 0% | 0% | 0% |  |
| client_protocols | vector[string] | 0% | 0% | 0% | 0% | 0% | 0% |  |
| server_extensions | vector[string] | 0% | 0% | 0% | 0% | 0% | 0% |  |
| client_extensions | vector[string] | 100% | 0% | 0% | 0% | 100% | 0% | websocket: zeek='permessage-deflate; client_max_window_bits' ours=- |

## x509.log

pcaps with zeek rows: 20, matched rows: 40

| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |
|---|---|---|---|---|---|---|---|---|
| ts | time | 100% | 100% | 0% | 100% | 0% | 0% | arkime_synthetic: zeek='1737752636.000000' ours='1737752632.000000'<br>arkime_synthetic: zeek='1737753024.000000' ours='1737753020.000000'<br>badcurveball: zeek='1579293816.584116' ours='1579293816.559976' |
| fingerprint | string | 100% | 100% | 98% | 2% | 0% | 0% | arkime_synthetic: zeek='17e82c76564443c3780c7a2f6c28f319d234f7e4fa6f1863f084e3e6447a12b1' ours='f4576088c7406a7a837953f77c361c4c54aa017bb9ad2097756e37ecac5742f6' |
| certificate.version | count | 100% | 100% | 100% | 0% | 0% | 0% |  |
| certificate.serial | string | 100% | 100% | 100% | 0% | 0% | 0% |  |
| certificate.subject | string | 100% | 100% | 2% | 98% | 0% | 0% | arkime_synthetic: zeek='OU=IT Department,O=Example Inc,CN=certs.example.com' ours='CN=certs.example.com,O=Example Inc,OU=IT Department'<br>arkime_synthetic: zeek='CN=tztest.example.com,O=TZTest,C=US' ours='CN=tztest.example.com,O=TZTest'<br>badcurveball: zeek='CN=SANS ISC DShield Test,O=SANS Internet Storm Center,C=US' ours='CN=sans isc dshield test,O=SANS Internet Storm Center' |
| certificate.issuer | string | 100% | 100% | 2% | 98% | 0% | 0% | arkime_synthetic: zeek='O=Test Organization,CN=Test CA' ours='CN=test ca,O=Test Organization'<br>arkime_synthetic: zeek='CN=TestCA,O=TestCA,C=US' ours='CN=testca,O=TestCA'<br>badcurveball: zeek='CN=INFIGO,O=INFIGO IS,ST=Zagreb,C=HR' ours='CN=infigo,O=INFIGO IS' |
| certificate.not_valid_before | time | 100% | 100% | 98% | 2% | 0% | 0% | arkime_synthetic: zeek='0.000000' ours='1736922600.000000' |
| certificate.not_valid_after | time | 100% | 100% | 98% | 2% | 0% | 0% | arkime_synthetic: zeek='1768477920.000000' ours='1768507200.000000' |
| certificate.key_alg | string | 100% | 100% | 100% | 0% | 0% | 0% |  |
| certificate.sig_alg | string | 100% | 100% | 100% | 0% | 0% | 0% |  |
| certificate.key_type | string | 98% | 100% | 98% | 0% | 0% | 2% |  |
| certificate.key_length | count | 98% | 96% | 95% | 0% | 2% | 0% | badcurveball: zeek='384' ours=- |
| certificate.exponent | string | 89% | 91% | 88% | 0% | 0% | 0% |  |
| certificate.curve | string | 7% | 7% | 8% | 0% | 0% | 2% |  |
| san.dns | vector[string] | 34% | 35% | 38% | 0% | 0% | 62% |  |
| san.uri | vector[string] | 0% | 0% | 0% | 0% | 0% | 0% |  |
| san.email | vector[string] | 0% | 2% | 0% | 0% | 0% | 0% |  |
| san.ip | vector[addr] | 0% | 0% | 0% | 0% | 0% | 0% |  |
| basic_constraints.ca | bool | 91% | 100% | 92% | 0% | 0% | 8% |  |
| basic_constraints.path_len | count | 27% | 28% | 30% | 0% | 0% | 0% |  |
| host_cert | bool | 100% | 100% | 100% | 0% | 0% | 0% |  |
| client_cert | bool | 100% | 100% | 100% | 0% | 0% | 0% |  |
