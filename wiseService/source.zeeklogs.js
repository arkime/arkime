var wiseSource     = require('./wiseSource.js')
  , util           = require('util')
  ;

//////////////////////////////////////////////////////////////////////////////////
// Moloch WISE Data Source definition for Zeek logs.
//
// Part of Malcolm (https://github.com/idaholab/malcolm)
//
// Data may be populated with Malcolm's Zeek Logstash filters:
//   (particularly https://raw.githubusercontent.com/idaholab/Malcolm/master/logstash/pipeline-main/11_zeek_logs.conf)
//
// Copyright (c) 2019 Battelle Energy Alliance, LLC.  All rights reserved.
// see https://raw.githubusercontent.com/idaholab/Malcolm/master/License.txt
//////////////////////////////////////////////////////////////////////////////////
function ZeekLogs (api, section) {
  ZeekLogs.super_.call(this, api, section);

  // id information
  this.uidField = this.api.addField("field:zeek.uid;db:zeek.uid;kind:termfield;friendly:Zeek Connection ID;help:Zeek Connection ID");
  this.logTypeField = this.api.addField("field:zeek.logType;db:zeek.logType;kind:termfield;friendly:Zeek Log Type;help:Zeek Log Type");
  this.tsField = this.api.addField("field:zeek.ts;db:zeek.ts;kind:termfield;friendly:Timestamp;help:Zeek Timestamp");
  this.hostField = this.api.addField("field:host.name;db:host.name;kind:termfield;friendly:Zeek Node;help:Zeek Node");

  // basic connection information
  this.orig_hField = this.api.addField("field:zeek.orig_h;db:zeek.orig_h;kind:termfield;friendly:Originating host;help:Originating Host");
  this.orig_pField = this.api.addField("field:zeek.orig_p;db:zeek.orig_p;kind:integer;friendly:Originating port;help:Originating Port");
  this.orig_l2_addrField = this.api.addField("field:zeek.orig_l2_addr;db:zeek.orig_l2_addr;kind:termfield;friendly:Originating MAC;help:Originating MAC");
  this.orig_l2_ouiField = this.api.addField("field:zeek.orig_l2_oui;db:zeek.orig_l2_oui;kind:termfield;friendly:Originating OUI;help:Originating OUI");
  this.orig_hostnameField = this.api.addField("field:zeek.orig_hostname;db:zeek.orig_hostname;kind:termfield;friendly:Originating Host Name;help:Originating Host Name");
  this.orig_segmentField = this.api.addField("field:zeek.orig_segment;db:zeek.orig_segment;kind:termfield;friendly:Originating Network Segment;help:Originating Network Segment");
  this.source_ip_reverse_dnsField = this.api.addField("field:zeek.source_ip_reverse_dns;db:zeek.source_ip_reverse_dns;kind:termfield;friendly:Originating IP Reverse DNS;help:Originating IP Reverse DNS");
  this.source_geo_cityField = this.api.addField("field:zeek.source_geo.city_name;db:zeek.source_geo.city_name;kind:termfield;friendly:Originating GeoIP City;help:Originating GeoIP City");
  this.source_geo_countryField = this.api.addField("field:zeek.source_geo.country_name;db:zeek.source_geo.country_name;kind:termfield;friendly:Originating GeoIP Country;help:Originating GeoIP Country");
  this.resp_hField = this.api.addField("field:zeek.resp_h;db:zeek.resp_h;kind:termfield;friendly:Responding host;help:Responding Host");
  this.resp_pField = this.api.addField("field:zeek.resp_p;db:zeek.resp_p;kind:integer;friendly:Responding port;help:Responding Port");
  this.resp_l2_addrField = this.api.addField("field:zeek.resp_l2_addr;db:zeek.resp_l2_addr;kind:termfield;friendly:Responding MAC;help:Responding MAC");
  this.resp_l2_ouiField = this.api.addField("field:zeek.resp_l2_oui;db:zeek.resp_l2_oui;kind:termfield;friendly:Responding OUI;help:Responding OUI");
  this.resp_hostnameField = this.api.addField("field:zeek.resp_hostname;db:zeek.resp_hostname;kind:termfield;friendly:Responding Host Name;help:Responding Host Name");
  this.resp_segmentField = this.api.addField("field:zeek.resp_segment;db:zeek.resp_segment;kind:termfield;friendly:Responding Network Segment;help:Responding Network Segment");
  this.destination_ip_reverse_dnsField = this.api.addField("field:zeek.destination_ip_reverse_dns;db:zeek.destination_ip_reverse_dns;kind:termfield;friendly:Responding IP Reverse DNS;help:Responding IP Reverse DNS");
  this.destination_geo_cityField = this.api.addField("field:zeek.destination_geo.city_name;db:zeek.destination_geo.city_name;kind:termfield;friendly:Responding GeoIP City;help:Responding GeoIP City");
  this.destination_geo_countryField = this.api.addField("field:zeek.destination_geo.country_name;db:zeek.destination_geo.country_name;kind:termfield;friendly:Responding GeoIP Country;help:Responding GeoIP Country");
  this.protoField = this.api.addField("field:zeek.proto;db:zeek.proto;kind:lotermfield;friendly:Protocol;help:Protocol");
  this.serviceField = this.api.addField("field:zeek.service;db:zeek.service;kind:termfield;friendly:Service;help:Service");
  this.userField = this.api.addField("field:zeek.user;db:zeek.user;kind:termfield;friendly:User;help:User");

  // file information
  this.fuidField = this.api.addField("field:zeek.fuid;db:zeek.fuid;kind:termfield;friendly:File ID;help:File ID");
  this.filenameField = this.api.addField("field:zeek.filename;db:zeek.filename;kind:termfield;friendly:File Name;help:File Name");
  this.filetypeField = this.api.addField("field:zeek.filetype;db:zeek.filetype;kind:termfield;friendly:File Magic;help:File Magic");

  // conn.log
  this.conn_durationField = this.api.addField("field:zeek_conn.duration;db:zeek_conn.duration;kind:termfield;friendly:conn duration;help:conn duration");
  this.conn_orig_bytesField = this.api.addField("field:zeek_conn.orig_bytes;db:zeek_conn.orig_bytes;kind:integer;friendly:conn orig_bytes;help:conn orig_bytes");
  this.conn_resp_bytesField = this.api.addField("field:zeek_conn.resp_bytes;db:zeek_conn.resp_bytes;kind:integer;friendly:conn resp_bytes;help:conn resp_bytes");
  this.conn_conn_stateField = this.api.addField("field:zeek_conn.conn_state;db:zeek_conn.conn_state;kind:termfield;friendly:conn conn_state;help:conn conn_state");
  this.conn_conn_state_descriptionField = this.api.addField("field:zeek_conn.conn_state_description;db:zeek_conn.conn_state_description;kind:termfield;friendly:conn conn_state_description;help:conn conn_state_description");
  this.conn_local_origField = this.api.addField("field:zeek_conn.local_orig;db:zeek_conn.local_orig;kind:termfield;friendly:conn local_orig;help:conn local_orig");
  this.conn_local_respField = this.api.addField("field:zeek_conn.local_resp;db:zeek_conn.local_resp;kind:termfield;friendly:conn local_resp;help:conn local_resp");
  this.conn_missed_bytesField = this.api.addField("field:zeek_conn.missed_bytes;db:zeek_conn.missed_bytes;kind:integer;friendly:conn missed_bytes;help:conn missed_bytes");
  this.conn_historyField = this.api.addField("field:zeek_conn.history;db:zeek_conn.history;kind:termfield;friendly:conn history;help:conn history");
  this.conn_orig_pktsField = this.api.addField("field:zeek_conn.orig_pkts;db:zeek_conn.orig_pkts;kind:integer;friendly:conn orig_pkts;help:conn orig_pkts");
  this.conn_orig_ip_bytesField = this.api.addField("field:zeek_conn.orig_ip_bytes;db:zeek_conn.orig_ip_bytes;kind:integer;friendly:conn orig_ip_bytes;help:conn orig_ip_bytes");
  this.conn_resp_pktsField = this.api.addField("field:zeek_conn.resp_pkts;db:zeek_conn.resp_pkts;kind:integer;friendly:conn resp_pkts;help:conn resp_pkts");
  this.conn_resp_ip_bytesField = this.api.addField("field:zeek_conn.resp_ip_bytes;db:zeek_conn.resp_ip_bytes;kind:integer;friendly:conn resp_ip_bytes;help:conn resp_ip_bytes");
  this.conn_tunnel_parentsField = this.api.addField("field:zeek_conn.tunnel_parents;db:zeek_conn.tunnel_parents;kind:termfield;friendly:conn tunnel_parents;help:conn tunnel_parents");
  this.conn_vlanField = this.api.addField("field:zeek_conn.vlan;db:zeek_conn.vlan;kind:integer;friendly:conn vlan;help:conn outer VLAN");
  this.conn_inner_vlanField = this.api.addField("field:zeek_conn.inner_vlan;db:zeek_conn.inner_vlan;kind:integer;friendly:conn inner_vlan;help:conn inner VLAN");

  // dce_rpc.log
  this.dce_rpc_rttField = this.api.addField("field:zeek_dce_rpc.rtt;db:zeek_dce_rpc.rtt;kind:termfield;friendly:dce_rpc rtt;help:dce_rpc rtt");
  this.dce_rpc_named_pipeField = this.api.addField("field:zeek_dce_rpc.named_pipe;db:zeek_dce_rpc.named_pipe;kind:termfield;friendly:dce_rpc named_pipe;help:dce_rpc named_pipe");
  this.dce_rpc_endpointField = this.api.addField("field:zeek_dce_rpc.endpoint;db:zeek_dce_rpc.endpoint;kind:termfield;friendly:dce_rpc endpoint;help:dce_rpc endpoint");
  this.dce_rpc_operationField = this.api.addField("field:zeek_dce_rpc.operation;db:zeek_dce_rpc.operation;kind:termfield;friendly:dce_rpc operation;help:dce_rpc operation");

  // dhcp.log
  this.dhcp_macField = this.api.addField("field:zeek_dhcp.mac;db:zeek_dhcp.mac;kind:termfield;friendly:dhcp mac;help:dhcp mac");
  this.dhcp_assigned_ipField = this.api.addField("field:zeek_dhcp.assigned_ip;db:zeek_dhcp.assigned_ip;kind:termfield;friendly:dhcp assigned_ip;help:dhcp assigned_ip");
  this.dhcp_lease_timeField = this.api.addField("field:zeek_dhcp.lease_time;db:zeek_dhcp.lease_time;kind:termfield;friendly:dhcp lease_time;help:dhcp lease_time");
  this.dhcp_trans_idField = this.api.addField("field:zeek_dhcp.trans_id;db:zeek_dhcp.trans_id;kind:integer;friendly:dhcp trans_id;help:dhcp trans_id");

  // dnp3.log
  this.dnp3_fc_requestField = this.api.addField("field:zeek_dnp3.fc_request;db:zeek_dnp3.fc_request;kind:termfield;friendly:dnp3 fc_request;help:dnp3 fc_request");
  this.dnp3_fc_replyField = this.api.addField("field:zeek_dnp3.fc_reply;db:zeek_dnp3.fc_reply;kind:termfield;friendly:dnp3 fc_reply;help:dnp3 fc_reply");
  this.dnp3_iinField = this.api.addField("field:zeek_dnp3.iin;db:zeek_dnp3.iin;kind:integer;friendly:dnp3 iin;help:dnp3 iin");

  // dns.log
  this.dns_trans_idField = this.api.addField("field:zeek_dns.trans_id;db:zeek_dns.trans_id;kind:integer;friendly:dns trans_id;help:dns trans_id");
  this.dns_rttField = this.api.addField("field:zeek_dns.rtt;db:zeek_dns.rtt;kind:termfield;friendly:dns rtt;help:dns rtt");
  this.dns_queryField = this.api.addField("field:zeek_dns.query;db:zeek_dns.query;kind:termfield;friendly:dns query;help:dns query");
  this.dns_qclassField = this.api.addField("field:zeek_dns.qclass;db:zeek_dns.qclass;kind:integer;friendly:dns qclass;help:dns qclass");
  this.dns_qclass_nameField = this.api.addField("field:zeek_dns.qclass_name;db:zeek_dns.qclass_name;kind:termfield;friendly:dns qclass_name;help:dns qclass_name");
  this.dns_qtypeField = this.api.addField("field:zeek_dns.qtype;db:zeek_dns.qtype;kind:integer;friendly:dns qtype;help:dns qtype");
  this.dns_qtype_nameField = this.api.addField("field:zeek_dns.qtype_name;db:zeek_dns.qtype_name;kind:termfield;friendly:dns qtype_name;help:dns qtype_name");
  this.dns_rcodeField = this.api.addField("field:zeek_dns.rcode;db:zeek_dns.rcode;kind:integer;friendly:dns rcode;help:dns rcode");
  this.dns_rcode_nameField = this.api.addField("field:zeek_dns.rcode_name;db:zeek_dns.rcode_name;kind:termfield;friendly:dns rcode_name;help:dns rcode_name");
  this.dns_AAField = this.api.addField("field:zeek_dns.AA;db:zeek_dns.AA;kind:termfield;friendly:dns AA;help:dns AA");
  this.dns_TCField = this.api.addField("field:zeek_dns.TC;db:zeek_dns.TC;kind:termfield;friendly:dns TC;help:dns TC");
  this.dns_RDField = this.api.addField("field:zeek_dns.RD;db:zeek_dns.RD;kind:termfield;friendly:dns RD;help:dns RD");
  this.dns_RAField = this.api.addField("field:zeek_dns.RA;db:zeek_dns.RA;kind:termfield;friendly:dns RA;help:dns RA");
  this.dns_ZField = this.api.addField("field:zeek_dns.Z;db:zeek_dns.Z;kind:integer;friendly:dns Z;help:dns Z");
  this.dns_answersField = this.api.addField("field:zeek_dns.answers;db:zeek_dns.answers;kind:termfield;friendly:dns answers;help:dns answers");
  this.dns_TTLsField = this.api.addField("field:zeek_dns.TTLs;db:zeek_dns.TTLs;kind:termfield;friendly:dns TTLs;help:dns TTLs");
  this.dns_rejectedField = this.api.addField("field:zeek_dns.rejected;db:zeek_dns.rejected;kind:termfield;friendly:dns rejected;help:dns rejected");

  // dpd.log
  this.dpd_serviceField = this.api.addField("field:zeek_dpd.service;db:zeek_dpd.service;kind:termfield;friendly:dpd service;help:dpd service");
  this.dpd_failure_reasonField = this.api.addField("field:zeek_dpd.failure_reason;db:zeek_dpd.failure_reason;kind:termfield;friendly:dpd failure_reason;help:dpd failure_reason");

  // files.log
  this.files_fuidField = this.api.addField("field:zeek_files.fuid;db:zeek_files.fuid;kind:termfield;friendly:files fuid;help:files fuid");
  this.files_tx_hostsField = this.api.addField("field:zeek_files.tx_hosts;db:zeek_files.tx_hosts;kind:termfield;friendly:files tx_hosts;help:files tx_hosts");
  this.files_rx_hostsField = this.api.addField("field:zeek_files.rx_hosts;db:zeek_files.rx_hosts;kind:termfield;friendly:files rx_hosts;help:files rx_hosts");
  this.files_conn_uidsField = this.api.addField("field:zeek_files.conn_uids;db:zeek_files.conn_uids;kind:termfield;friendly:files conn_uids;help:files conn_uids");
  this.files_sourceField = this.api.addField("field:zeek_files.source;db:zeek_files.source;kind:termfield;friendly:files source;help:files source");
  this.files_depthField = this.api.addField("field:zeek_files.depth;db:zeek_files.depth;kind:integer;friendly:files depth;help:files depth");
  this.files_analyzersField = this.api.addField("field:zeek_files.analyzers;db:zeek_files.analyzers;kind:termfield;friendly:files analyzers;help:files analyzers");
  this.files_mime_typeField = this.api.addField("field:zeek_files.mime_type;db:zeek_files.mime_type;kind:termfield;friendly:files mime_type;help:files mime_type");
  this.files_filenameField = this.api.addField("field:zeek_files.filename;db:zeek_files.filename;kind:termfield;friendly:files filename;help:files filename");
  this.files_durationField = this.api.addField("field:zeek_files.duration;db:zeek_files.duration;kind:termfield;friendly:files duration;help:files duration");
  this.files_local_origField = this.api.addField("field:zeek_files.local_orig;db:zeek_files.local_orig;kind:termfield;friendly:files local_orig;help:files local_orig");
  this.files_is_origField = this.api.addField("field:zeek_files.is_orig;db:zeek_files.is_orig;kind:termfield;friendly:files is_orig;help:files is_orig");
  this.files_seen_bytesField = this.api.addField("field:zeek_files.seen_bytes;db:zeek_files.seen_bytes;kind:integer;friendly:files seen_bytes;help:files seen_bytes");
  this.files_total_bytesField = this.api.addField("field:zeek_files.total_bytes;db:zeek_files.total_bytes;kind:integer;friendly:files total_bytes;help:files total_bytes");
  this.files_missing_bytesField = this.api.addField("field:zeek_files.missing_bytes;db:zeek_files.missing_bytes;kind:integer;friendly:files missing_bytes;help:files missing_bytes");
  this.files_overflow_bytesField = this.api.addField("field:zeek_files.overflow_bytes;db:zeek_files.overflow_bytes;kind:integer;friendly:files overflow_bytes;help:files overflow_bytes");
  this.files_timedoutField = this.api.addField("field:zeek_files.timedout;db:zeek_files.timedout;kind:termfield;friendly:files timedout;help:files timedout");
  this.files_parent_fuidField = this.api.addField("field:zeek_files.parent_fuid;db:zeek_files.parent_fuid;kind:termfield;friendly:files parent_fuid;help:files parent_fuid");
  this.files_md5Field = this.api.addField("field:zeek_files.md5;db:zeek_files.md5;kind:termfield;friendly:files md5;help:files md5");
  this.files_sha1Field = this.api.addField("field:zeek_files.sha1;db:zeek_files.sha1;kind:termfield;friendly:files sha1;help:files sha1");
  this.files_sha256Field = this.api.addField("field:zeek_files.sha256;db:zeek_files.sha256;kind:termfield;friendly:files sha256;help:files sha256");
  this.files_extractedField = this.api.addField("field:zeek_files.extracted;db:zeek_files.extracted;kind:termfield;friendly:files extracted;help:files extracted");
  this.files_extracted_cutoffField = this.api.addField("field:zeek_files.extracted_cutoff;db:zeek_files.extracted_cutoff;kind:integer;friendly:files extracted_cutoff;help:files extracted_cutoff");
  this.files_extracted_sizeField = this.api.addField("field:zeek_files.extracted_size;db:zeek_files.extracted_size;kind:termfield;friendly:files extracted_size;help:files extracted_size");

  // ftp.log
  this.ftp_passwordField = this.api.addField("field:zeek_ftp.password;db:zeek_ftp.password;kind:termfield;friendly:ftp password;help:ftp password");
  this.ftp_commandField = this.api.addField("field:zeek_ftp.command;db:zeek_ftp.command;kind:termfield;friendly:ftp command;help:ftp command");
  this.ftp_argField = this.api.addField("field:zeek_ftp.arg;db:zeek_ftp.arg;kind:termfield;friendly:ftp arg;help:ftp arg");
  this.ftp_mime_typeField = this.api.addField("field:zeek_ftp.mime_type;db:zeek_ftp.mime_type;kind:termfield;friendly:ftp mime_type;help:ftp mime_type");
  this.ftp_file_sizeField = this.api.addField("field:zeek_ftp.file_size;db:zeek_ftp.file_size;kind:integer;friendly:ftp file_size;help:ftp file_size");
  this.ftp_reply_codeField = this.api.addField("field:zeek_ftp.reply_code;db:zeek_ftp.reply_code;kind:integer;friendly:ftp reply_code;help:ftp reply_code");
  this.ftp_reply_msgField = this.api.addField("field:zeek_ftp.reply_msg;db:zeek_ftp.reply_msg;kind:termfield;friendly:ftp reply_msg;help:ftp reply_msg");
  this.ftp_data_channel_passiveField = this.api.addField("field:zeek_ftp.data_channel_passive;db:zeek_ftp.data_channel_passive;kind:termfield;friendly:ftp data_channel_passive;help:ftp data_channel_passive");
  this.ftp_data_channel_orig_hField = this.api.addField("field:zeek_ftp.data_channel_orig_h;db:zeek_ftp.data_channel_orig_h;kind:termfield;friendly:ftp data_channel_orig_h;help:ftp data_channel_orig_h");
  this.ftp_data_channel_resp_hField = this.api.addField("field:zeek_ftp.data_channel_resp_h;db:zeek_ftp.data_channel_resp_h;kind:termfield;friendly:ftp data_channel_resp_h;help:ftp data_channel_resp_h");
  this.ftp_data_channel_resp_pField = this.api.addField("field:zeek_ftp.data_channel_resp_p;db:zeek_ftp.data_channel_resp_p;kind:integer;friendly:ftp data_channel_resp_p;help:ftp data_channel_resp_p");
  this.ftp_fuidField = this.api.addField("field:zeek_ftp.fuid;db:zeek_ftp.fuid;kind:termfield;friendly:ftp fuid;help:ftp fuid");

  // http.log
  this.http_trans_depthField = this.api.addField("field:zeek_http.trans_depth;db:zeek_http.trans_depth;kind:integer;friendly:http trans_depth;help:http trans_depth");
  this.http_methodField = this.api.addField("field:zeek_http.method;db:zeek_http.method;kind:termfield;friendly:http method;help:http method");
  this.http_hostField = this.api.addField("field:zeek_http.host;db:zeek_http.host;kind:termfield;friendly:http host;help:http host");
  this.http_uriField = this.api.addField("field:zeek_http.uri;db:zeek_http.uri;kind:termfield;friendly:http uri;help:http uri");
  this.http_referrerField = this.api.addField("field:zeek_http.referrer;db:zeek_http.referrer;kind:termfield;friendly:http referrer;help:http referrer");
  this.http_versionField = this.api.addField("field:zeek_http.version;db:zeek_http.version;kind:termfield;friendly:http version;help:http version");
  this.http_user_agentField = this.api.addField("field:zeek_http.user_agent;db:zeek_http.user_agent;kind:termfield;friendly:http user_agent;help:http user_agent");
  this.http_request_body_lenField = this.api.addField("field:zeek_http.request_body_len;db:zeek_http.request_body_len;kind:integer;friendly:http request_body_len;help:http request_body_len");
  this.http_response_body_lenField = this.api.addField("field:zeek_http.response_body_len;db:zeek_http.response_body_len;kind:integer;friendly:http response_body_len;help:http response_body_len");
  this.http_status_codeField = this.api.addField("field:zeek_http.status_code;db:zeek_http.status_code;kind:termfield;friendly:http status_code;help:http status_code");
  this.http_status_msgField = this.api.addField("field:zeek_http.status_msg;db:zeek_http.status_msg;kind:termfield;friendly:http status_msg;help:http status_msg");
  this.http_info_codeField = this.api.addField("field:zeek_http.info_code;db:zeek_http.info_code;kind:integer;friendly:http info_code;help:http info_code");
  this.http_info_msgField = this.api.addField("field:zeek_http.info_msg;db:zeek_http.info_msg;kind:termfield;friendly:http info_msg;help:http info_msg");
  this.http_tagsField = this.api.addField("field:zeek_http.tags;db:zeek_http.tags;kind:termfield;friendly:http tags;help:http tags");
  this.http_userField = this.api.addField("field:zeek_http.user;db:zeek_http.user;kind:termfield;friendly:http user;help:http user");
  this.http_passwordField = this.api.addField("field:zeek_http.password;db:zeek_http.password;kind:termfield;friendly:http password;help:http password");
  this.http_proxiedField = this.api.addField("field:zeek_http.proxied;db:zeek_http.proxied;kind:termfield;friendly:http proxied;help:http proxied");
  this.http_orig_fuidsField = this.api.addField("field:zeek_http.orig_fuids;db:zeek_http.orig_fuids;kind:termfield;friendly:http orig_fuids;help:http orig_fuids");
  this.http_orig_filenamesField = this.api.addField("field:zeek_http.orig_filenames;db:zeek_http.orig_filenames;kind:termfield;friendly:http orig_filenames;help:http orig_filenames");
  this.http_orig_mime_typesField = this.api.addField("field:zeek_http.orig_mime_types;db:zeek_http.orig_mime_types;kind:termfield;friendly:http orig_mime_types;help:http orig_mime_types");
  this.http_resp_fuidsField = this.api.addField("field:zeek_http.resp_fuids;db:zeek_http.resp_fuids;kind:termfield;friendly:http resp_fuids;help:http resp_fuids");
  this.http_resp_filenamesField = this.api.addField("field:zeek_http.resp_filenames;db:zeek_http.resp_filenames;kind:termfield;friendly:http resp_filenames;help:http resp_filenames");
  this.http_resp_mime_typesField = this.api.addField("field:zeek_http.resp_mime_types;db:zeek_http.resp_mime_types;kind:termfield;friendly:http resp_mime_types;help:http resp_mime_types");

  // intel.log
  this.intel_indicatorField = this.api.addField("field:zeek_intel.indicator;db:zeek_intel.indicator;kind:termfield;friendly:intel indicator;help:intel indicator");
  this.intel_indicator_typeField = this.api.addField("field:zeek_intel.indicator_type;db:zeek_intel.indicator_type;kind:termfield;friendly:intel indicator_type;help:intel indicator_type");
  this.intel_seen_whereField = this.api.addField("field:zeek_intel.seen_where;db:zeek_intel.seen_where;kind:termfield;friendly:intel seen_where;help:intel seen_where");
  this.intel_seen_nodeField = this.api.addField("field:zeek_intel.seen_node;db:zeek_intel.seen_node;kind:termfield;friendly:intel seen_node;help:intel seen_node");
  this.intel_matchedField = this.api.addField("field:zeek_intel.matched;db:zeek_intel.matched;kind:termfield;friendly:intel matched;help:intel matched");
  this.intel_sourcesField = this.api.addField("field:zeek_intel.sources;db:zeek_intel.sources;kind:termfield;friendly:intel sources;help:intel sources");
  this.intel_fuidField = this.api.addField("field:zeek_intel.fuid;db:zeek_intel.fuid;kind:termfield;friendly:intel fuid;help:intel fuid");
  this.intel_mimetypeField = this.api.addField("field:zeek_intel.mimetype;db:zeek_intel.mimetype;kind:termfield;friendly:intel mimetype;help:intel mimetype");
  this.intel_file_descriptionField = this.api.addField("field:zeek_intel.file_description;db:zeek_intel.file_description;kind:termfield;friendly:intel file_description;help:intel file_description");

  // irc.log
  this.irc_nickField = this.api.addField("field:zeek_irc.nick;db:zeek_irc.nick;kind:termfield;friendly:irc nick;help:irc nick");
  this.irc_commandField = this.api.addField("field:zeek_irc.command;db:zeek_irc.command;kind:termfield;friendly:irc command;help:irc command");
  this.irc_valueField = this.api.addField("field:zeek_irc.value;db:zeek_irc.value;kind:termfield;friendly:irc value;help:irc value");
  this.irc_addlField = this.api.addField("field:zeek_irc.addl;db:zeek_irc.addl;kind:termfield;friendly:irc addl;help:irc addl");
  this.irc_dcc_file_nameField = this.api.addField("field:zeek_irc.dcc_file_name;db:zeek_irc.dcc_file_name;kind:termfield;friendly:irc dcc_file_name;help:irc dcc_file_name");
  this.irc_dcc_file_sizeField = this.api.addField("field:zeek_irc.dcc_file_size;db:zeek_irc.dcc_file_size;kind:integer;friendly:irc dcc_file_size;help:irc dcc_file_size");
  this.irc_dcc_mime_typeField = this.api.addField("field:zeek_irc.dcc_mime_type;db:zeek_irc.dcc_mime_type;kind:termfield;friendly:irc dcc_mime_type;help:irc dcc_mime_type");
  this.irc_fuidField = this.api.addField("field:zeek_irc.fuid;db:zeek_irc.fuid;kind:termfield;friendly:irc fuid;help:irc fuid");

  // kerberos.log
  this.kerberos_cnameField = this.api.addField("field:zeek_kerberos.cname;db:zeek_kerberos.cname;kind:termfield;friendly:kerberos cname;help:kerberos cname");
  this.kerberos_snameField = this.api.addField("field:zeek_kerberos.sname;db:zeek_kerberos.sname;kind:termfield;friendly:kerberos sname;help:kerberos sname");
  this.kerberos_successField = this.api.addField("field:zeek_kerberos.success;db:zeek_kerberos.success;kind:termfield;friendly:kerberos success;help:kerberos success");
  this.kerberos_error_msgField = this.api.addField("field:zeek_kerberos.error_msg;db:zeek_kerberos.error_msg;kind:termfield;friendly:kerberos error_msg;help:kerberos error_msg");
  this.kerberos_fromField = this.api.addField("field:zeek_kerberos.from;db:zeek_kerberos.from;kind:termfield;friendly:kerberos from;help:kerberos from");
  this.kerberos_tillField = this.api.addField("field:zeek_kerberos.till;db:zeek_kerberos.till;kind:termfield;friendly:kerberos till;help:kerberos till");
  this.kerberos_cipherField = this.api.addField("field:zeek_kerberos.cipher;db:zeek_kerberos.cipher;kind:termfield;friendly:kerberos cipher;help:kerberos cipher");
  this.kerberos_forwardableField = this.api.addField("field:zeek_kerberos.forwardable;db:zeek_kerberos.forwardable;kind:termfield;friendly:kerberos forwardable;help:kerberos forwardable");
  this.kerberos_renewableField = this.api.addField("field:zeek_kerberos.renewable;db:zeek_kerberos.renewable;kind:termfield;friendly:kerberos renewable;help:kerberos renewable");
  this.kerberos_client_cert_subjectField = this.api.addField("field:zeek_kerberos.client_cert_subject;db:zeek_kerberos.client_cert_subject;kind:termfield;friendly:kerberos client_cert_subject;help:kerberos client_cert_subject");
  this.kerberos_client_cert_fuidField = this.api.addField("field:zeek_kerberos.client_cert_fuid;db:zeek_kerberos.client_cert_fuid;kind:termfield;friendly:kerberos client_cert_fuid;help:kerberos client_cert_fuid");
  this.kerberos_server_cert_subjectField = this.api.addField("field:zeek_kerberos.server_cert_subject;db:zeek_kerberos.server_cert_subject;kind:termfield;friendly:kerberos server_cert_subject;help:kerberos server_cert_subject");
  this.kerberos_server_cert_fuidField = this.api.addField("field:zeek_kerberos.server_cert_fuid;db:zeek_kerberos.server_cert_fuid;kind:termfield;friendly:kerberos server_cert_fuid;help:kerberos server_cert_fuid");

  // modbus.log
  this.modbus_funcField = this.api.addField("field:zeek_modbus.func;db:zeek_modbus.func;kind:termfield;friendly:modbus func;help:modbus func");
  this.modbus_exceptionField = this.api.addField("field:zeek_modbus.exception;db:zeek_modbus.exception;kind:termfield;friendly:modbus exception;help:modbus exception");

  // mysql.log
  this.mysql_cmdField = this.api.addField("field:zeek_mysql.cmd;db:zeek_mysql.cmd;kind:termfield;friendly:mysql cmd;help:mysql cmd");
  this.mysql_argField = this.api.addField("field:zeek_mysql.arg;db:zeek_mysql.arg;kind:termfield;friendly:mysql arg;help:mysql arg");
  this.mysql_successField = this.api.addField("field:zeek_mysql.success;db:zeek_mysql.success;kind:termfield;friendly:mysql success;help:mysql success");
  this.mysql_rowsField = this.api.addField("field:zeek_mysql.rows;db:zeek_mysql.rows;kind:integer;friendly:mysql rows;help:mysql rows");
  this.mysql_responseField = this.api.addField("field:zeek_mysql.response;db:zeek_mysql.response;kind:termfield;friendly:mysql response;help:mysql response");

  // notice.log
  this.notice_fuidField = this.api.addField("field:zeek_notice.fuid;db:zeek_notice.fuid;kind:termfield;friendly:notice fuid;help:notice fuid");
  this.notice_file_mime_typeField = this.api.addField("field:zeek_notice.file_mime_type;db:zeek_notice.file_mime_type;kind:termfield;friendly:notice file_mime_type;help:notice file_mime_type");
  this.notice_file_descField = this.api.addField("field:zeek_notice.file_desc;db:zeek_notice.file_desc;kind:termfield;friendly:notice file_desc;help:notice file_desc");
  this.notice_noteField = this.api.addField("field:zeek_notice.note;db:zeek_notice.note;kind:termfield;friendly:notice note;help:notice note");
  this.notice_msgField = this.api.addField("field:zeek_notice.msg;db:zeek_notice.msg;kind:termfield;friendly:notice msg;help:notice msg");
  this.notice_subField = this.api.addField("field:zeek_notice.sub;db:zeek_notice.sub;kind:termfield;friendly:notice sub;help:notice sub");
  this.notice_srcField = this.api.addField("field:zeek_notice.src;db:zeek_notice.src;kind:termfield;friendly:notice src;help:notice src");
  this.notice_dstField = this.api.addField("field:zeek_notice.dst;db:zeek_notice.dst;kind:termfield;friendly:notice dst;help:notice dst");
  this.notice_pField = this.api.addField("field:zeek_notice.p;db:zeek_notice.p;kind:integer;friendly:notice p;help:notice p");
  this.notice_nField = this.api.addField("field:zeek_notice.n;db:zeek_notice.n;kind:integer;friendly:notice n;help:notice n");
  this.notice_peer_descrField = this.api.addField("field:zeek_notice.peer_descr;db:zeek_notice.peer_descr;kind:termfield;friendly:notice peer_descr;help:notice peer_descr");
  this.notice_actionsField = this.api.addField("field:zeek_notice.actions;db:zeek_notice.actions;kind:termfield;friendly:notice actions;help:notice actions");
  this.notice_suppress_forField = this.api.addField("field:zeek_notice.suppress_for;db:zeek_notice.suppress_for;kind:termfield;friendly:notice suppress_for;help:notice suppress_for");
  this.notice_droppedField = this.api.addField("field:zeek_notice.dropped;db:zeek_notice.dropped;kind:termfield;friendly:notice dropped;help:notice dropped");
  this.notice_remote_location_country_codeField = this.api.addField("field:zeek_notice.remote_location_country_code;db:zeek_notice.remote_location_country_code;kind:termfield;friendly:notice remote_location_country_code;help:notice remote_location_country_code");
  this.notice_remote_location_regionField = this.api.addField("field:zeek_notice.remote_location_region;db:zeek_notice.remote_location_region;kind:termfield;friendly:notice remote_location_region;help:notice remote_location_region");
  this.notice_remote_location_cityField = this.api.addField("field:zeek_notice.remote_location_city;db:zeek_notice.remote_location_city;kind:termfield;friendly:notice remote_location_city;help:notice remote_location_city");
  this.notice_remote_location_latitudeField = this.api.addField("field:zeek_notice.remote_location_latitude;db:zeek_notice.remote_location_latitude;kind:termfield;friendly:notice remote_location_latitude;help:notice remote_location_latitude");
  this.notice_remote_location_longitudeField = this.api.addField("field:zeek_notice.remote_location_longitude;db:zeek_notice.remote_location_longitude;kind:termfield;friendly:notice remote_location_longitude;help:notice remote_location_longitude");

  // ntlm.log
  this.ntlm_hostField = this.api.addField("field:zeek_ntlm.host;db:zeek_ntlm.host;kind:termfield;friendly:ntlm host;help:ntlm host");
  this.ntlm_domainField = this.api.addField("field:zeek_ntlm.domain;db:zeek_ntlm.domain;kind:termfield;friendly:ntlm domain;help:ntlm domain");
  this.ntlm_successField = this.api.addField("field:zeek_ntlm.success;db:zeek_ntlm.success;kind:termfield;friendly:ntlm success;help:ntlm success");
  this.ntlm_statusField = this.api.addField("field:zeek_ntlm.status;db:zeek_ntlm.status;kind:termfield;friendly:ntlm status;help:ntlm status");

  // pe.log
  this.pe_fuidField = this.api.addField("field:zeek_pe.fuid;db:zeek_pe.fuid;kind:termfield;friendly:pe fuid;help:pe fuid");
  this.pe_machineField = this.api.addField("field:zeek_pe.machine;db:zeek_pe.machine;kind:termfield;friendly:pe machine;help:pe machine");
  this.pe_compile_tsField = this.api.addField("field:zeek_pe.compile_ts;db:zeek_pe.compile_ts;kind:termfield;friendly:pe compile_ts;help:pe compile_ts");
  this.pe_osField = this.api.addField("field:zeek_pe.os;db:zeek_pe.os;kind:termfield;friendly:pe os;help:pe os");
  this.pe_subsystemField = this.api.addField("field:zeek_pe.subsystem;db:zeek_pe.subsystem;kind:termfield;friendly:pe subsystem;help:pe subsystem");
  this.pe_is_exeField = this.api.addField("field:zeek_pe.is_exe;db:zeek_pe.is_exe;kind:termfield;friendly:pe is_exe;help:pe is_exe");
  this.pe_is_64bitField = this.api.addField("field:zeek_pe.is_64bit;db:zeek_pe.is_64bit;kind:termfield;friendly:pe is_64bit;help:pe is_64bit");
  this.pe_uses_aslrField = this.api.addField("field:zeek_pe.uses_aslr;db:zeek_pe.uses_aslr;kind:termfield;friendly:pe uses_aslr;help:pe uses_aslr");
  this.pe_uses_depField = this.api.addField("field:zeek_pe.uses_dep;db:zeek_pe.uses_dep;kind:termfield;friendly:pe uses_dep;help:pe uses_dep");
  this.pe_uses_code_integrityField = this.api.addField("field:zeek_pe.uses_code_integrity;db:zeek_pe.uses_code_integrity;kind:termfield;friendly:pe uses_code_integrity;help:pe uses_code_integrity");
  this.pe_uses_sehField = this.api.addField("field:zeek_pe.uses_seh;db:zeek_pe.uses_seh;kind:termfield;friendly:pe uses_seh;help:pe uses_seh");
  this.pe_has_import_tableField = this.api.addField("field:zeek_pe.has_import_table;db:zeek_pe.has_import_table;kind:termfield;friendly:pe has_import_table;help:pe has_import_table");
  this.pe_has_export_tableField = this.api.addField("field:zeek_pe.has_export_table;db:zeek_pe.has_export_table;kind:termfield;friendly:pe has_export_table;help:pe has_export_table");
  this.pe_has_cert_tableField = this.api.addField("field:zeek_pe.has_cert_table;db:zeek_pe.has_cert_table;kind:termfield;friendly:pe has_cert_table;help:pe has_cert_table");
  this.pe_has_debug_dataField = this.api.addField("field:zeek_pe.has_debug_data;db:zeek_pe.has_debug_data;kind:termfield;friendly:pe has_debug_data;help:pe has_debug_data");
  this.pe_section_namesField = this.api.addField("field:zeek_pe.section_names;db:zeek_pe.section_names;kind:termfield;friendly:pe section_names;help:pe section_names");

  // radius.log
  this.radius_macField = this.api.addField("field:zeek_radius.mac;db:zeek_radius.mac;kind:termfield;friendly:radius mac;help:radius mac");
  this.radius_framed_addrField = this.api.addField("field:zeek_radius.framed_addr;db:zeek_radius.framed_addr;kind:termfield;friendly:radius framed_addr;help:radius framed_addr");
  this.radius_remote_ipField = this.api.addField("field:zeek_radius.remote_ip;db:zeek_radius.remote_ip;kind:termfield;friendly:radius remote_ip;help:radius remote_ip");
  this.radius_connect_infoField = this.api.addField("field:zeek_radius.connect_info;db:zeek_radius.connect_info;kind:termfield;friendly:radius connect_info;help:radius connect_info");
  this.radius_reply_msgField = this.api.addField("field:zeek_radius.reply_msg;db:zeek_radius.reply_msg;kind:termfield;friendly:radius reply_msg;help:radius reply_msg");
  this.radius_resultField = this.api.addField("field:zeek_radius.result;db:zeek_radius.result;kind:termfield;friendly:radius result;help:radius result");
  this.radius_ttlField = this.api.addField("field:zeek_radius.ttl;db:zeek_radius.ttl;kind:termfield;friendly:radius ttl;help:radius ttl");

  // rdp.log
  this.rdp_cookieField = this.api.addField("field:zeek_rdp.cookie;db:zeek_rdp.cookie;kind:termfield;friendly:rdp cookie;help:rdp cookie");
  this.rdp_resultField = this.api.addField("field:zeek_rdp.result;db:zeek_rdp.result;kind:termfield;friendly:rdp result;help:rdp result");
  this.rdp_security_protocolField = this.api.addField("field:zeek_rdp.security_protocol;db:zeek_rdp.security_protocol;kind:termfield;friendly:rdp security_protocol;help:rdp security_protocol");
  this.rdp_keyboard_layoutField = this.api.addField("field:zeek_rdp.keyboard_layout;db:zeek_rdp.keyboard_layout;kind:termfield;friendly:rdp keyboard_layout;help:rdp keyboard_layout");
  this.rdp_client_buildField = this.api.addField("field:zeek_rdp.client_build;db:zeek_rdp.client_build;kind:termfield;friendly:rdp client_build;help:rdp client_build");
  this.rdp_client_nameField = this.api.addField("field:zeek_rdp.client_name;db:zeek_rdp.client_build;kind:termfield;friendly:rdp client_build;help:rdp client_build");
  this.rdp_client_dig_product_idField = this.api.addField("field:zeek_rdp.client_dig_product_id;db:zeek_rdp.client_dig_product_id;kind:termfield;friendly:rdp client_dig_product_id;help:rdp client_dig_product_id");
  this.rdp_desktop_widthField = this.api.addField("field:zeek_rdp.desktop_width;db:zeek_rdp.desktop_width;kind:integer;friendly:rdp desktop_width;help:rdp desktop_width");
  this.rdp_desktop_heightField = this.api.addField("field:zeek_rdp.desktop_height;db:zeek_rdp.desktop_height;kind:integer;friendly:rdp desktop_height;help:rdp desktop_height");
  this.rdp_requested_color_depthField = this.api.addField("field:zeek_rdp.requested_color_depth;db:zeek_rdp.requested_color_depth;kind:termfield;friendly:rdp requested_color_depth;help:rdp requested_color_depth");
  this.rdp_cert_typeField = this.api.addField("field:zeek_rdp.cert_type;db:zeek_rdp.cert_type;kind:termfield;friendly:rdp cert_type;help:rdp cert_type");
  this.rdp_cert_countField = this.api.addField("field:zeek_rdp.cert_count;db:zeek_rdp.cert_count;kind:integer;friendly:rdp cert_count;help:rdp cert_count");
  this.rdp_cert_permanentField = this.api.addField("field:zeek_rdp.cert_permanent;db:zeek_rdp.cert_permanent;kind:termfield;friendly:rdp cert_permanent;help:rdp cert_permanent");
  this.rdp_encryption_levelField = this.api.addField("field:zeek_rdp.encryption_level;db:zeek_rdp.encryption_level;kind:termfield;friendly:rdp encryption_level;help:rdp encryption_level");
  this.rdp_encryption_methodField = this.api.addField("field:zeek_rdp.encryption_method;db:zeek_rdp.encryption_method;kind:termfield;friendly:rdp encryption_method;help:rdp encryption_method");

  // rfb.log
  this.rfb_client_major_versionField = this.api.addField("field:zeek_rfb.client_major_version;db:zeek_rfb.client_major_version;kind:termfield;friendly:rfb client_major_version;help:rfb client_major_version");
  this.rfb_client_minor_versionField = this.api.addField("field:zeek_rfb.client_minor_version;db:zeek_rfb.client_minor_version;kind:termfield;friendly:rfb client_minor_version;help:rfb client_minor_version");
  this.rfb_server_major_versionField = this.api.addField("field:zeek_rfb.server_major_version;db:zeek_rfb.server_major_version;kind:termfield;friendly:rfb server_major_version;help:rfb server_major_version");
  this.rfb_server_minor_versionField = this.api.addField("field:zeek_rfb.server_minor_version;db:zeek_rfb.server_minor_version;kind:termfield;friendly:rfb server_minor_version;help:rfb server_minor_version");
  this.rfb_authentication_methodField = this.api.addField("field:zeek_rfb.authentication_method;db:zeek_rfb.authentication_method;kind:termfield;friendly:rfb authentication_method;help:rfb authentication_method");
  this.rfb_authField = this.api.addField("field:zeek_rfb.auth;db:zeek_rfb.auth;kind:termfield;friendly:rfb auth;help:rfb auth");
  this.rfb_share_flagField = this.api.addField("field:zeek_rfb.share_flag;db:zeek_rfb.share_flag;kind:termfield;friendly:rfb share_flag;help:rfb share_flag");
  this.rfb_desktop_nameField = this.api.addField("field:zeek_rfb.desktop_name;db:zeek_rfb.desktop_name;kind:termfield;friendly:rfb desktop_name;help:rfb desktop_name");
  this.rfb_widthField = this.api.addField("field:zeek_rfb.width;db:zeek_rfb.width;kind:integer;friendly:rfb width;help:rfb width");
  this.rfb_heightField = this.api.addField("field:zeek_rfb.height;db:zeek_rfb.height;kind:integer;friendly:rfb height;help:rfb height");

  // signatures.log
  this.signatures_noteField = this.api.addField("field:zeek_signatures.note;db:zeek_signatures.note;kind:termfield;friendly:signatures note;help:signatures note");
  this.signatures_signature_idField = this.api.addField("field:zeek_signatures.signature_id;db:zeek_signatures.signature_id;kind:termfield;friendly:signatures signature_id;help:signatures signature_id");
  this.signatures_event_messageField = this.api.addField("field:zeek_signatures.event_message;db:zeek_signatures.event_message;kind:termfield;friendly:signatures event_message;help:signatures event_message");
  this.signatures_sub_messageField = this.api.addField("field:zeek_signatures.sub_message;db:zeek_signatures.sub_message;kind:termfield;friendly:signatures sub_message;help:signatures sub_message");
  this.signatures_signature_countField = this.api.addField("field:zeek_signatures.signature_count;db:zeek_signatures.signature_count;kind:integer;friendly:signatures signature_count;help:signatures signature_count");
  this.signatures_host_countField = this.api.addField("field:zeek_signatures.host_count;db:zeek_signatures.host_count;kind:integer;friendly:signatures host_count;help:signatures host_count");
  this.signatures_engineField = this.api.addField("field:zeek_signatures.engine;db:zeek_signatures.engine;kind:termfield;friendly:signatures engine;help:signatures engine");
  this.signatures_hitsField = this.api.addField("field:zeek_signatures.hits;db:zeek_signatures.hits;kind:termfield;friendly:signatures hits;help:signatures hits");

  // sip.log
  this.sip_trans_depthField = this.api.addField("field:zeek_sip.trans_depth;db:zeek_sip.trans_depth;kind:integer;friendly:sip trans_depth;help:sip trans_depth");
  this.sip_methodField = this.api.addField("field:zeek_sip.method;db:zeek_sip.method;kind:termfield;friendly:sip method;help:sip method");
  this.sip_uriField = this.api.addField("field:zeek_sip.uri;db:zeek_sip.uri;kind:termfield;friendly:sip uri;help:sip uri");
  this.sip_dateField = this.api.addField("field:zeek_sip.date;db:zeek_sip.date;kind:termfield;friendly:sip date;help:sip date");
  this.sip_request_fromField = this.api.addField("field:zeek_sip.request_from;db:zeek_sip.request_from;kind:termfield;friendly:sip request_from;help:sip request_from");
  this.sip_request_toField = this.api.addField("field:zeek_sip.request_to;db:zeek_sip.request_to;kind:termfield;friendly:sip request_to;help:sip request_to");
  this.sip_response_fromField = this.api.addField("field:zeek_sip.response_from;db:zeek_sip.response_from;kind:termfield;friendly:sip response_from;help:sip response_from");
  this.sip_response_toField = this.api.addField("field:zeek_sip.response_to;db:zeek_sip.response_to;kind:termfield;friendly:sip response_to;help:sip response_to");
  this.sip_reply_toField = this.api.addField("field:zeek_sip.reply_to;db:zeek_sip.reply_to;kind:termfield;friendly:sip reply_to;help:sip reply_to");
  this.sip_call_idField = this.api.addField("field:zeek_sip.call_id;db:zeek_sip.call_id;kind:termfield;friendly:sip call_id;help:sip call_id");
  this.sip_seqField = this.api.addField("field:zeek_sip.seq;db:zeek_sip.seq;kind:termfield;friendly:sip seq;help:sip seq");
  this.sip_subjectField = this.api.addField("field:zeek_sip.subject;db:zeek_sip.subject;kind:termfield;friendly:sip subject;help:sip subject");
  this.sip_request_pathField = this.api.addField("field:zeek_sip.request_path;db:zeek_sip.request_path;kind:termfield;friendly:sip request_path;help:sip request_path");
  this.sip_response_pathField = this.api.addField("field:zeek_sip.response_path;db:zeek_sip.response_path;kind:termfield;friendly:sip response_path;help:sip response_path");
  this.sip_user_agentField = this.api.addField("field:zeek_sip.user_agent;db:zeek_sip.user_agent;kind:termfield;friendly:sip user_agent;help:sip user_agent");
  this.sip_status_codeField = this.api.addField("field:zeek_sip.status_code;db:zeek_sip.status_code;kind:termfield;friendly:sip status_code;help:sip status_code");
  this.sip_status_msgField = this.api.addField("field:zeek_sip.status_msg;db:zeek_sip.status_msg;kind:termfield;friendly:sip status_msg;help:sip status_msg");
  this.sip_warningField = this.api.addField("field:zeek_sip.warning;db:zeek_sip.warning;kind:termfield;friendly:sip warning;help:sip warning");
  this.sip_request_body_lenField = this.api.addField("field:zeek_sip.request_body_len;db:zeek_sip.request_body_len;kind:integer;friendly:sip request_body_len;help:sip request_body_len");
  this.sip_response_body_lenField = this.api.addField("field:zeek_sip.response_body_len;db:zeek_sip.response_body_len;kind:integer;friendly:sip response_body_len;help:sip response_body_len");
  this.sip_content_typeField = this.api.addField("field:zeek_sip.content_type;db:zeek_sip.content_type;kind:termfield;friendly:sip content_type;help:sip content_type");

  // smb_files.log
  this.smb_files_fuidField = this.api.addField("field:zeek_smb_files.fuid;db:zeek_smb_files.fuid;kind:termfield;friendly:smb_files fuid;help:smb_files fuid");
  this.smb_files_actionField = this.api.addField("field:zeek_smb_files.action;db:zeek_smb_files.action;kind:termfield;friendly:smb_files action;help:smb_files action");
  this.smb_files_pathField = this.api.addField("field:zeek_smb_files.path;db:zeek_smb_files.path;kind:termfield;friendly:smb_files path;help:smb_files path");
  this.smb_files_nameField = this.api.addField("field:zeek_smb_files.name;db:zeek_smb_files.name;kind:termfield;friendly:smb_files name;help:smb_files name");
  this.smb_files_sizeField = this.api.addField("field:zeek_smb_files.size;db:zeek_smb_files.size;kind:integer;friendly:smb_files size;help:smb_files size");
  this.smb_files_prev_nameField = this.api.addField("field:zeek_smb_files.prev_name;db:zeek_smb_files.prev_name;kind:termfield;friendly:smb_files prev_name;help:smb_files prev_name");
  this.smb_files_times_modifiedField = this.api.addField("field:zeek_smb_files.times_modified;db:zeek_smb_files.times_modified;kind:termfield;friendly:smb_files times_modified;help:smb_files times_modified");
  this.smb_files_times_accessedField = this.api.addField("field:zeek_smb_files.times_accessed;db:zeek_smb_files.times_accessed;kind:termfield;friendly:smb_files times_accessed;help:smb_files times_accessed");
  this.smb_files_times_createdField = this.api.addField("field:zeek_smb_files.times_created;db:zeek_smb_files.times_created;kind:termfield;friendly:smb_files times_created;help:smb_files times_created");
  this.smb_files_times_changedField = this.api.addField("field:zeek_smb_files.times_changed;db:zeek_smb_files.times_changed;kind:termfield;friendly:smb_files times_changed;help:smb_files times_changed");

  // smb_mapping.log
  this.smb_mapping_pathField = this.api.addField("field:zeek_smb_mapping.path;db:zeek_smb_mapping.path;kind:termfield;friendly:smb_mapping path;help:smb_mapping path");
  this.smb_mapping_resource_typeField = this.api.addField("field:zeek_smb_mapping.resource_type;db:zeek_smb_mapping.resource_type;kind:termfield;friendly:smb_mapping resource_type;help:smb_mapping resource_type");
  this.smb_mapping_native_file_systemField = this.api.addField("field:zeek_smb_mapping.native_file_system;db:zeek_smb_mapping.native_file_system;kind:termfield;friendly:smb_mapping native_file_system;help:smb_mapping native_file_system");
  this.smb_mapping_share_typeField = this.api.addField("field:zeek_smb_mapping.share_type;db:zeek_smb_mapping.share_type;kind:termfield;friendly:smb_mapping share_type;help:smb_mapping share_type");

  // smtp.log
  this.smtp_trans_depthField = this.api.addField("field:zeek_smtp.trans_depth;db:zeek_smtp.trans_depth;kind:integer;friendly:smtp trans_depth;help:smtp trans_depth");
  this.smtp_heloField = this.api.addField("field:zeek_smtp.helo;db:zeek_smtp.helo;kind:termfield;friendly:smtp helo;help:smtp helo");
  this.smtp_mailfromField = this.api.addField("field:zeek_smtp.mailfrom;db:zeek_smtp.mailfrom;kind:termfield;friendly:smtp mailfrom;help:smtp mailfrom");
  this.smtp_rcpttoField = this.api.addField("field:zeek_smtp.rcptto;db:zeek_smtp.rcptto;kind:termfield;friendly:smtp rcptto;help:smtp rcptto");
  this.smtp_dateField = this.api.addField("field:zeek_smtp.date;db:zeek_smtp.date;kind:termfield;friendly:smtp date;help:smtp date");
  this.smtp_fromField = this.api.addField("field:zeek_smtp.from;db:zeek_smtp.from;kind:termfield;friendly:smtp from;help:smtp from");
  this.smtp_toField = this.api.addField("field:zeek_smtp.to;db:zeek_smtp.to;kind:termfield;friendly:smtp to;help:smtp to");
  this.smtp_ccField = this.api.addField("field:zeek_smtp.cc;db:zeek_smtp.cc;kind:termfield;friendly:smtp cc;help:smtp cc");
  this.smtp_reply_toField = this.api.addField("field:zeek_smtp.reply_to;db:zeek_smtp.reply_to;kind:termfield;friendly:smtp reply_to;help:smtp reply_to");
  this.smtp_msg_idField = this.api.addField("field:zeek_smtp.msg_id;db:zeek_smtp.msg_id;kind:termfield;friendly:smtp msg_id;help:smtp msg_id");
  this.smtp_in_reply_toField = this.api.addField("field:zeek_smtp.in_reply_to;db:zeek_smtp.in_reply_to;kind:termfield;friendly:smtp in_reply_to;help:smtp in_reply_to");
  this.smtp_subjectField = this.api.addField("field:zeek_smtp.subject;db:zeek_smtp.subject;kind:termfield;friendly:smtp subject;help:smtp subject");
  this.smtp_x_originating_ipField = this.api.addField("field:zeek_smtp.x_originating_ip;db:zeek_smtp.x_originating_ip;kind:termfield;friendly:smtp x_originating_ip;help:smtp x_originating_ip");
  this.smtp_first_receivedField = this.api.addField("field:zeek_smtp.first_received;db:zeek_smtp.first_received;kind:termfield;friendly:smtp first_received;help:smtp first_received");
  this.smtp_second_receivedField = this.api.addField("field:zeek_smtp.second_received;db:zeek_smtp.second_received;kind:termfield;friendly:smtp second_received;help:smtp second_received");
  this.smtp_last_replyField = this.api.addField("field:zeek_smtp.last_reply;db:zeek_smtp.last_reply;kind:termfield;friendly:smtp last_reply;help:smtp last_reply");
  this.smtp_pathField = this.api.addField("field:zeek_smtp.path;db:zeek_smtp.path;kind:termfield;friendly:smtp path;help:smtp path");
  this.smtp_user_agentField = this.api.addField("field:zeek_smtp.user_agent;db:zeek_smtp.user_agent;kind:termfield;friendly:smtp user_agent;help:smtp user_agent");
  this.smtp_tlsField = this.api.addField("field:zeek_smtp.tls;db:zeek_smtp.tls;kind:termfield;friendly:smtp tls;help:smtp tls");
  this.smtp_fuidsField = this.api.addField("field:zeek_smtp.fuids;db:zeek_smtp.fuids;kind:termfield;friendly:smtp fuids;help:smtp fuids");
  this.smtp_is_webmailField = this.api.addField("field:zeek_smtp.is_webmail;db:zeek_smtp.is_webmail;kind:termfield;friendly:smtp is_webmail;help:smtp is_webmail");

  // snmp.log
  this.snmp_durationField = this.api.addField("field:zeek_snmp.duration;db:zeek_snmp.duration;kind:termfield;friendly:snmp duration;help:snmp duration");
  this.snmp_versionField = this.api.addField("field:zeek_snmp.version;db:zeek_snmp.version;kind:termfield;friendly:snmp version;help:snmp version");
  this.snmp_communityField = this.api.addField("field:zeek_snmp.community;db:zeek_snmp.community;kind:termfield;friendly:snmp community;help:snmp community");
  this.snmp_get_requestsField = this.api.addField("field:zeek_snmp.get_requests;db:zeek_snmp.get_requests;kind:termfield;friendly:snmp get_requests;help:snmp get_requests");
  this.snmp_get_bulk_requestsField = this.api.addField("field:zeek_snmp.get_bulk_requests;db:zeek_snmp.get_bulk_requests;kind:integer;friendly:snmp get_bulk_requests;help:snmp get_bulk_requests");
  this.snmp_get_responsesField = this.api.addField("field:zeek_snmp.get_responses;db:zeek_snmp.get_responses;kind:integer;friendly:snmp get_responses;help:snmp get_responses");
  this.snmp_set_requestsField = this.api.addField("field:zeek_snmp.set_requests;db:zeek_snmp.set_requests;kind:integer;friendly:snmp set_requests;help:snmp set_requests");
  this.snmp_display_stringField = this.api.addField("field:zeek_snmp.display_string;db:zeek_snmp.display_string;kind:termfield;friendly:snmp display_string;help:snmp display_string");
  this.snmp_up_sinceField = this.api.addField("field:zeek_snmp.up_since;db:zeek_snmp.up_since;kind:termfield;friendly:snmp up_since;help:snmp up_since");

  // socks.log
  this.socks_versionField = this.api.addField("field:zeek_socks.version;db:zeek_socks.version;kind:integer;friendly:socks version;help:socks version");
  this.socks_passwordField = this.api.addField("field:zeek_socks.password;db:zeek_socks.password;kind:termfield;friendly:socks password;help:socks password");
  this.socks_server_statusField = this.api.addField("field:zeek_socks.server_status;db:zeek_socks.server_status;kind:termfield;friendly:socks server_status;help:socks server_status");
  this.socks_request_hostField = this.api.addField("field:zeek_socks.request_host;db:zeek_socks.request_host;kind:termfield;friendly:socks request_host;help:socks request_host");
  this.socks_request_nameField = this.api.addField("field:zeek_socks.request_name;db:zeek_socks.request_name;kind:termfield;friendly:socks request_name;help:socks request_name");
  this.socks_request_portField = this.api.addField("field:zeek_socks.request_port;db:zeek_socks.request_port;kind:integer;friendly:socks request_port;help:socks request_port");
  this.socks_bound_hostField = this.api.addField("field:zeek_socks.bound_host;db:zeek_socks.bound_host;kind:termfield;friendly:socks bound_host;help:socks bound_host");
  this.socks_bound_nameField = this.api.addField("field:zeek_socks.bound_name;db:zeek_socks.bound_name;kind:termfield;friendly:socks bound_name;help:socks bound_name");
  this.socks_bound_portField = this.api.addField("field:zeek_socks.bound_port;db:zeek_socks.bound_port;kind:integer;friendly:socks bound_port;help:socks bound_port");

  // software.log
  this.software_software_typeField = this.api.addField("field:zeek_software.software_type;db:zeek_software.software_type;kind:termfield;friendly:software software_type;help:software software_type");
  this.software_nameField = this.api.addField("field:zeek_software.name;db:zeek_software.name;kind:termfield;friendly:software name;help:software name");
  this.software_version_majorField = this.api.addField("field:zeek_software.version_major;db:zeek_software.version_major;kind:integer;friendly:software version_major;help:software version_major");
  this.software_version_minorField = this.api.addField("field:zeek_software.version_minor;db:zeek_software.version_minor;kind:integer;friendly:software version_minor;help:software version_minor");
  this.software_version_minor2Field = this.api.addField("field:zeek_software.version_minor2;db:zeek_software.version_minor2;kind:integer;friendly:software version_minor2;help:software version_minor2");
  this.software_version_minor3Field = this.api.addField("field:zeek_software.version_minor3;db:zeek_software.version_minor3;kind:integer;friendly:software version_minor3;help:software version_minor3");
  this.software_version_addlField = this.api.addField("field:zeek_software.version_addl;db:zeek_software.version_addl;kind:termfield;friendly:software version_addl;help:software version_addl");
  this.software_unparsed_versionField = this.api.addField("field:zeek_software.unparsed_version;db:zeek_software.unparsed_version;kind:termfield;friendly:software unparsed_version;help:software unparsed_version");

  // ssh.log
  this.ssh_versionField = this.api.addField("field:zeek_ssh.version;db:zeek_ssh.version;kind:integer;friendly:ssh version;help:ssh version");
  this.ssh_auth_successField = this.api.addField("field:zeek_ssh.auth_success;db:zeek_ssh.auth_success;kind:termfield;friendly:ssh auth_success;help:ssh auth_success");
  this.ssh_auth_attemptsField = this.api.addField("field:zeek_ssh.auth_attempts;db:zeek_ssh.auth_attempts;kind:integer;friendly:ssh auth_attempts;help:ssh auth_attempts");
  this.ssh_directionField = this.api.addField("field:zeek_ssh.direction;db:zeek_ssh.direction;kind:termfield;friendly:ssh direction;help:ssh direction");
  this.ssh_clientField = this.api.addField("field:zeek_ssh.client;db:zeek_ssh.client;kind:termfield;friendly:ssh client;help:ssh client");
  this.ssh_serverField = this.api.addField("field:zeek_ssh.server;db:zeek_ssh.server;kind:termfield;friendly:ssh server;help:ssh server");
  this.ssh_cipher_algField = this.api.addField("field:zeek_ssh.cipher_alg;db:zeek_ssh.cipher_alg;kind:termfield;friendly:ssh cipher_alg;help:ssh cipher_alg");
  this.ssh_mac_algField = this.api.addField("field:zeek_ssh.mac_alg;db:zeek_ssh.mac_alg;kind:termfield;friendly:ssh mac_alg;help:ssh mac_alg");
  this.ssh_compression_algField = this.api.addField("field:zeek_ssh.compression_alg;db:zeek_ssh.compression_alg;kind:termfield;friendly:ssh compression_alg;help:ssh compression_alg");
  this.ssh_kex_algField = this.api.addField("field:zeek_ssh.kex_alg;db:zeek_ssh.kex_alg;kind:termfield;friendly:ssh kex_alg;help:ssh kex_alg");
  this.ssh_host_key_algField = this.api.addField("field:zeek_ssh.host_key_alg;db:zeek_ssh.host_key_alg;kind:termfield;friendly:ssh host_key_alg;help:ssh host_key_alg");
  this.ssh_host_keyField = this.api.addField("field:zeek_ssh.host_key;db:zeek_ssh.host_key;kind:termfield;friendly:ssh host_key;help:ssh host_key");
  this.ssh_remote_location_country_codeField = this.api.addField("field:zeek_ssh.remote_location_country_code;db:zeek_ssh.remote_location_country_code;kind:termfield;friendly:ssh remote_location_country_code;help:ssh remote_location_country_code");
  this.ssh_remote_location_regionField = this.api.addField("field:zeek_ssh.remote_location_region;db:zeek_ssh.remote_location_region;kind:termfield;friendly:ssh remote_location_region;help:ssh remote_location_region");
  this.ssh_remote_location_cityField = this.api.addField("field:zeek_ssh.remote_location_city;db:zeek_ssh.remote_location_city;kind:termfield;friendly:ssh remote_location_city;help:ssh remote_location_city");
  this.ssh_remote_location_latitudeField = this.api.addField("field:zeek_ssh.remote_location_latitude;db:zeek_ssh.remote_location_latitude;kind:termfield;friendly:ssh remote_location_latitude;help:ssh remote_location_latitude");
  this.ssh_remote_location_longitudeField = this.api.addField("field:zeek_ssh.remote_location_longitude;db:zeek_ssh.remote_location_longitude;kind:termfield;friendly:ssh remote_location_longitude;help:ssh remote_location_longitude");

  // ssl.log
  this.ssl_ssl_versionField = this.api.addField("field:zeek_ssl.ssl_version;db:zeek_ssl.ssl_version;kind:termfield;friendly:ssl ssl_version;help:ssl ssl_version");
  this.ssl_cipherField = this.api.addField("field:zeek_ssl.cipher;db:zeek_ssl.cipher;kind:termfield;friendly:ssl cipher;help:ssl cipher");
  this.ssl_curveField = this.api.addField("field:zeek_ssl.curve;db:zeek_ssl.curve;kind:termfield;friendly:ssl curve;help:ssl curve");
  this.ssl_server_nameField = this.api.addField("field:zeek_ssl.server_name;db:zeek_ssl.server_name;kind:termfield;friendly:ssl server_name;help:ssl server_name");
  this.ssl_resumedField = this.api.addField("field:zeek_ssl.resumed;db:zeek_ssl.resumed;kind:termfield;friendly:ssl resumed;help:ssl resumed");
  this.ssl_last_alertField = this.api.addField("field:zeek_ssl.last_alert;db:zeek_ssl.last_alert;kind:termfield;friendly:ssl last_alert;help:ssl last_alert");
  this.ssl_next_protocolField = this.api.addField("field:zeek_ssl.next_protocol;db:zeek_ssl.next_protocol;kind:termfield;friendly:ssl next_protocol;help:ssl next_protocol");
  this.ssl_establishedField = this.api.addField("field:zeek_ssl.established;db:zeek_ssl.established;kind:termfield;friendly:ssl established;help:ssl established");
  this.ssl_cert_chain_fuidsField = this.api.addField("field:zeek_ssl.cert_chain_fuids;db:zeek_ssl.cert_chain_fuids;kind:termfield;friendly:ssl cert_chain_fuids;help:ssl cert_chain_fuids");
  this.ssl_client_cert_chain_fuidsField = this.api.addField("field:zeek_ssl.client_cert_chain_fuids;db:zeek_ssl.client_cert_chain_fuids;kind:termfield;friendly:ssl client_cert_chain_fuids;help:ssl client_cert_chain_fuids");
  this.ssl_subject_fullField = this.api.addField("field:zeek_ssl.subject_full;db:zeek_ssl.subject_full;kind:termfield;friendly:ssl subject;help:ssl subject");
  this.ssl_subject_CNField = this.api.addField("field:zeek_ssl.subject.CN;db:zeek_ssl.subject.CN;kind:termfield;friendly:ssl subject common name;help:ssl subject common name");
  this.ssl_subject_CField = this.api.addField("field:zeek_ssl.subject.C;db:zeek_ssl.subject.C;kind:termfield;friendly:ssl subject country;help:ssl subject country");
  this.ssl_subject_OField = this.api.addField("field:zeek_ssl.subject.O;db:zeek_ssl.subject.O;kind:termfield;friendly:ssl subject organization;help:ssl subject organization");
  this.ssl_subject_OUField = this.api.addField("field:zeek_ssl.subject.OU;db:zeek_ssl.subject.OU;kind:termfield;friendly:ssl subject organization unit;help:ssl subject organization unit");
  this.ssl_subject_STField = this.api.addField("field:zeek_ssl.subject.ST;db:zeek_ssl.subject.ST;kind:termfield;friendly:ssl subject state;help:ssl subject state");
  this.ssl_subject_SNField = this.api.addField("field:zeek_ssl.subject.SN;db:zeek_ssl.subject.SN;kind:termfield;friendly:ssl subject surname;help:ssl subject surname");
  this.ssl_subject_LField = this.api.addField("field:zeek_ssl.subject.L;db:zeek_ssl.subject.L;kind:termfield;friendly:ssl subject locality;help:ssl subject locality");
  this.ssl_subject_GNField = this.api.addField("field:zeek_ssl.subject.GN;db:zeek_ssl.subject.GN;kind:termfield;friendly:ssl subject given name;help:ssl subject given name");
  this.ssl_subject_pseudonymField = this.api.addField("field:zeek_ssl.subject.pseudonym;db:zeek_ssl.subject.pseudonym;kind:termfield;friendly:ssl subject pseudonym;help:ssl subject pseudonym");
  this.ssl_subject_serialNumberField = this.api.addField("field:zeek_ssl.subject.serialNumber;db:zeek_ssl.subject.serialNumber;kind:termfield;friendly:ssl subject serial number;help:ssl subject serial number");
  this.ssl_subject_titleField = this.api.addField("field:zeek_ssl.subject.title;db:zeek_ssl.subject.title;kind:termfield;friendly:ssl subject title;help:ssl subject title");
  this.ssl_subject_initialsField = this.api.addField("field:zeek_ssl.subject.initials;db:zeek_ssl.subject.initials;kind:termfield;friendly:ssl subject initials;help:ssl subject initials");
  this.ssl_subject_emailAddressField = this.api.addField("field:zeek_ssl.subject.emailAddress;db:zeek_ssl.subject.emailAddress;kind:termfield;friendly:ssl subject email address;help:ssl subject email address");
  this.ssl_issuer_fullField = this.api.addField("field:zeek_ssl.issuer_full;db:zeek_ssl.issuer_full;kind:termfield;friendly:ssl issuer;help:ssl issuer");
  this.ssl_issuer_CNField = this.api.addField("field:zeek_ssl.issuer.CN;db:zeek_ssl.issuer.CN;kind:termfield;friendly:ssl issuer common name;help:ssl issuer common name");
  this.ssl_issuer_CField = this.api.addField("field:zeek_ssl.issuer.C;db:zeek_ssl.issuer.C;kind:termfield;friendly:ssl issuer country;help:ssl issuer country");
  this.ssl_issuer_OField = this.api.addField("field:zeek_ssl.issuer.O;db:zeek_ssl.issuer.O;kind:termfield;friendly:ssl issuer organization;help:ssl issuer organization");
  this.ssl_issuer_OUField = this.api.addField("field:zeek_ssl.issuer.OU;db:zeek_ssl.issuer.OU;kind:termfield;friendly:ssl issuer organization unit;help:ssl issuer organization unit");
  this.ssl_issuer_STField = this.api.addField("field:zeek_ssl.issuer.ST;db:zeek_ssl.issuer.ST;kind:termfield;friendly:ssl issuer state;help:ssl issuer state");
  this.ssl_issuer_SNField = this.api.addField("field:zeek_ssl.issuer.SN;db:zeek_ssl.issuer.SN;kind:termfield;friendly:ssl issuer surname;help:ssl issuer surname");
  this.ssl_issuer_LField = this.api.addField("field:zeek_ssl.issuer.L;db:zeek_ssl.issuer.L;kind:termfield;friendly:ssl issuer locality;help:ssl issuer locality");
  this.ssl_issuer_DCField = this.api.addField("field:zeek_ssl.issuer.DC;db:zeek_ssl.issuer.DC;kind:termfield;friendly:ssl issuer distinguished name;help:ssl issuer distinguished name");
  this.ssl_issuer_GNField = this.api.addField("field:zeek_ssl.issuer.GN;db:zeek_ssl.issuer.GN;kind:termfield;friendly:ssl issuer given name;help:ssl issuer given name");
  this.ssl_issuer_pseudonymField = this.api.addField("field:zeek_ssl.issuer.pseudonym;db:zeek_ssl.issuer.pseudonym;kind:termfield;friendly:ssl issuer pseudonym;help:ssl issuer pseudonym");
  this.ssl_issuer_serialNumberField = this.api.addField("field:zeek_ssl.issuer.serialNumber;db:zeek_ssl.issuer.serialNumber;kind:termfield;friendly:ssl issuer serial number;help:ssl issuer serial number");
  this.ssl_issuer_titleField = this.api.addField("field:zeek_ssl.issuer.title;db:zeek_ssl.issuer.title;kind:termfield;friendly:ssl issuer title;help:ssl issuer title");
  this.ssl_issuer_initialsField = this.api.addField("field:zeek_ssl.issuer.initials;db:zeek_ssl.issuer.initials;kind:termfield;friendly:ssl issuer initials;help:ssl issuer initials");
  this.ssl_issuer_emailAddressField = this.api.addField("field:zeek_ssl.issuer.emailAddress;db:zeek_ssl.issuer.emailAddress;kind:termfield;friendly:ssl issuer email address;help:ssl issuer email address");
  this.ssl_client_subject_fullField = this.api.addField("field:zeek_ssl.client_subject_full;db:zeek_ssl.client_subject_full;kind:termfield;friendly:ssl client subject;help:ssl client subject");
  this.ssl_client_subject_CNField = this.api.addField("field:zeek_ssl.client_subject.CN;db:zeek_ssl.client_subject.CN;kind:termfield;friendly:ssl client subject common name;help:ssl client subject common name");
  this.ssl_client_subject_CField = this.api.addField("field:zeek_ssl.client_subject.C;db:zeek_ssl.client_subject.C;kind:termfield;friendly:ssl client subject country;help:ssl client subject country");
  this.ssl_client_subject_OField = this.api.addField("field:zeek_ssl.client_subject.O;db:zeek_ssl.client_subject.O;kind:termfield;friendly:ssl client subject organization;help:ssl client subject organization");
  this.ssl_client_subject_OUField = this.api.addField("field:zeek_ssl.client_subject.OU;db:zeek_ssl.client_subject.OU;kind:termfield;friendly:ssl client subject organization unit;help:ssl client subject organization unit");
  this.ssl_client_subject_STField = this.api.addField("field:zeek_ssl.client_subject.ST;db:zeek_ssl.client_subject.ST;kind:termfield;friendly:ssl client subject state;help:ssl client subject state");
  this.ssl_client_subject_SNField = this.api.addField("field:zeek_ssl.client_subject.SN;db:zeek_ssl.client_subject.SN;kind:termfield;friendly:ssl client subject surname;help:ssl client subject surname");
  this.ssl_client_subject_LField = this.api.addField("field:zeek_ssl.client_subject.L;db:zeek_ssl.client_subject.L;kind:termfield;friendly:ssl client subject locality;help:ssl client subject locality");
  this.ssl_client_subject_GNField = this.api.addField("field:zeek_ssl.client_subject.GN;db:zeek_ssl.client_subject.GN;kind:termfield;friendly:ssl client subject given name;help:ssl client subject given name");
  this.ssl_client_subject_pseudonymField = this.api.addField("field:zeek_ssl.client_subject.pseudonym;db:zeek_ssl.client_subject.pseudonym;kind:termfield;friendly:ssl client subject pseudonym;help:ssl client subject pseudonym");
  this.ssl_client_subject_serialNumberField = this.api.addField("field:zeek_ssl.client_subject.serialNumber;db:zeek_ssl.client_subject.serialNumber;kind:termfield;friendly:ssl client subject serial number;help:ssl client subject serial number");
  this.ssl_client_subject_titleField = this.api.addField("field:zeek_ssl.client_subject.title;db:zeek_ssl.client_subject.title;kind:termfield;friendly:ssl client subject title;help:ssl client subject title");
  this.ssl_client_subject_initialsField = this.api.addField("field:zeek_ssl.client_subject.initials;db:zeek_ssl.client_subject.initials;kind:termfield;friendly:ssl client subject initials;help:ssl client subject initials");
  this.ssl_client_subject_emailAddressField = this.api.addField("field:zeek_ssl.client_subject.emailAddress;db:zeek_ssl.client_subject.emailAddress;kind:termfield;friendly:ssl client subject email address;help:ssl client subject email address");
  this.ssl_client_issuer_fullField = this.api.addField("field:zeek_ssl.client_issuer_full;db:zeek_ssl.client_issuer_full;kind:termfield;friendly:ssl client issuer;help:ssl client issuer");
  this.ssl_client_issuer_CNField = this.api.addField("field:zeek_ssl.client_issuer.CN;db:zeek_ssl.client_issuer.CN;kind:termfield;friendly:ssl client issuer common name;help:ssl client issuer common name");
  this.ssl_client_issuer_CField = this.api.addField("field:zeek_ssl.client_issuer.C;db:zeek_ssl.client_issuer.C;kind:termfield;friendly:ssl client issuer country;help:ssl client issuer country");
  this.ssl_client_issuer_OField = this.api.addField("field:zeek_ssl.client_issuer.O;db:zeek_ssl.client_issuer.O;kind:termfield;friendly:ssl client issuer organization;help:ssl client issuer organization");
  this.ssl_client_issuer_OUField = this.api.addField("field:zeek_ssl.client_issuer.OU;db:zeek_ssl.client_issuer.OU;kind:termfield;friendly:ssl client issuer organization unit;help:ssl client issuer organization unit");
  this.ssl_client_issuer_STField = this.api.addField("field:zeek_ssl.client_issuer.ST;db:zeek_ssl.client_issuer.ST;kind:termfield;friendly:ssl client issuer state;help:ssl client issuer state");
  this.ssl_client_issuer_SNField = this.api.addField("field:zeek_ssl.client_issuer.SN;db:zeek_ssl.client_issuer.SN;kind:termfield;friendly:ssl client issuer surname;help:ssl client issuer surname");
  this.ssl_client_issuer_LField = this.api.addField("field:zeek_ssl.client_issuer.L;db:zeek_ssl.client_issuer.L;kind:termfield;friendly:ssl client issuer locality;help:ssl client issuer locality");
  this.ssl_client_issuer_DCField = this.api.addField("field:zeek_ssl.client_issuer.DC;db:zeek_ssl.client_issuer.DC;kind:termfield;friendly:ssl client issuer distinguished name;help:ssl client issuer distinguished name");
  this.ssl_client_issuer_GNField = this.api.addField("field:zeek_ssl.client_issuer.GN;db:zeek_ssl.client_issuer.GN;kind:termfield;friendly:ssl client issuer given name;help:ssl client issuer given name");
  this.ssl_client_issuer_pseudonymField = this.api.addField("field:zeek_ssl.client_issuer.pseudonym;db:zeek_ssl.client_issuer.pseudonym;kind:termfield;friendly:ssl client issuer pseudonym;help:ssl client issuer pseudonym");
  this.ssl_client_issuer_serialNumberField = this.api.addField("field:zeek_ssl.client_issuer.serialNumber;db:zeek_ssl.client_issuer.serialNumber;kind:termfield;friendly:ssl client issuer serial number;help:ssl client issuer serial number");
  this.ssl_client_issuer_titleField = this.api.addField("field:zeek_ssl.client_issuer.title;db:zeek_ssl.client_issuer.title;kind:termfield;friendly:ssl client issuer title;help:ssl client issuer title");
  this.ssl_client_issuer_initialsField = this.api.addField("field:zeek_ssl.client_issuer.initials;db:zeek_ssl.client_issuer.initials;kind:termfield;friendly:ssl client issuer initials;help:ssl client issuer initials");
  this.ssl_client_issuer_emailAddressField = this.api.addField("field:zeek_ssl.client_issuer.emailAddress;db:zeek_ssl.client_issuer.emailAddress;kind:termfield;friendly:ssl client issuer email address;help:ssl client issuer email address");
  this.ssl_validation_statusField = this.api.addField("field:zeek_ssl.validation_status;db:zeek_ssl.validation_status;kind:termfield;friendly:ssl validation_status;help:ssl validation_status");
  this.ssl_ja3Field = this.api.addField("field:zeek_ssl.ja3;db:zeek_ssl.ja3;kind:termfield;friendly:JA3 Fingerprint;help:JA3 Fingerprint");
  this.ssl_ja3sField = this.api.addField("field:zeek_ssl.ja3s;db:zeek_ssl.ja3s;kind:termfield;friendly:JA3S Fingerprint;help:JA3S Fingerprint");
  this.ssl_ja3_descField = this.api.addField("field:zeek_ssl.ja3_desc;db:zeek_ssl.ja3_desc;kind:termfield;friendly:JA3 Fingerprint Lookup;help:JA3 Fingerprint Lookup");
  this.ssl_ja3s_descField = this.api.addField("field:zeek_ssl.ja3s_desc;db:zeek_ssl.ja3s_desc;kind:termfield;friendly:JA3S Fingerprint Lookup;help:JA3S Fingerprint Lookup");

  // syslog.log
  this.syslog_facilityField = this.api.addField("field:zeek_syslog.facility;db:zeek_syslog.facility;kind:termfield;friendly:syslog facility;help:syslog facility");
  this.syslog_severityField = this.api.addField("field:zeek_syslog.severity;db:zeek_syslog.severity;kind:termfield;friendly:syslog severity;help:syslog severity");
  this.syslog_messageField = this.api.addField("field:zeek_syslog.message;db:zeek_syslog.message;kind:termfield;friendly:syslog message;help:syslog message");

  // tunnel.log
  this.tunnel_tunnel_typeField = this.api.addField("field:zeek_tunnel.tunnel_type;db:zeek_tunnel.tunnel_type;kind:termfield;friendly:tunnel tunnel_type;help:tunnel tunnel_type");
  this.tunnel_actionField = this.api.addField("field:zeek_tunnel.action;db:zeek_tunnel.action;kind:termfield;friendly:tunnel action;help:tunnel action");

  // weird.log
  this.weird_nameField = this.api.addField("field:zeek_weird.name;db:zeek_weird.name;kind:termfield;friendly:weird name;help:weird name");
  this.weird_addlField = this.api.addField("field:zeek_weird.addl;db:zeek_weird.addl;kind:termfield;friendly:weird addl;help:weird addl");
  this.weird_noticeField = this.api.addField("field:zeek_weird.notice;db:zeek_weird.notice;kind:termfield;friendly:weird notice;help:weird notice");
  this.weird_peerField = this.api.addField("field:zeek_weird.peer;db:zeek_weird.peer;kind:termfield;friendly:weird peer;help:weird peer");

  // x509.log
  this.x509_fuidField = this.api.addField("field:zeek_x509.fuid;db:zeek_x509.fuid;kind:termfield;friendly:x509 fuid;help:x509 fuid");
  this.x509_certificate_versionField = this.api.addField("field:zeek_x509.certificate_version;db:zeek_x509.certificate_version;kind:integer;friendly:x509 certificate_version;help:x509 certificate_version");
  this.x509_certificate_serialField = this.api.addField("field:zeek_x509.certificate_serial;db:zeek_x509.certificate_serial;kind:termfield;friendly:x509 certificate_serial;help:x509 certificate_serial");
  this.x509_certificate_subject_fullField = this.api.addField("field:zeek_x509.certificate_subject_full;db:zeek_x509.certificate_subject_full;kind:termfield;friendly:x509 certificate subject;help:x509 certificate subject");
  this.x509_certificate_subject_CNField = this.api.addField("field:zeek_x509.certificate_subject.CN;db:zeek_x509.certificate_subject.CN;kind:termfield;friendly:x509 certificate subject common name;help:x509 certificate subject common name");
  this.x509_certificate_subject_CField = this.api.addField("field:zeek_x509.certificate_subject.C;db:zeek_x509.certificate_subject.C;kind:termfield;friendly:x509 certificate subject country;help:x509 certificate subject country");
  this.x509_certificate_subject_OField = this.api.addField("field:zeek_x509.certificate_subject.O;db:zeek_x509.certificate_subject.O;kind:termfield;friendly:x509 certificate subject organization;help:x509 certificate subject organization");
  this.x509_certificate_subject_OUField = this.api.addField("field:zeek_x509.certificate_subject.OU;db:zeek_x509.certificate_subject.OU;kind:termfield;friendly:x509 certificate subject organization unit;help:x509 certificate subject organization unit");
  this.x509_certificate_subject_STField = this.api.addField("field:zeek_x509.certificate_subject.ST;db:zeek_x509.certificate_subject.ST;kind:termfield;friendly:x509 certificate subject state;help:x509 certificate subject state");
  this.x509_certificate_subject_SNField = this.api.addField("field:zeek_x509.certificate_subject.SN;db:zeek_x509.certificate_subject.SN;kind:termfield;friendly:x509 certificate subject surname;help:x509 certificate subject surname");
  this.x509_certificate_subject_LField = this.api.addField("field:zeek_x509.certificate_subject.L;db:zeek_x509.certificate_subject.L;kind:termfield;friendly:x509 certificate subject locality;help:x509 certificate subject locality");
  this.x509_certificate_subject_DCField = this.api.addField("field:zeek_x509.certificate_subject.DC;db:zeek_x509.certificate_subject.DC;kind:termfield;friendly:x509 certificate subject distinguished name;help:x509 certificate subject distinguished name");
  this.x509_certificate_subject_GNField = this.api.addField("field:zeek_x509.certificate_subject.GN;db:zeek_x509.certificate_subject.GN;kind:termfield;friendly:x509 certificate subject given name;help:x509 certificate subject given name");
  this.x509_certificate_subject_pseudonymField = this.api.addField("field:zeek_x509.certificate_subject.pseudonym;db:zeek_x509.certificate_subject.pseudonym;kind:termfield;friendly:x509 certificate subject pseudonym;help:x509 certificate subject pseudonym");
  this.x509_certificate_subject_serialNumberField = this.api.addField("field:zeek_x509.certificate_subject.serialNumber;db:zeek_x509.certificate_subject.serialNumber;kind:termfield;friendly:x509 certificate subject serial number;help:x509 certificate subject serial number");
  this.x509_certificate_subject_titleField = this.api.addField("field:zeek_x509.certificate_subject.title;db:zeek_x509.certificate_subject.title;kind:termfield;friendly:x509 certificate subject title;help:x509 certificate subject title");
  this.x509_certificate_subject_initialsField = this.api.addField("field:zeek_x509.certificate_subject.initials;db:zeek_x509.certificate_subject.initials;kind:termfield;friendly:x509 certificate subject initials;help:x509 certificate subject initials");
  this.x509_certificate_subject_emailAddressField = this.api.addField("field:zeek_x509.certificate_subject.emailAddress;db:zeek_x509.certificate_subject.emailAddress;kind:termfield;friendly:x509 certificate subject email address;help:x509 certificate subject email address");
  this.x509_certificate_issuer_fullField = this.api.addField("field:zeek_x509.certificate_issuer_full;db:zeek_x509.certificate_issuer_full;kind:termfield;friendly:x509 certificate issuer;help:x509 certificate issuer");
  this.x509_certificate_issuer_CNField = this.api.addField("field:zeek_x509.certificate_issuer.CN;db:zeek_x509.certificate_issuer.CN;kind:termfield;friendly:x509 certificate issuer common name;help:x509 certificate issuer common name");
  this.x509_certificate_issuer_CField = this.api.addField("field:zeek_x509.certificate_issuer.C;db:zeek_x509.certificate_issuer.C;kind:termfield;friendly:x509 certificate issuer country;help:x509 certificate issuer country");
  this.x509_certificate_issuer_OField = this.api.addField("field:zeek_x509.certificate_issuer.O;db:zeek_x509.certificate_issuer.O;kind:termfield;friendly:x509 certificate issuer organization;help:x509 certificate issuer organization");
  this.x509_certificate_issuer_OUField = this.api.addField("field:zeek_x509.certificate_issuer.OU;db:zeek_x509.certificate_issuer.OU;kind:termfield;friendly:x509 certificate issuer organization unit;help:x509 certificate issuer organization unit");
  this.x509_certificate_issuer_STField = this.api.addField("field:zeek_x509.certificate_issuer.ST;db:zeek_x509.certificate_issuer.ST;kind:termfield;friendly:x509 certificate issuer state;help:x509 certificate issuer state");
  this.x509_certificate_issuer_SNField = this.api.addField("field:zeek_x509.certificate_issuer.SN;db:zeek_x509.certificate_issuer.SN;kind:termfield;friendly:x509 certificate issuer surname;help:x509 certificate issuer surname");
  this.x509_certificate_issuer_LField = this.api.addField("field:zeek_x509.certificate_issuer.L;db:zeek_x509.certificate_issuer.L;kind:termfield;friendly:x509 certificate issuer locality;help:x509 certificate issuer locality");
  this.x509_certificate_issuer_GNField = this.api.addField("field:zeek_x509.certificate_issuer.GN;db:zeek_x509.certificate_issuer.GN;kind:termfield;friendly:x509 certificate issuer given name;help:x509 certificate issuer given name");
  this.x509_certificate_issuer_pseudonymField = this.api.addField("field:zeek_x509.certificate_issuer.pseudonym;db:zeek_x509.certificate_issuer.pseudonym;kind:termfield;friendly:x509 certificate issuer pseudonym;help:x509 certificate issuer pseudonym");
  this.x509_certificate_issuer_serialNumberField = this.api.addField("field:zeek_x509.certificate_issuer.serialNumber;db:zeek_x509.certificate_issuer.serialNumber;kind:termfield;friendly:x509 certificate issuer serial number;help:x509 certificate issuer serial number");
  this.x509_certificate_issuer_titleField = this.api.addField("field:zeek_x509.certificate_issuer.title;db:zeek_x509.certificate_issuer.title;kind:termfield;friendly:x509 certificate issuer title;help:x509 certificate issuer title");
  this.x509_certificate_issuer_initialsField = this.api.addField("field:zeek_x509.certificate_issuer.initials;db:zeek_x509.certificate_issuer.initials;kind:termfield;friendly:x509 certificate issuer initials;help:x509 certificate issuer initials");
  this.x509_certificate_issuer_emailAddressField = this.api.addField("field:zeek_x509.certificate_issuer.emailAddress;db:zeek_x509.certificate_issuer.emailAddress;kind:termfield;friendly:x509 certificate issuer email address;help:x509 certificate issuer email address");
  this.x509_certificate_not_valid_beforeField = this.api.addField("field:zeek_x509.certificate_not_valid_before;db:zeek_x509.certificate_not_valid_before;kind:termfield;friendly:x509 certificate_not_valid_before;help:x509 certificate_not_valid_before");
  this.x509_certificate_not_valid_afterField = this.api.addField("field:zeek_x509.certificate_not_valid_after;db:zeek_x509.certificate_not_valid_after;kind:termfield;friendly:x509 certificate_not_valid_after;help:x509 certificate_not_valid_after");
  this.x509_certificate_key_algField = this.api.addField("field:zeek_x509.certificate_key_alg;db:zeek_x509.certificate_key_alg;kind:termfield;friendly:x509 certificate_key_alg;help:x509 certificate_key_alg");
  this.x509_certificate_sig_algField = this.api.addField("field:zeek_x509.certificate_sig_alg;db:zeek_x509.certificate_sig_alg;kind:termfield;friendly:x509 certificate_sig_alg;help:x509 certificate_sig_alg");
  this.x509_certificate_key_typeField = this.api.addField("field:zeek_x509.certificate_key_type;db:zeek_x509.certificate_key_type;kind:termfield;friendly:x509 certificate_key_type;help:x509 certificate_key_type");
  this.x509_certificate_key_lengthField = this.api.addField("field:zeek_x509.certificate_key_length;db:zeek_x509.certificate_key_length;kind:integer;friendly:x509 certificate_key_length;help:x509 certificate_key_length");
  this.x509_certificate_exponentField = this.api.addField("field:zeek_x509.certificate_exponent;db:zeek_x509.certificate_exponent;kind:termfield;friendly:x509 certificate_exponent;help:x509 certificate_exponent");
  this.x509_certificate_curveField = this.api.addField("field:zeek_x509.certificate_curve;db:zeek_x509.certificate_curve;kind:termfield;friendly:x509 certificate_curve;help:x509 certificate_curve");
  this.x509_san_dnsField = this.api.addField("field:zeek_x509.san_dns;db:zeek_x509.san_dns;kind:termfield;friendly:x509 san_dns;help:x509 san_dns");
  this.x509_san_uriField = this.api.addField("field:zeek_x509.san_uri;db:zeek_x509.san_uri;kind:termfield;friendly:x509 san_uri;help:x509 san_uri");
  this.x509_san_emailField = this.api.addField("field:zeek_x509.san_email;db:zeek_x509.san_email;kind:termfield;friendly:x509 san_email;help:x509 san_email");
  this.x509_san_ipField = this.api.addField("field:zeek_x509.san_ip;db:zeek_x509.san_ip;kind:termfield;friendly:x509 san_ip;help:x509 san_ip");
  this.x509_basic_constraints_caField = this.api.addField("field:zeek_x509.basic_constraints_ca;db:zeek_x509.basic_constraints_ca;kind:termfield;friendly:x509 basic_constraints_ca;help:x509 basic_constraints_ca");
  this.x509_basic_constraints_path_lenField = this.api.addField("field:zeek_x509.basic_constraints_path_len;db:zeek_x509.basic_constraints_path_len;kind:integer;friendly:x509 basic_constraints_path_len;help:x509 basic_constraints_path_len");

  // todo: look at expressions for things that have parents (tunnelling, parent files, etc.)
  // todo: look at IP types and use ipPrint?

  this.api.addView("zeek",
    "if (session.zeek)\n" +

    // id information
    "  div.sessionDetailMeta.bold zeek\n" +
    "  dl.sessionDetailMeta(suffix=\"IDs\")\n" +
    "    +arrayList(session.zeek, 'uid', 'Zeek Connection ID', 'zeek.uid')\n" +
    "    +arrayList(session.zeek, 'logType', 'Zeek Log Type', 'zeek.logType')\n" +
    "    +arrayList(session.host, 'name', 'Zeek Node', 'host.name')\n" +

    // basic connection information
    "  if (session.zeek.orig_h || session.zeek.orig_p || session.zeek.orig_l2_addr || session.zeek.resp_h || " +
    "      session.zeek.resp_p || session.zeek.resp_l2_addr || session.zeek.proto || session.zeek.service || " +
    "      session.zeek.user)\n" +
    "    dl.sessionDetailMeta(suffix=\"Basic Connection Info\")\n" +
    "      +arrayList(session.zeek, 'orig_h', 'Originating Host', 'zeek.orig_h')\n" +
    "      +arrayList(session.zeek, 'orig_l2_addr', 'Originating MAC', 'zeek.orig_l2_addr')\n" +
    "      +arrayList(session.zeek, 'orig_l2_oui', 'Originating OUI', 'zeek.orig_l2_oui')\n" +
    "      +arrayList(session.zeek, 'orig_hostname', 'Originating Host Name', 'zeek.orig_hostname')\n" +
    "      +arrayList(session.zeek, 'source_ip_reverse_dns', 'Originating Host rDNS', 'zeek.source_ip_reverse_dns')\n" +
    "      +arrayList(session.zeek, 'orig_segment', 'Originating Network Segment', 'zeek.orig_segment')\n" +
    "      +arrayList(session.zeek.source_geo, 'country_name', 'Originating GeoIP Country', 'zeek.source_geo.country_name')\n" +
    "      +arrayList(session.zeek.source_geo, 'city_name', 'Originating GeoIP City', 'zeek.source_geo.city_name')\n" +
    "      +arrayList(session.zeek, 'resp_h', 'Responding Host', 'zeek.resp_h')\n" +
    "      +arrayList(session.zeek, 'resp_l2_addr', 'Responding MAC', 'zeek.resp_l2_addr')\n" +
    "      +arrayList(session.zeek, 'resp_l2_oui', 'Responding OUI', 'zeek.resp_l2_oui')\n" +
    "      +arrayList(session.zeek, 'resp_hostname', 'Responding Host Name', 'zeek.resp_hostname')\n" +
    "      +arrayList(session.zeek, 'destination_ip_reverse_dns', 'Responding Host rDNS', 'zeek.destination_ip_reverse_dns')\n" +
    "      +arrayList(session.zeek, 'resp_segment', 'Responding Network Segment', 'zeek.resp_segment')\n" +
    "      +arrayList(session.zeek.destination_geo, 'country_name', 'Responding GeoIP Country', 'zeek.destination_geo.country_name')\n" +
    "      +arrayList(session.zeek.destination_geo, 'city_name', 'Responding GeoIP City', 'zeek.destination_geo.city_name')\n" +
    "      +arrayList(session.zeek, 'orig_p', 'Originating Port', 'zeek.orig_p')\n" +
    "      +arrayList(session.zeek, 'resp_p', 'Responding Port', 'zeek.resp_p')\n" +
    "      +arrayList(session.zeek, 'proto', 'Protocol', 'zeek.proto')\n" +
    "      +arrayList(session.zeek, 'service', 'Service', 'zeek.service')\n" +
    "      +arrayList(session.zeek, 'user', 'User', 'zeek.user')\n" +

    // file information
    "  if (session.zeek.fuid || session.zeek.filename || session.zeek.filetype)\n" +
    "    dl.sessionDetailMeta(suffix=\"File IDs\")\n" +
    "      +arrayList(session.zeek, 'fuid', 'File ID', 'zeek.fuid')\n" +
    "      +arrayList(session.zeek, 'filename', 'File Name', 'zeek.filename')\n" +
    "      +arrayList(session.zeek, 'filetype', 'File Magic', 'zeek.filetype')\n" +

    // conn.log
    "  if (session.zeek_conn)\n" +
    "    dl.sessionDetailMeta(suffix=\"conn.log\")\n" +
    "      +arrayList(session.zeek_conn, 'duration', 'conn duration', 'zeek_conn.duration')\n" +
    "      +arrayList(session.zeek_conn, 'orig_bytes', 'conn orig_bytes', 'zeek_conn.orig_bytes')\n" +
    "      +arrayList(session.zeek_conn, 'resp_bytes', 'conn resp_bytes', 'zeek_conn.resp_bytes')\n" +
    "      +arrayList(session.zeek_conn, 'conn_state', 'conn conn_state', 'zeek_conn.conn_state')\n" +
    "      +arrayList(session.zeek_conn, 'conn_state_description', 'conn conn_state_description', 'zeek_conn.conn_state_description')\n" +
    "      +arrayList(session.zeek_conn, 'local_orig', 'conn local_orig', 'zeek_conn.local_orig')\n" +
    "      +arrayList(session.zeek_conn, 'local_resp', 'conn local_resp', 'zeek_conn.local_resp')\n" +
    "      +arrayList(session.zeek_conn, 'missed_bytes', 'conn missed_bytes', 'zeek_conn.missed_bytes')\n" +
    "      +arrayList(session.zeek_conn, 'history', 'conn history', 'zeek_conn.history')\n" +
    "      +arrayList(session.zeek_conn, 'orig_pkts', 'conn orig_pkts', 'zeek_conn.orig_pkts')\n" +
    "      +arrayList(session.zeek_conn, 'orig_ip_bytes', 'conn orig_ip_bytes', 'zeek_conn.orig_ip_bytes')\n" +
    "      +arrayList(session.zeek_conn, 'resp_pkts', 'conn resp_pkts', 'zeek_conn.resp_pkts')\n" +
    "      +arrayList(session.zeek_conn, 'resp_ip_bytes', 'conn resp_ip_bytes', 'zeek_conn.resp_ip_bytes')\n" +
    "      +arrayList(session.zeek_conn, 'tunnel_parents', 'conn tunnel_parents', 'zeek_conn.tunnel_parents')\n" +
    "      +arrayList(session.zeek_conn, 'vlan', 'conn vlan', 'zeek_conn.vlan')\n" +
    "      +arrayList(session.zeek_conn, 'inner_vlan', 'conn inner_vlan', 'zeek_conn.inner_vlan')\n" +

    // dce_rpc.log
    "  if (session.zeek_dce_rpc)\n" +
    "    dl.sessionDetailMeta(suffix=\"dce_rpc.log\")\n" +
    "      +arrayList(session.zeek_dce_rpc, 'rtt', 'dce_rpc rtt', 'zeek_dce_rpc.rtt')\n" +
    "      +arrayList(session.zeek_dce_rpc, 'named_pipe', 'dce_rpc named_pipe', 'zeek_dce_rpc.named_pipe')\n" +
    "      +arrayList(session.zeek_dce_rpc, 'endpoint', 'dce_rpc endpoint', 'zeek_dce_rpc.endpoint')\n" +
    "      +arrayList(session.zeek_dce_rpc, 'operation', 'dce_rpc operation', 'zeek_dce_rpc.operation')\n" +

    // dhcp.log
    "  if (session.zeek_dhcp)\n" +
    "    dl.sessionDetailMeta(suffix=\"dhcp.log\")\n" +
    "      +arrayList(session.zeek_dhcp, 'mac', 'dhcp mac', 'zeek_dhcp.mac')\n" +
    "      +arrayList(session.zeek_dhcp, 'assigned_ip', 'dhcp assigned_ip', 'zeek_dhcp.assigned_ip')\n" +
    "      +arrayList(session.zeek_dhcp, 'lease_time', 'dhcp lease_time', 'zeek_dhcp.lease_time')\n" +
    "      +arrayList(session.zeek_dhcp, 'trans_id', 'dhcp trans_id', 'zeek_dhcp.trans_id')\n" +

    // dnp3.log
    "  if (session.zeek_dnp3)\n" +
    "    dl.sessionDetailMeta(suffix=\"dnp3.log\")\n" +
    "      +arrayList(session.zeek_dnp3, 'fc_request', 'dnp3 fc_request', 'zeek_dnp3.fc_request')\n" +
    "      +arrayList(session.zeek_dnp3, 'fc_reply', 'dnp3 fc_reply', 'zeek_dnp3.fc_reply')\n" +
    "      +arrayList(session.zeek_dnp3, 'iin', 'dnp3 iin', 'zeek_dnp3.iin')\n" +

    // dns.log
    "  if (session.zeek_dns)\n" +
    "    dl.sessionDetailMeta(suffix=\"dns.log\")\n" +
    "      +arrayList(session.zeek_dns, 'trans_id', 'dns trans_id', 'zeek_dns.trans_id')\n" +
    "      +arrayList(session.zeek_dns, 'rtt', 'dns rtt', 'zeek_dns.rtt')\n" +
    "      +arrayList(session.zeek_dns, 'query', 'dns query', 'zeek_dns.query')\n" +
    "      +arrayList(session.zeek_dns, 'qclass', 'dns qclass', 'zeek_dns.qclass')\n" +
    "      +arrayList(session.zeek_dns, 'qclass_name', 'dns qclass_name', 'zeek_dns.qclass_name')\n" +
    "      +arrayList(session.zeek_dns, 'qtype', 'dns qtype', 'zeek_dns.qtype')\n" +
    "      +arrayList(session.zeek_dns, 'qtype_name', 'dns qtype_name', 'zeek_dns.qtype_name')\n" +
    "      +arrayList(session.zeek_dns, 'rcode', 'dns rcode', 'zeek_dns.rcode')\n" +
    "      +arrayList(session.zeek_dns, 'rcode_name', 'dns rcode_name', 'zeek_dns.rcode_name')\n" +
    "      +arrayList(session.zeek_dns, 'AA', 'dns AA', 'zeek_dns.AA')\n" +
    "      +arrayList(session.zeek_dns, 'TC', 'dns TC', 'zeek_dns.TC')\n" +
    "      +arrayList(session.zeek_dns, 'RD', 'dns RD', 'zeek_dns.RD')\n" +
    "      +arrayList(session.zeek_dns, 'RA', 'dns RA', 'zeek_dns.RA')\n" +
    "      +arrayList(session.zeek_dns, 'Z', 'dns Z', 'zeek_dns.Z')\n" +
    "      +arrayList(session.zeek_dns, 'answers', 'dns answers', 'zeek_dns.answers')\n" +
    "      +arrayList(session.zeek_dns, 'TTLs', 'dns TTLs', 'zeek_dns.TTLs')\n" +
    "      +arrayList(session.zeek_dns, 'rejected', 'dns rejected', 'zeek_dns.rejected')\n" +

    // dpd.log
    "  if (session.zeek_dpd)\n" +
    "    dl.sessionDetailMeta(suffix=\"dpd.log\")\n" +
    "      +arrayList(session.zeek_dpd, 'service', 'dpd service', 'zeek_dpd.service')\n" +
    "      +arrayList(session.zeek_dpd, 'failure_reason', 'dpd failure_reason', 'zeek_dpd.failure_reason')\n" +

    // files.log
    "  if (session.zeek_files)\n" +
    "    dl.sessionDetailMeta(suffix=\"files.log\")\n" +
    "      +arrayList(session.zeek_files, 'fuid', 'files fuid', 'zeek_files.fuid')\n" +
    "      +arrayList(session.zeek_files, 'tx_hosts', 'files tx_hosts', 'zeek_files.tx_hosts')\n" +
    "      +arrayList(session.zeek_files, 'rx_hosts', 'files rx_hosts', 'zeek_files.rx_hosts')\n" +
    "      +arrayList(session.zeek_files, 'conn_uids', 'files conn_uids', 'zeek_files.conn_uids')\n" +
    "      +arrayList(session.zeek_files, 'source', 'files source', 'zeek_files.source')\n" +
    "      +arrayList(session.zeek_files, 'depth', 'files depth', 'zeek_files.depth')\n" +
    "      +arrayList(session.zeek_files, 'analyzers', 'files analyzers', 'zeek_files.analyzers')\n" +
    "      +arrayList(session.zeek_files, 'mime_type', 'files mime_type', 'zeek_files.mime_type')\n" +
    "      +arrayList(session.zeek_files, 'filename', 'files filename', 'zeek_files.filename')\n" +
    "      +arrayList(session.zeek_files, 'duration', 'files duration', 'zeek_files.duration')\n" +
    "      +arrayList(session.zeek_files, 'local_orig', 'files local_orig', 'zeek_files.local_orig')\n" +
    "      +arrayList(session.zeek_files, 'is_orig', 'files is_orig', 'zeek_files.is_orig')\n" +
    "      +arrayList(session.zeek_files, 'seen_bytes', 'files seen_bytes', 'zeek_files.seen_bytes')\n" +
    "      +arrayList(session.zeek_files, 'total_bytes', 'files total_bytes', 'zeek_files.total_bytes')\n" +
    "      +arrayList(session.zeek_files, 'missing_bytes', 'files missing_bytes', 'zeek_files.missing_bytes')\n" +
    "      +arrayList(session.zeek_files, 'overflow_bytes', 'files overflow_bytes', 'zeek_files.overflow_bytes')\n" +
    "      +arrayList(session.zeek_files, 'timedout', 'files timedout', 'zeek_files.timedout')\n" +
    "      +arrayList(session.zeek_files, 'parent_fuid', 'files parent_fuid', 'zeek_files.parent_fuid')\n" +
    "      +arrayList(session.zeek_files, 'md5', 'files md5', 'zeek_files.md5')\n" +
    "      +arrayList(session.zeek_files, 'sha1', 'files sha1', 'zeek_files.sha1')\n" +
    "      +arrayList(session.zeek_files, 'sha256', 'files sha256', 'zeek_files.sha256')\n" +
    "      +arrayList(session.zeek_files, 'extracted', 'files extracted', 'zeek_files.extracted')\n" +
    "      +arrayList(session.zeek_files, 'extracted_cutoff', 'files extracted_cutoff', 'zeek_files.extracted_cutoff')\n" +
    "      +arrayList(session.zeek_files, 'extracted_size', 'files extracted_size', 'zeek_files.extracted_size')\n" +

    // ftp.log
    "  if (session.zeek_ftp)\n" +
    "    dl.sessionDetailMeta(suffix=\"ftp.log\")\n" +
    "      +arrayList(session.zeek_ftp, 'password', 'ftp password', 'zeek_ftp.password')\n" +
    "      +arrayList(session.zeek_ftp, 'command', 'ftp command', 'zeek_ftp.command')\n" +
    "      +arrayList(session.zeek_ftp, 'arg', 'ftp arg', 'zeek_ftp.arg')\n" +
    "      +arrayList(session.zeek_ftp, 'mime_type', 'ftp mime_type', 'zeek_ftp.mime_type')\n" +
    "      +arrayList(session.zeek_ftp, 'file_size', 'ftp file_size', 'zeek_ftp.file_size')\n" +
    "      +arrayList(session.zeek_ftp, 'reply_code', 'ftp reply_code', 'zeek_ftp.reply_code')\n" +
    "      +arrayList(session.zeek_ftp, 'reply_msg', 'ftp reply_msg', 'zeek_ftp.reply_msg')\n" +
    "      +arrayList(session.zeek_ftp, 'data_channel_passive', 'ftp data_channel_passive', 'zeek_ftp.data_channel_passive')\n" +
    "      +arrayList(session.zeek_ftp, 'data_channel_orig_h', 'ftp data_channel_orig_h', 'zeek_ftp.data_channel_orig_h')\n" +
    "      +arrayList(session.zeek_ftp, 'data_channel_resp_h', 'ftp data_channel_resp_h', 'zeek_ftp.data_channel_resp_h')\n" +
    "      +arrayList(session.zeek_ftp, 'data_channel_resp_p', 'ftp data_channel_resp_p', 'zeek_ftp.data_channel_resp_p')\n" +
    "      +arrayList(session.zeek_ftp, 'fuid', 'ftp fuid', 'zeek_ftp.fuid')\n" +

    // http.log
    "  if (session.zeek_http)\n" +
    "    dl.sessionDetailMeta(suffix=\"http.log\")\n" +
    "      +arrayList(session.zeek_http, 'trans_depth', 'http trans_depth', 'zeek_http.trans_depth')\n" +
    "      +arrayList(session.zeek_http, 'method', 'http method', 'zeek_http.method')\n" +
    "      +arrayList(session.zeek_http, 'host', 'http host', 'zeek_http.host')\n" +
    "      +arrayList(session.zeek_http, 'uri', 'http uri', 'zeek_http.uri')\n" +
    "      +arrayList(session.zeek_http, 'referrer', 'http referrer', 'zeek_http.referrer')\n" +
    "      +arrayList(session.zeek_http, 'version', 'http version', 'zeek_http.version')\n" +
    "      +arrayList(session.zeek_http, 'user_agent', 'http user_agent', 'zeek_http.user_agent')\n" +
    "      +arrayList(session.zeek_http, 'request_body_len', 'http request_body_len', 'zeek_http.request_body_len')\n" +
    "      +arrayList(session.zeek_http, 'response_body_len', 'http response_body_len', 'zeek_http.response_body_len')\n" +
    "      +arrayList(session.zeek_http, 'status_code', 'http status_code', 'zeek_http.status_code')\n" +
    "      +arrayList(session.zeek_http, 'status_msg', 'http status_msg', 'zeek_http.status_msg')\n" +
    "      +arrayList(session.zeek_http, 'info_code', 'http info_code', 'zeek_http.info_code')\n" +
    "      +arrayList(session.zeek_http, 'info_msg', 'http info_msg', 'zeek_http.info_msg')\n" +
    "      +arrayList(session.zeek_http, 'tags', 'http tags', 'zeek_http.tags')\n" +
    "      +arrayList(session.zeek_http, 'user', 'http user', 'zeek_http.user')\n" +
    "      +arrayList(session.zeek_http, 'password', 'http password', 'zeek_http.password')\n" +
    "      +arrayList(session.zeek_http, 'proxied', 'http proxied', 'zeek_http.proxied')\n" +
    "      +arrayList(session.zeek_http, 'orig_fuids', 'http orig_fuids', 'zeek_http.orig_fuids')\n" +
    "      +arrayList(session.zeek_http, 'orig_filenames', 'http orig_filenames', 'zeek_http.orig_filenames')\n" +
    "      +arrayList(session.zeek_http, 'orig_mime_types', 'http orig_mime_types', 'zeek_http.orig_mime_types')\n" +
    "      +arrayList(session.zeek_http, 'resp_fuids', 'http resp_fuids', 'zeek_http.resp_fuids')\n" +
    "      +arrayList(session.zeek_http, 'resp_filenames', 'http resp_filenames', 'zeek_http.resp_filenames')\n" +
    "      +arrayList(session.zeek_http, 'resp_mime_types', 'http resp_mime_types', 'zeek_http.resp_mime_types')\n" +

    // intel.log
    "  if (session.zeek_intel)\n" +
    "    dl.sessionDetailMeta(suffix=\"intel.log\")\n" +
    "      +arrayList(session.zeek_intel, 'indicator', 'intel indicator', 'zeek_intel.indicator')\n" +
    "      +arrayList(session.zeek_intel, 'indicator_type', 'intel indicator_type', 'zeek_intel.indicator_type')\n" +
    "      +arrayList(session.zeek_intel, 'seen_where', 'intel seen_where', 'zeek_intel.seen_where')\n" +
    "      +arrayList(session.zeek_intel, 'seen_node', 'intel seen_node', 'zeek_intel.seen_node')\n" +
    "      +arrayList(session.zeek_intel, 'matched', 'intel matched', 'zeek_intel.matched')\n" +
    "      +arrayList(session.zeek_intel, 'sources', 'intel sources', 'zeek_intel.sources')\n" +
    "      +arrayList(session.zeek_intel, 'fuid', 'intel fuid', 'zeek_intel.fuid')\n" +
    "      +arrayList(session.zeek_intel, 'mimetype', 'intel mimetype', 'zeek_intel.mimetype')\n" +
    "      +arrayList(session.zeek_intel, 'file_description', 'intel file_description', 'zeek_intel.file_description')\n" +

    // irc.log
    "  if (session.zeek_irc)\n" +
    "    dl.sessionDetailMeta(suffix=\"irc.log\")\n" +
    "      +arrayList(session.zeek_irc, 'nick', 'irc nick', 'zeek_irc.nick')\n" +
    "      +arrayList(session.zeek_irc, 'command', 'irc command', 'zeek_irc.command')\n" +
    "      +arrayList(session.zeek_irc, 'value', 'irc value', 'zeek_irc.value')\n" +
    "      +arrayList(session.zeek_irc, 'addl', 'irc addl', 'zeek_irc.addl')\n" +
    "      +arrayList(session.zeek_irc, 'dcc_file_name', 'irc dcc_file_name', 'zeek_irc.dcc_file_name')\n" +
    "      +arrayList(session.zeek_irc, 'dcc_file_size', 'irc dcc_file_size', 'zeek_irc.dcc_file_size')\n" +
    "      +arrayList(session.zeek_irc, 'dcc_mime_type', 'irc dcc_mime_type', 'zeek_irc.dcc_mime_type')\n" +
    "      +arrayList(session.zeek_irc, 'fuid', 'irc fuid', 'zeek_irc.fuid')\n" +

    // kerberos.log
    "  if (session.zeek_kerberos)\n" +
    "    dl.sessionDetailMeta(suffix=\"kerberos.log\")\n" +
    "      +arrayList(session.zeek_kerberos, 'cname', 'kerberos cname', 'zeek_kerberos.cname')\n" +
    "      +arrayList(session.zeek_kerberos, 'sname', 'kerberos sname', 'zeek_kerberos.sname')\n" +
    "      +arrayList(session.zeek_kerberos, 'success', 'kerberos success', 'zeek_kerberos.success')\n" +
    "      +arrayList(session.zeek_kerberos, 'error_msg', 'kerberos error_msg', 'zeek_kerberos.error_msg')\n" +
    "      +arrayList(session.zeek_kerberos, 'from', 'kerberos from', 'zeek_kerberos.from')\n" +
    "      +arrayList(session.zeek_kerberos, 'till', 'kerberos till', 'zeek_kerberos.till')\n" +
    "      +arrayList(session.zeek_kerberos, 'cipher', 'kerberos cipher', 'zeek_kerberos.cipher')\n" +
    "      +arrayList(session.zeek_kerberos, 'forwardable', 'kerberos forwardable', 'zeek_kerberos.forwardable')\n" +
    "      +arrayList(session.zeek_kerberos, 'renewable', 'kerberos renewable', 'zeek_kerberos.renewable')\n" +
    "      +arrayList(session.zeek_kerberos, 'client_cert_subject', 'kerberos client_cert_subject', 'zeek_kerberos.client_cert_subject')\n" +
    "      +arrayList(session.zeek_kerberos, 'client_cert_fuid', 'kerberos client_cert_fuid', 'zeek_kerberos.client_cert_fuid')\n" +
    "      +arrayList(session.zeek_kerberos, 'server_cert_subject', 'kerberos server_cert_subject', 'zeek_kerberos.server_cert_subject')\n" +
    "      +arrayList(session.zeek_kerberos, 'server_cert_fuid', 'kerberos server_cert_fuid', 'zeek_kerberos.server_cert_fuid')\n" +

    // modbus.log
    "  if (session.zeek_modbus)\n" +
    "    dl.sessionDetailMeta(suffix=\"modbus.log\")\n" +
    "      +arrayList(session.zeek_modbus, 'func', 'modbus func', 'zeek_modbus.func')\n" +
    "      +arrayList(session.zeek_modbus, 'exception', 'modbus exception', 'zeek_modbus.exception')\n" +

    // mysql.log
    "  if (session.zeek_mysql)\n" +
    "    dl.sessionDetailMeta(suffix=\"mysql.log\")\n" +
    "      +arrayList(session.zeek_mysql, 'cmd', 'mysql cmd', 'zeek_mysql.cmd')\n" +
    "      +arrayList(session.zeek_mysql, 'arg', 'mysql arg', 'zeek_mysql.arg')\n" +
    "      +arrayList(session.zeek_mysql, 'success', 'mysql success', 'zeek_mysql.success')\n" +
    "      +arrayList(session.zeek_mysql, 'rows', 'mysql rows', 'zeek_mysql.rows')\n" +
    "      +arrayList(session.zeek_mysql, 'response', 'mysql response', 'zeek_mysql.response')\n" +

    // notice.log
    "  if (session.zeek_notice)\n" +
    "    dl.sessionDetailMeta(suffix=\"notice.log\")\n" +
    "      +arrayList(session.zeek_notice, 'fuid', 'notice fuid', 'zeek_notice.fuid')\n" +
    "      +arrayList(session.zeek_notice, 'file_mime_type', 'notice file_mime_type', 'zeek_notice.file_mime_type')\n" +
    "      +arrayList(session.zeek_notice, 'file_desc', 'notice file_desc', 'zeek_notice.file_desc')\n" +
    "      +arrayList(session.zeek_notice, 'note', 'notice note', 'zeek_notice.note')\n" +
    "      +arrayList(session.zeek_notice, 'msg', 'notice msg', 'zeek_notice.msg')\n" +
    "      +arrayList(session.zeek_notice, 'sub', 'notice sub', 'zeek_notice.sub')\n" +
    "      +arrayList(session.zeek_notice, 'src', 'notice src', 'zeek_notice.src')\n" +
    "      +arrayList(session.zeek_notice, 'dst', 'notice dst', 'zeek_notice.dst')\n" +
    "      +arrayList(session.zeek_notice, 'p', 'notice p', 'zeek_notice.p')\n" +
    "      +arrayList(session.zeek_notice, 'n', 'notice n', 'zeek_notice.n')\n" +
    "      +arrayList(session.zeek_notice, 'peer_descr', 'notice peer_descr', 'zeek_notice.peer_descr')\n" +
    "      +arrayList(session.zeek_notice, 'actions', 'notice actions', 'zeek_notice.actions')\n" +
    "      +arrayList(session.zeek_notice, 'suppress_for', 'notice suppress_for', 'zeek_notice.suppress_for')\n" +
    "      +arrayList(session.zeek_notice, 'dropped', 'notice dropped', 'zeek_notice.dropped')\n" +
    "      +arrayList(session.zeek_notice, 'remote_location_country_code', 'notice remote_location_country_code', 'zeek_notice.remote_location_country_code')\n" +
    "      +arrayList(session.zeek_notice, 'remote_location_region', 'notice remote_location_region', 'zeek_notice.remote_location_region')\n" +
    "      +arrayList(session.zeek_notice, 'remote_location_cityremote_location_latitude', 'notice remote_location_cityremote_location_latitude', 'zeek_notice.remote_location_cityremote_location_latitude')\n" +
    "      +arrayList(session.zeek_notice, 'remote_location_longitude', 'notice remote_location_longitude', 'zeek_notice.remote_location_longitude')\n" +

    // ntlm.log
    "  if (session.zeek_ntlm)\n" +
    "    dl.sessionDetailMeta(suffix=\"ntlm.log\")\n" +
    "      +arrayList(session.zeek_ntlm, 'host', 'ntlm host', 'zeek_ntlm.host')\n" +
    "      +arrayList(session.zeek_ntlm, 'domain', 'ntlm domain', 'zeek_ntlm.domain')\n" +
    "      +arrayList(session.zeek_ntlm, 'success', 'ntlm success', 'zeek_ntlm.success')\n" +
    "      +arrayList(session.zeek_ntlm, 'status', 'ntlm status', 'zeek_ntlm.status')\n" +

    // pe.log
    "  if (session.zeek_pe)\n" +
    "    dl.sessionDetailMeta(suffix=\"pe.log\")\n" +
    "      +arrayList(session.zeek_pe, 'fuid', 'pe fuid', 'zeek_pe.fuid')\n" +
    "      +arrayList(session.zeek_pe, 'machine', 'pe machine', 'zeek_pe.machine')\n" +
    "      +arrayList(session.zeek_pe, 'compile_ts', 'pe compile_ts', 'zeek_pe.compile_ts')\n" +
    "      +arrayList(session.zeek_pe, 'os', 'pe os', 'zeek_pe.os')\n" +
    "      +arrayList(session.zeek_pe, 'subsystem', 'pe subsystem', 'zeek_pe.subsystem')\n" +
    "      +arrayList(session.zeek_pe, 'is_exe', 'pe is_exe', 'zeek_pe.is_exe')\n" +
    "      +arrayList(session.zeek_pe, 'is_64bit', 'pe is_64bit', 'zeek_pe.is_64bit')\n" +
    "      +arrayList(session.zeek_pe, 'uses_aslr', 'pe uses_aslr', 'zeek_pe.uses_aslr')\n" +
    "      +arrayList(session.zeek_pe, 'uses_dep', 'pe uses_dep', 'zeek_pe.uses_dep')\n" +
    "      +arrayList(session.zeek_pe, 'uses_code_integrity', 'pe uses_code_integrity', 'zeek_pe.uses_code_integrity')\n" +
    "      +arrayList(session.zeek_pe, 'uses_seh', 'pe uses_seh', 'zeek_pe.uses_seh')\n" +
    "      +arrayList(session.zeek_pe, 'has_import_table', 'pe has_import_table', 'zeek_pe.has_import_table')\n" +
    "      +arrayList(session.zeek_pe, 'has_export_table', 'pe has_export_table', 'zeek_pe.has_export_table')\n" +
    "      +arrayList(session.zeek_pe, 'has_cert_table', 'pe has_cert_table', 'zeek_pe.has_cert_table')\n" +
    "      +arrayList(session.zeek_pe, 'has_debug_data', 'pe has_debug_data', 'zeek_pe.has_debug_data')\n" +
    "      +arrayList(session.zeek_pe, 'section_names', 'pe section_names', 'zeek_pe.section_names')\n" +

    // radius.log
    "  if (session.zeek_radius)\n" +
    "    dl.sessionDetailMeta(suffix=\"radius.log\")\n" +
    "      +arrayList(session.zeek_radius, 'mac', 'radius mac', 'zeek_radius.mac')\n" +
    "      +arrayList(session.zeek_radius, 'framed_addr', 'radius framed_addr', 'zeek_radius.framed_addr')\n" +
    "      +arrayList(session.zeek_radius, 'remote_ip', 'radius remote_ip', 'zeek_radius.remote_ip')\n" +
    "      +arrayList(session.zeek_radius, 'connect_info', 'radius connect_info', 'zeek_radius.connect_info')\n" +
    "      +arrayList(session.zeek_radius, 'reply_msg', 'radius reply_msg', 'zeek_radius.reply_msg')\n" +
    "      +arrayList(session.zeek_radius, 'result', 'radius result', 'zeek_radius.result')\n" +
    "      +arrayList(session.zeek_radius, 'ttl', 'radius ttl', 'zeek_radius.ttl')\n" +

    // rdp.log
    "  if (session.zeek_rdp)\n" +
    "    dl.sessionDetailMeta(suffix=\"rdp.log\")\n" +
    "      +arrayList(session.zeek_rdp, 'cookie', 'rdp cookie', 'zeek_rdp.cookie')\n" +
    "      +arrayList(session.zeek_rdp, 'result', 'rdp result', 'zeek_rdp.result')\n" +
    "      +arrayList(session.zeek_rdp, 'security_protocol', 'rdp security_protocol', 'zeek_rdp.security_protocol')\n" +
    "      +arrayList(session.zeek_rdp, 'keyboard_layout', 'rdp keyboard_layout', 'zeek_rdp.keyboard_layout')\n" +
    "      +arrayList(session.zeek_rdp, 'client_build', 'rdp client_build', 'zeek_rdp.client_build')\n" +
    "      +arrayList(session.zeek_rdp, 'client_name', 'rdp client_build', 'zeek_rdp.client_name')\n" +
    "      +arrayList(session.zeek_rdp, 'client_dig_product_id', 'rdp client_dig_product_id', 'zeek_rdp.client_dig_product_id')\n" +
    "      +arrayList(session.zeek_rdp, 'desktop_width', 'rdp desktop_width', 'zeek_rdp.desktop_width')\n" +
    "      +arrayList(session.zeek_rdp, 'desktop_height', 'rdp desktop_height', 'zeek_rdp.desktop_height')\n" +
    "      +arrayList(session.zeek_rdp, 'requested_color_depth', 'rdp requested_color_depth', 'zeek_rdp.requested_color_depth')\n" +
    "      +arrayList(session.zeek_rdp, 'cert_type', 'rdp cert_type', 'zeek_rdp.cert_type')\n" +
    "      +arrayList(session.zeek_rdp, 'cert_count', 'rdp cert_count', 'zeek_rdp.cert_count')\n" +
    "      +arrayList(session.zeek_rdp, 'cert_permanent', 'rdp cert_permanent', 'zeek_rdp.cert_permanent')\n" +
    "      +arrayList(session.zeek_rdp, 'encryption_level', 'rdp encryption_level', 'zeek_rdp.encryption_level')\n" +
    "      +arrayList(session.zeek_rdp, 'encryption_method', 'rdp encryption_method', 'zeek_rdp.encryption_method')\n" +

    // rfb.log
    "  if (session.zeek_rfb)\n" +
    "    dl.sessionDetailMeta(suffix=\"rfb.log\")\n" +
    "      +arrayList(session.zeek_rfb, 'client_major_version', 'rfb client_major_version', 'zeek_rfb.client_major_version')\n" +
    "      +arrayList(session.zeek_rfb, 'client_minor_version', 'rfb client_minor_version', 'zeek_rfb.client_minor_version')\n" +
    "      +arrayList(session.zeek_rfb, 'server_major_version', 'rfb server_major_version', 'zeek_rfb.server_major_version')\n" +
    "      +arrayList(session.zeek_rfb, 'server_minor_version', 'rfb server_minor_version', 'zeek_rfb.server_minor_version')\n" +
    "      +arrayList(session.zeek_rfb, 'authentication_method', 'rfb authentication_method', 'zeek_rfb.authentication_method')\n" +
    "      +arrayList(session.zeek_rfb, 'auth', 'rfb auth', 'zeek_rfb.auth')\n" +
    "      +arrayList(session.zeek_rfb, 'share_flag', 'rfb share_flag', 'zeek_rfb.share_flag')\n" +
    "      +arrayList(session.zeek_rfb, 'desktop_name', 'rfb desktop_name', 'zeek_rfb.desktop_name')\n" +
    "      +arrayList(session.zeek_rfb, 'width', 'rfb width', 'zeek_rfb.width')\n" +
    "      +arrayList(session.zeek_rfb, 'height', 'rfb height', 'zeek_rfb.height')\n" +

    // signatures.log
    "  if (session.zeek_signatures)\n" +
    "    dl.sessionDetailMeta(suffix=\"signatures.log\")\n" +
    "      +arrayList(session.zeek_signatures, 'note', 'signatures note', 'zeek_signatures.note')\n" +
    "      +arrayList(session.zeek_signatures, 'signature_id', 'signatures signature_id', 'zeek_signatures.signature_id')\n" +
    "      +arrayList(session.zeek_signatures, 'engine', 'signatures engine', 'zeek_signatures.engine')\n" +
    "      +arrayList(session.zeek_signatures, 'event_message', 'signatures event_message', 'zeek_signatures.event_message')\n" +
    "      +arrayList(session.zeek_signatures, 'sub_message', 'signatures sub_message', 'zeek_signatures.sub_message')\n" +
    "      +arrayList(session.zeek_signatures, 'signature_count', 'signatures signature_count', 'zeek_signatures.signature_count')\n" +
    "      +arrayList(session.zeek_signatures, 'host_count', 'signatures host_count', 'zeek_signatures.host_count')\n" +

    // sip.log
    "  if (session.zeek_sip)\n" +
    "    dl.sessionDetailMeta(suffix=\"sip.log\")\n" +
    "      +arrayList(session.zeek_sip, 'trans_depth', 'sip trans_depth', 'zeek_sip.trans_depth')\n" +
    "      +arrayList(session.zeek_sip, 'method', 'sip method', 'zeek_sip.method')\n" +
    "      +arrayList(session.zeek_sip, 'uri', 'sip uri', 'zeek_sip.uri')\n" +
    "      +arrayList(session.zeek_sip, 'date', 'sip date', 'zeek_sip.date')\n" +
    "      +arrayList(session.zeek_sip, 'request_from', 'sip request_from', 'zeek_sip.request_from')\n" +
    "      +arrayList(session.zeek_sip, 'request_to', 'sip request_to', 'zeek_sip.request_to')\n" +
    "      +arrayList(session.zeek_sip, 'response_from', 'sip response_from', 'zeek_sip.response_from')\n" +
    "      +arrayList(session.zeek_sip, 'response_to', 'sip response_to', 'zeek_sip.response_to')\n" +
    "      +arrayList(session.zeek_sip, 'reply_to', 'sip reply_to', 'zeek_sip.reply_to')\n" +
    "      +arrayList(session.zeek_sip, 'call_id', 'sip call_id', 'zeek_sip.call_id')\n" +
    "      +arrayList(session.zeek_sip, 'seq', 'sip seq', 'zeek_sip.seq')\n" +
    "      +arrayList(session.zeek_sip, 'subject', 'sip subject', 'zeek_sip.subject')\n" +
    "      +arrayList(session.zeek_sip, 'request_path', 'sip request_path', 'zeek_sip.request_path')\n" +
    "      +arrayList(session.zeek_sip, 'response_path', 'sip response_path', 'zeek_sip.response_path')\n" +
    "      +arrayList(session.zeek_sip, 'user_agent', 'sip user_agent', 'zeek_sip.user_agent')\n" +
    "      +arrayList(session.zeek_sip, 'status_code', 'sip status_code', 'zeek_sip.status_code')\n" +
    "      +arrayList(session.zeek_sip, 'status_msg', 'sip status_msg', 'zeek_sip.status_msg')\n" +
    "      +arrayList(session.zeek_sip, 'warning', 'sip warning', 'zeek_sip.warning')\n" +
    "      +arrayList(session.zeek_sip, 'request_body_len', 'sip request_body_len', 'zeek_sip.request_body_len')\n" +
    "      +arrayList(session.zeek_sip, 'response_body_len', 'sip response_body_len', 'zeek_sip.response_body_len')\n" +
    "      +arrayList(session.zeek_sip, 'content_type', 'sip content_type', 'zeek_sip.content_type')\n" +

    // smb_files.log
    "  if (session.zeek_smb_files)\n" +
    "    dl.sessionDetailMeta(suffix=\"smb_files.log\")\n" +
    "      +arrayList(session.zeek_smb_files, 'fuid', 'smb_files fuid', 'zeek_smb_files.fuid')\n" +
    "      +arrayList(session.zeek_smb_files, 'action', 'smb_files action', 'zeek_smb_files.action')\n" +
    "      +arrayList(session.zeek_smb_files, 'path', 'smb_files path', 'zeek_smb_files.path')\n" +
    "      +arrayList(session.zeek_smb_files, 'name', 'smb_files name', 'zeek_smb_files.name')\n" +
    "      +arrayList(session.zeek_smb_files, 'size', 'smb_files size', 'zeek_smb_files.size')\n" +
    "      +arrayList(session.zeek_smb_files, 'prev_name', 'smb_files prev_name', 'zeek_smb_files.prev_name')\n" +
    "      +arrayList(session.zeek_smb_files, 'times_modified', 'smb_files times_modified', 'zeek_smb_files.times_modified')\n" +
    "      +arrayList(session.zeek_smb_files, 'times_accessed', 'smb_files times_accessed', 'zeek_smb_files.times_accessed')\n" +
    "      +arrayList(session.zeek_smb_files, 'times_created', 'smb_files times_created', 'zeek_smb_files.times_created')\n" +
    "      +arrayList(session.zeek_smb_files, 'times_changed', 'smb_files times_changed', 'zeek_smb_files.times_changed')\n" +

    // smb_mapping.log
    "  if (session.zeek_smb_mapping)\n" +
    "    dl.sessionDetailMeta(suffix=\"smb_mapping.log\")\n" +
    "      +arrayList(session.zeek_smb_mapping, 'path', 'smb_mapping path', 'zeek_smb_mapping.path')\n" +
    "      +arrayList(session.zeek_smb_mapping, 'resource_type', 'smb_mapping resource_type', 'zeek_smb_mapping.resource_type')\n" +
    "      +arrayList(session.zeek_smb_mapping, 'native_file_system', 'smb_mapping native_file_system', 'zeek_smb_mapping.native_file_system')\n" +
    "      +arrayList(session.zeek_smb_mapping, 'share_type', 'smb_mapping share_type', 'zeek_smb_mapping.share_type')\n" +

    // smtp.log
    "  if (session.zeek_smtp)\n" +
    "    dl.sessionDetailMeta(suffix=\"smtp.log\")\n" +
    "      +arrayList(session.zeek_smtp, 'trans_depth', 'smtp trans_depth', 'zeek_smtp.trans_depth')\n" +
    "      +arrayList(session.zeek_smtp, 'helo', 'smtp helo', 'zeek_smtp.helo')\n" +
    "      +arrayList(session.zeek_smtp, 'mailfrom', 'smtp mailfrom', 'zeek_smtp.mailfrom')\n" +
    "      +arrayList(session.zeek_smtp, 'rcptto', 'smtp rcptto', 'zeek_smtp.rcptto')\n" +
    "      +arrayList(session.zeek_smtp, 'date', 'smtp date', 'zeek_smtp.date')\n" +
    "      +arrayList(session.zeek_smtp, 'from', 'smtp from', 'zeek_smtp.from')\n" +
    "      +arrayList(session.zeek_smtp, 'to', 'smtp to', 'zeek_smtp.to')\n" +
    "      +arrayList(session.zeek_smtp, 'cc', 'smtp cc', 'zeek_smtp.cc')\n" +
    "      +arrayList(session.zeek_smtp, 'reply_to', 'smtp reply_to', 'zeek_smtp.reply_to')\n" +
    "      +arrayList(session.zeek_smtp, 'msg_id', 'smtp msg_id', 'zeek_smtp.msg_id')\n" +
    "      +arrayList(session.zeek_smtp, 'in_reply_to', 'smtp in_reply_to', 'zeek_smtp.in_reply_to')\n" +
    "      +arrayList(session.zeek_smtp, 'subject', 'smtp subject', 'zeek_smtp.subject')\n" +
    "      +arrayList(session.zeek_smtp, 'x_originating_ip', 'smtp x_originating_ip', 'zeek_smtp.x_originating_ip')\n" +
    "      +arrayList(session.zeek_smtp, 'first_received', 'smtp first_received', 'zeek_smtp.first_received')\n" +
    "      +arrayList(session.zeek_smtp, 'second_received', 'smtp second_received', 'zeek_smtp.second_received')\n" +
    "      +arrayList(session.zeek_smtp, 'last_reply', 'smtp last_reply', 'zeek_smtp.last_reply')\n" +
    "      +arrayList(session.zeek_smtp, 'path', 'smtp path', 'zeek_smtp.path')\n" +
    "      +arrayList(session.zeek_smtp, 'user_agent', 'smtp user_agent', 'zeek_smtp.user_agent')\n" +
    "      +arrayList(session.zeek_smtp, 'tls', 'smtp tls', 'zeek_smtp.tls')\n" +
    "      +arrayList(session.zeek_smtp, 'fuids', 'smtp fuids', 'zeek_smtp.fuids')\n" +
    "      +arrayList(session.zeek_smtp, 'is_webmail', 'smtp is_webmail', 'zeek_smtp.is_webmail')\n" +

    // snmp.log
    "  if (session.zeek_snmp)\n" +
    "    dl.sessionDetailMeta(suffix=\"snmp.log\")\n" +
    "      +arrayList(session.zeek_snmp, 'duration', 'snmp duration', 'zeek_snmp.duration')\n" +
    "      +arrayList(session.zeek_snmp, 'version', 'snmp version', 'zeek_snmp.version')\n" +
    "      +arrayList(session.zeek_snmp, 'community', 'snmp community', 'zeek_snmp.community')\n" +
    "      +arrayList(session.zeek_snmp, 'get_requests', 'snmp get_requests', 'zeek_snmp.get_requests')\n" +
    "      +arrayList(session.zeek_snmp, 'get_bulk_requests', 'snmp get_bulk_requests', 'zeek_snmp.get_bulk_requests')\n" +
    "      +arrayList(session.zeek_snmp, 'get_responses', 'snmp get_responses', 'zeek_snmp.get_responses')\n" +
    "      +arrayList(session.zeek_snmp, 'set_requests', 'snmp set_requests', 'zeek_snmp.set_requests')\n" +
    "      +arrayList(session.zeek_snmp, 'display_string', 'snmp display_string', 'zeek_snmp.display_string')\n" +
    "      +arrayList(session.zeek_snmp, 'up_since', 'snmp up_since', 'zeek_snmp.up_since')\n" +

    // socks.log
    "  if (session.zeek_socks)\n" +
    "    dl.sessionDetailMeta(suffix=\"socks.log\")\n" +
    "      +arrayList(session.zeek_socks, 'version', 'socks version', 'zeek_socks.version')\n" +
    "      +arrayList(session.zeek_socks, 'password', 'socks password', 'zeek_socks.password')\n" +
    "      +arrayList(session.zeek_socks, 'server_status', 'socks server_status', 'zeek_socks.server_status')\n" +
    "      +arrayList(session.zeek_socks, 'request_host', 'socks request_host', 'zeek_socks.request_host')\n" +
    "      +arrayList(session.zeek_socks, 'request_name', 'socks request_name', 'zeek_socks.request_name')\n" +
    "      +arrayList(session.zeek_socks, 'request_port', 'socks request_port', 'zeek_socks.request_port')\n" +
    "      +arrayList(session.zeek_socks, 'bound_host', 'socks bound_host', 'zeek_socks.bound_host')\n" +
    "      +arrayList(session.zeek_socks, 'bound_name', 'socks bound_name', 'zeek_socks.bound_name')\n" +
    "      +arrayList(session.zeek_socks, 'bound_port', 'socks bound_port', 'zeek_socks.bound_port')\n" +

    // software.log
    "  if (session.zeek_software)\n" +
    "    dl.sessionDetailMeta(suffix=\"software.log\")\n" +
    "      +arrayList(session.zeek_software, 'software_type', 'software software_type', 'zeek_software.software_type')\n" +
    "      +arrayList(session.zeek_software, 'name', 'software name', 'zeek_software.name')\n" +
    "      +arrayList(session.zeek_software, 'version_major', 'software version_major', 'zeek_software.version_major')\n" +
    "      +arrayList(session.zeek_software, 'version_minor', 'software version_minor', 'zeek_software.version_minor')\n" +
    "      +arrayList(session.zeek_software, 'version_minor2', 'software version_minor2', 'zeek_software.version_minor2')\n" +
    "      +arrayList(session.zeek_software, 'version_minor3', 'software version_minor3', 'zeek_software.version_minor3')\n" +
    "      +arrayList(session.zeek_software, 'version_addl', 'software version_addl', 'zeek_software.version_addl')\n" +
    "      +arrayList(session.zeek_software, 'unparsed_version', 'software unparsed_version', 'zeek_software.unparsed_version')\n" +

    // ssh.log
    "  if (session.zeek_ssh)\n" +
    "    dl.sessionDetailMeta(suffix=\"ssh.log\")\n" +
    "      +arrayList(session.zeek_ssh, 'version', 'ssh version', 'zeek_ssh.version')\n" +
    "      +arrayList(session.zeek_ssh, 'auth_success', 'ssh auth_success', 'zeek_ssh.auth_success')\n" +
    "      +arrayList(session.zeek_ssh, 'auth_attempts', 'ssh auth_attempts', 'zeek_ssh.auth_attempts')\n" +
    "      +arrayList(session.zeek_ssh, 'direction', 'ssh direction', 'zeek_ssh.direction')\n" +
    "      +arrayList(session.zeek_ssh, 'client', 'ssh client', 'zeek_ssh.client')\n" +
    "      +arrayList(session.zeek_ssh, 'server', 'ssh server', 'zeek_ssh.server')\n" +
    "      +arrayList(session.zeek_ssh, 'cipher_alg', 'ssh cipher_alg', 'zeek_ssh.cipher_alg')\n" +
    "      +arrayList(session.zeek_ssh, 'mac_alg', 'ssh mac_alg', 'zeek_ssh.mac_alg')\n" +
    "      +arrayList(session.zeek_ssh, 'compression_alg', 'ssh compression_alg', 'zeek_ssh.compression_alg')\n" +
    "      +arrayList(session.zeek_ssh, 'kex_alg', 'ssh kex_alg', 'zeek_ssh.kex_alg')\n" +
    "      +arrayList(session.zeek_ssh, 'host_key_alg', 'ssh host_key_alg', 'zeek_ssh.host_key_alg')\n" +
    "      +arrayList(session.zeek_ssh, 'host_key', 'ssh host_key', 'zeek_ssh.host_key')\n" +
    "      +arrayList(session.zeek_ssh, 'remote_location_country_code', 'ssh remote_location_country_code', 'zeek_ssh.remote_location_country_code')\n" +
    "      +arrayList(session.zeek_ssh, 'remote_location_region', 'ssh remote_location_region', 'zeek_ssh.remote_location_region')\n" +
    "      +arrayList(session.zeek_ssh, 'remote_location_city', 'ssh remote_location_city', 'zeek_ssh.remote_location_city')\n" +
    "      +arrayList(session.zeek_ssh, 'remote_location_latitude', 'ssh remote_location_latitude', 'zeek_ssh.remote_location_latitude')\n" +
    "      +arrayList(session.zeek_ssh, 'remote_location_longitude', 'ssh remote_location_longitude', 'zeek_ssh.remote_location_longitude')\n" +

    // ssl.log
    "  if (session.zeek_ssl)\n" +
    "    dl.sessionDetailMeta(suffix=\"ssl.log\")\n" +
    "      +arrayList(session.zeek_ssl, 'ssl_version', 'ssl ssl_version', 'zeek_ssl.ssl_version')\n" +
    "      +arrayList(session.zeek_ssl, 'cipher', 'ssl cipher', 'zeek_ssl.cipher')\n" +
    "      +arrayList(session.zeek_ssl, 'curve', 'ssl curve', 'zeek_ssl.curve')\n" +
    "      +arrayList(session.zeek_ssl, 'server_name', 'ssl server_name', 'zeek_ssl.server_name')\n" +
    "      +arrayList(session.zeek_ssl, 'resumed', 'ssl resumed', 'zeek_ssl.resumed')\n" +
    "      +arrayList(session.zeek_ssl, 'last_alert', 'ssl last_alert', 'zeek_ssl.last_alert')\n" +
    "      +arrayList(session.zeek_ssl, 'next_protocol', 'ssl next_protocol', 'zeek_ssl.next_protocol')\n" +
    "      +arrayList(session.zeek_ssl, 'established', 'ssl established', 'zeek_ssl.established')\n" +
    "      +arrayList(session.zeek_ssl, 'ja3', 'JA3 fingerprint', 'zeek_ssl.ja3')\n" +
    "      +arrayList(session.zeek_ssl, 'ja3_desc', 'JA3 lookup', 'zeek_ssl.ja3_desc')\n" +
    "      +arrayList(session.zeek_ssl, 'ja3s', 'JA3S fingerprint', 'zeek_ssl.ja3s')\n" +
    "      +arrayList(session.zeek_ssl, 'ja3s_desc', 'JA3S lookup', 'zeek_ssl.ja3s_desc')\n" +
    "      +arrayList(session.zeek_ssl, 'cert_chain_fuids', 'ssl cert_chain_fuids', 'zeek_ssl.cert_chain_fuids')\n" +
    "      +arrayList(session.zeek_ssl, 'client_cert_chain_fuids', 'ssl client_cert_chain_fuids', 'zeek_ssl.client_cert_chain_fuids')\n" +
    "      +arrayList(session.zeek_ssl.subject, 'CN', 'ssl subject common name', 'zeek_ssl.subject.CN')\n" +
    "      +arrayList(session.zeek_ssl.subject, 'C', 'ssl subject country', 'zeek_ssl.subject.C')\n" +
    "      +arrayList(session.zeek_ssl.subject, 'O', 'ssl subject organization', 'zeek_ssl.subject.O')\n" +
    "      +arrayList(session.zeek_ssl.subject, 'OU', 'ssl subject organization unit', 'zeek_ssl.subject.OU')\n" +
    "      +arrayList(session.zeek_ssl.subject, 'ST', 'ssl subject state', 'zeek_ssl.subject.ST')\n" +
    "      +arrayList(session.zeek_ssl.subject, 'SN', 'ssl subject surname', 'zeek_ssl.subject.SN')\n" +
    "      +arrayList(session.zeek_ssl.subject, 'L', 'ssl subject locality', 'zeek_ssl.subject.L')\n" +
    "      +arrayList(session.zeek_ssl.subject, 'GN', 'ssl subject given name', 'zeek_ssl.subject.GN')\n" +
    "      +arrayList(session.zeek_ssl.subject, 'pseudonym', 'ssl subject pseudonym', 'zeek_ssl.subject.pseudonym')\n" +
    "      +arrayList(session.zeek_ssl.subject, 'serialNumber', 'ssl subject serial number', 'zeek_ssl.subject.serialNumber')\n" +
    "      +arrayList(session.zeek_ssl.subject, 'title', 'ssl subject title', 'zeek_ssl.subject.title')\n" +
    "      +arrayList(session.zeek_ssl.subject, 'initials', 'ssl subject initials', 'zeek_ssl.subject.initials')\n" +
    "      +arrayList(session.zeek_ssl.subject, 'emailAddress', 'ssl subject email address', 'zeek_ssl.subject.emailAddress')\n" +
    "      +arrayList(session.zeek_ssl.client_subject, 'CN', 'ssl client subject common name', 'zeek_ssl.client_subject.CN')\n" +
    "      +arrayList(session.zeek_ssl.client_subject, 'C', 'ssl client subject country', 'zeek_ssl.client_subject.C')\n" +
    "      +arrayList(session.zeek_ssl.client_subject, 'O', 'ssl client subject organization', 'zeek_ssl.client_subject.O')\n" +
    "      +arrayList(session.zeek_ssl.client_subject, 'OU', 'ssl client subject organization unit', 'zeek_ssl.client_subject.OU')\n" +
    "      +arrayList(session.zeek_ssl.client_subject, 'ST', 'ssl client subject state', 'zeek_ssl.client_subject.ST')\n" +
    "      +arrayList(session.zeek_ssl.client_subject, 'SN', 'ssl client subject surname', 'zeek_ssl.client_subject.SN')\n" +
    "      +arrayList(session.zeek_ssl.client_subject, 'L', 'ssl client subject locality', 'zeek_ssl.client_subject.L')\n" +
    "      +arrayList(session.zeek_ssl.client_subject, 'GN', 'ssl client subject given name', 'zeek_ssl.client_subject.GN')\n" +
    "      +arrayList(session.zeek_ssl.client_subject, 'pseudonym', 'ssl client subject pseudonym', 'zeek_ssl.client_subject.pseudonym')\n" +
    "      +arrayList(session.zeek_ssl.client_subject, 'serialNumber', 'ssl client subject serial number', 'zeek_ssl.client_subject.serialNumber')\n" +
    "      +arrayList(session.zeek_ssl.client_subject, 'title', 'ssl client subject title', 'zeek_ssl.client_subject.title')\n" +
    "      +arrayList(session.zeek_ssl.client_subject, 'initials', 'ssl client subject initials', 'zeek_ssl.client_subject.initials')\n" +
    "      +arrayList(session.zeek_ssl.client_subject, 'emailAddress', 'ssl client subject email address', 'zeek_ssl.client_subject.emailAddress')\n" +
    "      +arrayList(session.zeek_ssl.issuer, 'CN', 'ssl issuer common name', 'zeek_ssl.issuer.CN')\n" +
    "      +arrayList(session.zeek_ssl.issuer, 'C', 'ssl issuer country', 'zeek_ssl.issuer.C')\n" +
    "      +arrayList(session.zeek_ssl.issuer, 'O', 'ssl issuer organization', 'zeek_ssl.issuer.O')\n" +
    "      +arrayList(session.zeek_ssl.issuer, 'OU', 'ssl issuer organization unit', 'zeek_ssl.issuer.OU')\n" +
    "      +arrayList(session.zeek_ssl.issuer, 'ST', 'ssl issuer state', 'zeek_ssl.issuer.ST')\n" +
    "      +arrayList(session.zeek_ssl.issuer, 'SN', 'ssl issuer surname', 'zeek_ssl.issuer.SN')\n" +
    "      +arrayList(session.zeek_ssl.issuer, 'L', 'ssl issuer locality', 'zeek_ssl.issuer.L')\n" +
    "      +arrayList(session.zeek_ssl.issuer, 'DC', 'ssl issuer distinguished name', 'zeek_ssl.issuer.DC')\n" +
    "      +arrayList(session.zeek_ssl.issuer, 'GN', 'ssl issuer given name', 'zeek_ssl.issuer.GN')\n" +
    "      +arrayList(session.zeek_ssl.issuer, 'pseudonym', 'ssl issuer pseudonym', 'zeek_ssl.issuer.pseudonym')\n" +
    "      +arrayList(session.zeek_ssl.issuer, 'serialNumber', 'ssl issuer serial number', 'zeek_ssl.issuer.serialNumber')\n" +
    "      +arrayList(session.zeek_ssl.issuer, 'title', 'ssl issuer title', 'zeek_ssl.issuer.title')\n" +
    "      +arrayList(session.zeek_ssl.issuer, 'initials', 'ssl issuer initials', 'zeek_ssl.issuer.initials')\n" +
    "      +arrayList(session.zeek_ssl.issuer, 'emailAddress', 'ssl issuer email address', 'zeek_ssl.issuer.emailAddress')\n" +
    "      +arrayList(session.zeek_ssl.client_issuer, 'CN', 'ssl client issuer common name', 'zeek_ssl.client_issuer.CN')\n" +
    "      +arrayList(session.zeek_ssl.client_issuer, 'C', 'ssl client issuer country', 'zeek_ssl.client_issuer.C')\n" +
    "      +arrayList(session.zeek_ssl.client_issuer, 'O', 'ssl client issuer organization', 'zeek_ssl.client_issuer.O')\n" +
    "      +arrayList(session.zeek_ssl.client_issuer, 'OU', 'ssl client issuer organization unit', 'zeek_ssl.client_issuer.OU')\n" +
    "      +arrayList(session.zeek_ssl.client_issuer, 'ST', 'ssl client issuer state', 'zeek_ssl.client_issuer.ST')\n" +
    "      +arrayList(session.zeek_ssl.client_issuer, 'SN', 'ssl client issuer surname', 'zeek_ssl.client_issuer.SN')\n" +
    "      +arrayList(session.zeek_ssl.client_issuer, 'L', 'ssl client issuer locality', 'zeek_ssl.client_issuer.L')\n" +
    "      +arrayList(session.zeek_ssl.client_issuer, 'DC', 'ssl client issuer distinguished name', 'zeek_ssl.client_issuer.DC')\n" +
    "      +arrayList(session.zeek_ssl.client_issuer, 'GN', 'ssl client issuer given name', 'zeek_ssl.client_issuer.GN')\n" +
    "      +arrayList(session.zeek_ssl.client_issuer, 'pseudonym', 'ssl client issuer pseudonym', 'zeek_ssl.client_issuer.pseudonym')\n" +
    "      +arrayList(session.zeek_ssl.client_issuer, 'serialNumber', 'ssl client issuer serial number', 'zeek_ssl.client_issuer.serialNumber')\n" +
    "      +arrayList(session.zeek_ssl.client_issuer, 'title', 'ssl client issuer title', 'zeek_ssl.client_issuer.title')\n" +
    "      +arrayList(session.zeek_ssl.client_issuer, 'initials', 'ssl client issuer initials', 'zeek_ssl.client_issuer.initials')\n" +
    "      +arrayList(session.zeek_ssl.client_issuer, 'emailAddress', 'ssl client issuer email address', 'zeek_ssl.client_issuer.emailAddress')\n" +
    "      +arrayList(session.zeek_ssl, 'validation_status', 'ssl validation_status', 'zeek_ssl.validation_status')\n" +

    // syslog.log
    "  if (session.zeek_syslog)\n" +
    "    dl.sessionDetailMeta(suffix=\"syslog.log\")\n" +
    "      +arrayList(session.zeek_syslog, 'facility', 'syslog facility', 'zeek_syslog.facility')\n" +
    "      +arrayList(session.zeek_syslog, 'severity', 'syslog severity', 'zeek_syslog.severity')\n" +
    "      +arrayList(session.zeek_syslog, 'message', 'syslog message', 'zeek_syslog.message')\n" +

    // tunnel.log
    "  if (session.zeek_tunnel)\n" +
    "    dl.sessionDetailMeta(suffix=\"tunnel.log\")\n" +
    "      +arrayList(session.zeek_tunnel, 'tunnel_type', 'tunnel tunnel_type', 'zeek_tunnel.tunnel_type')\n" +
    "      +arrayList(session.zeek_tunnel, 'action', 'tunnel action', 'zeek_tunnel.action')\n" +

    // weird.log
    "  if (session.zeek_weird)\n" +
    "    dl.sessionDetailMeta(suffix=\"weird.log\")\n" +
    "      +arrayList(session.zeek_weird, 'name', 'weird name', 'zeek_weird.name')\n" +
    "      +arrayList(session.zeek_weird, 'addl', 'weird addl', 'zeek_weird.addl')\n" +
    "      +arrayList(session.zeek_weird, 'notice', 'weird notice', 'zeek_weird.notice')\n" +
    "      +arrayList(session.zeek_weird, 'peer', 'weird peer', 'zeek_weird.peer')\n" +

    // x509.log
    "  if (session.zeek_x509)\n" +
    "    dl.sessionDetailMeta(suffix=\"x509.log\")\n" +
    "      +arrayList(session.zeek_x509, 'fuid', 'x509 fuid', 'zeek_x509.fuid')\n" +
    "      +arrayList(session.zeek_x509, 'certificate_version', 'x509 certificate_version', 'zeek_x509.certificate_version')\n" +
    "      +arrayList(session.zeek_x509, 'certificate_serial', 'x509 certificate_serial', 'zeek_x509.certificate_serial')\n" +
    "      +arrayList(session.zeek_x509.certificate_subject, 'CN', 'x509 certificate subject common name', 'zeek_x509.certificate_subject.CN')\n" +
    "      +arrayList(session.zeek_x509.certificate_subject, 'C', 'x509 certificate subject country', 'zeek_x509.certificate_subject.C')\n" +
    "      +arrayList(session.zeek_x509.certificate_subject, 'O', 'x509 certificate subject organization', 'zeek_x509.certificate_subject.O')\n" +
    "      +arrayList(session.zeek_x509.certificate_subject, 'OU', 'x509 certificate subject organization unit', 'zeek_x509.certificate_subject.OU')\n" +
    "      +arrayList(session.zeek_x509.certificate_subject, 'ST', 'x509 certificate subject state', 'zeek_x509.certificate_subject.ST')\n" +
    "      +arrayList(session.zeek_x509.certificate_subject, 'SN', 'x509 certificate subject surname', 'zeek_x509.certificate_subject.SN')\n" +
    "      +arrayList(session.zeek_x509.certificate_subject, 'L', 'x509 certificate subject locality', 'zeek_x509.certificate_subject.L')\n" +
    "      +arrayList(session.zeek_x509.certificate_subject, 'DC', 'x509 certificate subject distinguished name', 'zeek_x509.certificate_subject.DC')\n" +
    "      +arrayList(session.zeek_x509.certificate_subject, 'GN', 'x509 certificate subject given name', 'zeek_x509.certificate_subject.GN')\n" +
    "      +arrayList(session.zeek_x509.certificate_subject, 'pseudonym', 'x509 certificate subject pseudonym', 'zeek_x509.certificate_subject.pseudonym')\n" +
    "      +arrayList(session.zeek_x509.certificate_subject, 'serialNumber', 'x509 certificate subject serial number', 'zeek_x509.certificate_subject.serialNumber')\n" +
    "      +arrayList(session.zeek_x509.certificate_subject, 'title', 'x509 certificate subject title', 'zeek_x509.certificate_subject.title')\n" +
    "      +arrayList(session.zeek_x509.certificate_subject, 'initials', 'x509 certificate subject initials', 'zeek_x509.certificate_subject.initials')\n" +
    "      +arrayList(session.zeek_x509.certificate_subject, 'emailAddress', 'x509 certificate subject email address', 'zeek_x509.certificate_subject.emailAddress')\n" +
    "      +arrayList(session.zeek_x509.certificate_issuer, 'CN', 'x509 certificate issuer common name', 'zeek_x509.certificate_issuer.CN')\n" +
    "      +arrayList(session.zeek_x509.certificate_issuer, 'C', 'x509 certificate issuer country', 'zeek_x509.certificate_issuer.C')\n" +
    "      +arrayList(session.zeek_x509.certificate_issuer, 'O', 'x509 certificate issuer organization', 'zeek_x509.certificate_issuer.O')\n" +
    "      +arrayList(session.zeek_x509.certificate_issuer, 'OU', 'x509 certificate issuer organization unit', 'zeek_x509.certificate_issuer.OU')\n" +
    "      +arrayList(session.zeek_x509.certificate_issuer, 'ST', 'x509 certificate issuer state', 'zeek_x509.certificate_issuer.ST')\n" +
    "      +arrayList(session.zeek_x509.certificate_issuer, 'SN', 'x509 certificate issuer surname', 'zeek_x509.certificate_issuer.SN')\n" +
    "      +arrayList(session.zeek_x509.certificate_issuer, 'L', 'x509 certificate issuer locality', 'zeek_x509.certificate_issuer.L')\n" +
    "      +arrayList(session.zeek_x509.certificate_issuer, 'GN', 'x509 certificate issuer given name', 'zeek_x509.certificate_issuer.GN')\n" +
    "      +arrayList(session.zeek_x509.certificate_issuer, 'pseudonym', 'x509 certificate issuer pseudonym', 'zeek_x509.certificate_issuer.pseudonym')\n" +
    "      +arrayList(session.zeek_x509.certificate_issuer, 'serialNumber', 'x509 certificate issuer serial number', 'zeek_x509.certificate_issuer.serialNumber')\n" +
    "      +arrayList(session.zeek_x509.certificate_issuer, 'title', 'x509 certificate issuer title', 'zeek_x509.certificate_issuer.title')\n" +
    "      +arrayList(session.zeek_x509.certificate_issuer, 'initials', 'x509 certificate issuer initials', 'zeek_x509.certificate_issuer.initials')\n" +
    "      +arrayList(session.zeek_x509.certificate_issuer, 'emailAddress', 'x509 certificate issuer email address', 'zeek_x509.certificate_issuer.emailAddress')\n" +
    "      +arrayList(session.zeek_x509, 'certificate_not_valid_before', 'x509 certificate_not_valid_before', 'zeek_x509.certificate_not_valid_before')\n" +
    "      +arrayList(session.zeek_x509, 'certificate_not_valid_after', 'x509 certificate_not_valid_after', 'zeek_x509.certificate_not_valid_after')\n" +
    "      +arrayList(session.zeek_x509, 'certificate_key_alg', 'x509 certificate_key_alg', 'zeek_x509.certificate_key_alg')\n" +
    "      +arrayList(session.zeek_x509, 'certificate_sig_alg', 'x509 certificate_sig_alg', 'zeek_x509.certificate_sig_alg')\n" +
    "      +arrayList(session.zeek_x509, 'certificate_key_type', 'x509 certificate_key_type', 'zeek_x509.certificate_key_type')\n" +
    "      +arrayList(session.zeek_x509, 'certificate_key_length', 'x509 certificate_key_length', 'zeek_x509.certificate_key_length')\n" +
    "      +arrayList(session.zeek_x509, 'certificate_exponent', 'x509 certificate_exponent', 'zeek_x509.certificate_exponent')\n" +
    "      +arrayList(session.zeek_x509, 'certificate_curve', 'x509 certificate_curve', 'zeek_x509.certificate_curve')\n" +
    "      +arrayList(session.zeek_x509, 'san_dns', 'x509 san_dns', 'zeek_x509.san_dns')\n" +
    "      +arrayList(session.zeek_x509, 'san_uri', 'x509 san_uri', 'zeek_x509.san_uri')\n" +
    "      +arrayList(session.zeek_x509, 'san_email', 'x509 san_email', 'zeek_x509.san_email')\n" +
    "      +arrayList(session.zeek_x509, 'san_ip', 'x509 san_ip', 'zeek_x509.san_ip')\n" +
    "      +arrayList(session.zeek_x509, 'basic_constraints_ca', 'x509 basic_constraints_ca', 'zeek_x509.basic_constraints_ca')\n" +
    "      +arrayList(session.zeek_x509, 'basic_constraints_path_len', 'x509 basic_constraints_path_len', 'zeek_x509.basic_constraints_path_len')\n" +

    // ####################################################################
    "  br\n");

  // Add the source as available
  this.api.addSource("zeek", this);
}
util.inherits(ZeekLogs, wiseSource);

ZeekLogs.prototype.load = function() {
  var self = this;
  this.data.clear();
}

ZeekLogs.prototype.getDomain = function(domain, cb) {
};

ZeekLogs.prototype.getIp = function(ip, cb) {
};

ZeekLogs.prototype.getMd5 = function(md5, cb) {
};

ZeekLogs.prototype.getEmail = function(email, cb) {
};

exports.initSource = function(api) {
  var source = new ZeekLogs(api, "zeek");
};
