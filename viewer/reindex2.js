#!/data/moloch/bin/node --max_old_space_size=2000
/******************************************************************************/
/* reindex2.js  -- Reindex all the sessions indices to sessions2, which will
 *                 * rename most fields
 *                 * remove duplicate fields
 *                 * remove all tokenized fields
 *                 * switch to ip and date ES fields
 *
 * Create a mapping of private/unknown fields in a reindex2.local.json file
 *
 * Copyright 2017 AOL Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
'use strict';

try {
var util           = require('util'),
    fs             = require('fs-ext'),
    async          = require('async'),
    Db             = require('./db.js'),
    cluster        = require('cluster'),
    os             = require('os');
} catch (e) {
  console.log ("ERROR - Couldn't load some dependancies, maybe need to 'npm update' inside viewer directory", e);
  process.exit(1);
  throw new Error("Exiting");
}

var MIN_DB_VERSION = 38;

//////////////////////////////////////////////////////////////////////////////////
//// Config
//////////////////////////////////////////////////////////////////////////////////
var internals = {
  sliceMax: 5,
  sliceNum: 0,
  scrollSize: 500,
  scrollPause: 0,
  index: "sessions-*",
  stats: {},
  statsTime: {},
  deleteExisting: false,
  deleteOnDone: false,
};

var fieldsMap = {
  "_id":              "ignore",
  "fileand":          "ignore", // special
  "viewand":          "ignore", // special
  "ipall":            "ignore", // special
  "tipv61-term":      "ignore", // Becomes srcIp
  "tipv62-term":      "ignore", // Becomes dstIp
  "ta":               "ignore", // Becomes tags
  "fp":               "ignore", // Becomes firstPacket when lpd not set
  "lp":               "ignore", // Becomes lastPacket when lpd not set
  "lpd":              "ignore", // Becomes firstPacket
  "fpd":              "ignore", // Becomes lastPacket
  "a1":               "ignore", // Becomes srcIp
  "a2":               "ignore", // Becomes dstIp

  "ps":               "packetPos",
  "psl":              "packetLen",
  "fs":               "fileId",
  "timestamp":        "timestamp",
  "firstPacket":      "firstPacket",
  "lastPacket":       "lastPacket",

  "ro":               "rootId",
  "no":               "node",
  "ss":               "segmentCnt",
  "user":             "user",
  "usercnt":          "userCnt",
  "sl":               "length",
  "tags-term":        "tags",
  "tacnt":            "tagsCnt",
  "asset-term":       "asset",
  "asset-term-cnt":   "assetCnt",
  "prot-term":        "protocol",
  "prot-term-cnt":    "protocolCnt",

  "vlan":             "vlan",
  "vlan-cnt":         "vlanCnt",


  "ipSrc":            "srcIp",
  "p1":               "srcPort",
  "portSrc":          "srcPort",
  "as1":              "srcASN",
  "g1":               "srcGEO",
  "rir1":             "srcRIR",
  "by1":              "srcBytes",
  "db1":              "srcDataBytes",
  "pa1":              "srcPackets",
  "fb1":              "srcPayload8",
  "mac1-term":        "srcMac",
  "mac1-term-cnt":    "srcMacCnt",

  "ipDst":            "dstIp",
  "p2":               "dstPort",
  "portDst":          "dstPort",
  "as2":              "dstASN",
  "g2":               "dstGEO",
  "rir2":             "dstRIR",
  "by2":              "dstBytes",
  "db2":              "dstDataBytes",
  "pa2":              "dstPackets",
  "fb2":              "dstPayload8",
  "mac2-term":        "dstMac",
  "mac2-term-cnt":    "dstMacCnt",

  "by":               "totBytes",
  "db":               "totDataBytes",
  "pa":               "totPackets",
  "pr":               "ipProtocol",

  "scrubby":          "scrubby",

//GRE
  "greip":            "greIp",
  "greip-cnt":        "greIpCnt",
  "greip-asn":        "greASN",
  "greip-geo":        "greGEO",
  "greip-rir":        "greRIR",

//TCP FLAGS
  "tcpflags":          "tcpflags",
  "tcpflags.syn-ack":  "tcpflags.syn-ack",
  "tcpflags.ack":      "tcpflags.ack",
  "tcpflags.fin":      "tcpflags.fin",
  "tcpflags.syn":      "tcpflags.syn",
  "tcpflags.psh":      "tcpflags.psh",
  "tcpflags.rst":      "tcpflags.rst",
  "tcpflags.urg":      "tcpflags.urg",

//HTTP
  "ho":                             "http.host",
  "hocnt":                          "http.hostCnt",
  "us":                             "http.uri",
  "uscnt":                          "http.uriCnt",
  "hpath":                          "http.path",
  "hpathcnt":                       "http.pathCnt",
  "hkey":                           "http.key",
  "hkeycnt":                        "http.keyCnt",
  "hval":                           "http.value",
  "hvalcnt":                        "http.valueCnt",
  "hckey-term":                     "http.cookieKey",
  "hckey-term-cnt":                 "http.cookieKeyCnt",
  "hcval-term":                     "http.cookieValue",
  "hcval-term-cnt":                 "http.cookieValueCnt",
  "http.method-term":               "http.method",
  "http.method-term-cnt":           "http.methodCnt",
  "hat-term":                       "http.authType",
  "hat-termcnt":                    "http.authTypeCnt",
  "ua":                             "http.useragent",
  "uacnt":                          "http.useragentCnt",
  "huser-termcnt":                  "http.userCnt",
  "huser-term":                     "http.user",
  "http.bodymagic-term":            "http.bodyMagic",
  "http.bodymagic-term-cnt":        "http.bodyMagicCnt",
  "http.statuscode":                "http.statuscode",
  "http.statuscode-cnt":            "http.statuscodeCnt",
  "xff":                            "http.xffIp",
  "xffscnt":                        "http.xffIpCnt",
  "gxff":                           "http.xffGEO",
  "asxff":                          "http.xffASN",
  "rirxff":                         "http.xffRIR",
  "hh1":                            "ignore", // http.requestHeader
  "hh1cnt":                         "ignore", // http.requestHeaderCnt
  "hh2":                            "ignore", // http.responseHeader
  "hh2cnt":                         "ignore", // http.responseHeaderCnt
  "hmd5":                           "http.md5",
  "hmd5cnt":                        "http.md5Cnt",
  "hsver":                          "http.clientVersion",
  "hsvercnt":                       "http.cilentVersionCnt",
  "hdver":                          "http.serverVersion",
  "hdvercnt":                       "http.serverVersionCnt",
  "rqbd-term":                      "http.requestBody",

//QUIC
  "quic.host-term":         "quic.host",
  "quic.host-termcnt":      "quic.hostCnt",
  "quic.ua-term":           "quic.useragent",
  "quic.ua-termcnt":        "quic.useragentCnt",
  "quic.version-term":      "quic.version",
  "quic.version-termcnt":   "quic.versionCnt",

//EMAIL
  "esrc":                           "email.src",
  "esrccnt":                        "email.srcCnt",
  "edst":                           "email.dst",
  "edstcnt":                        "email.dstCnt",
  "esub":                           "email.subject",
  "esubcnt":                        "email.subjectCnt",
  "ect":                            "email.contentType",
  "ectcnt":                         "email.contentTypeCnt",
  "emd5":                           "email.md5",
  "emd5cnt":                        "email.md5Cnt",
  "eip":                            "email.ip",
  "eipcnt":                         "email.ipCnt",
  "rireip":                         "email.RIR",
  "aseip":                          "email.ASN",
  "geip":                           "email.GEO",
  "eho":                            "email.host",
  "ehocnt":                         "email.hostCnt",
  "eua":                            "email.useragent",
  "euacnt":                         "email.useragentCnt",
  "eid":                            "email.id",
  "eidcnt":                         "email.idCnt",
  "emv":                            "email.mimeVersion",
  "emvcnt":                         "email.mimeVersionCnt",
  "efn":                            "email.filename",
  "efncnt":                         "email.filenameCnt",
  "efct":                           "email.fileContentType",
  "efctcnt":                        "email.fileContentTypeCnt",
  "ehh":                            "ignore", // email.header
  "ehhcnt":                         "ignore", // email.headerCnt
  "email.bodymagic-term":           "email.bodyMagic",
  "email.bodymagic-term-cnt":       "email.bodyMagicCnt",

//IRC
  "ircnck":            "irc.nick",
  "ircnckcnt":         "irc.nickCnt",
  "ircch":             "irc.channel",
  "ircchcnt":          "irc.channelCnt",

//KRB5
  "krb5.realm-term":      "krb5.realm",
  "krb5.realm-termcnt":   "krb5.realmCnt",
  "krb5.cname-term":      "krb5.cname",
  "krb5.cname-termcnt":   "krb5.cnameCnt",
  "krb5.sname-term":      "krb5.sname",
  "krb5.sname-termcnt":   "krb5.snameCnt",

//LDAP
  "ldap.authtype-term":      "ldap.authtype",
  "ldap.authtype-term-cnt":  "ldap.authtypeCnt",
  "ldap.bindname-term":      "ldap.bindname",
  "ldap.bindname-term-cnt":  "ldap.bindnameCnt",

//MYSQL
  "mysql.user-term":      "mysql.user",
  "mysql.ver-term":       "mysql.version",

//ORACLE
  "oracle.user-term":      "oracle.user",
  "oracle.host-term":      "oracle.host",
  "oracle.service-term":   "oracle.service",

//POSTGRES
  "postgresql.user-term":      "postgresql.user",
  "postgresql.db-term":        "postgresql.db",
  "postgresql.app-term":       "postgresql.app",

//RADIUS
  "radius.user-term":          "radius.user",
  "radius.mac-term":           "radius.mac",
  "radius.mac-term-cnt":       "radius.macCnt",
  "radius.eip":                "radius.endpointIp",
  "radius.eip-cnt":            "radius.endpointIpCnt",
  "radius.eip-geo":            "radius.endpointGEO",
  "radius.eip-rir":            "radius.endpointRIR",
  "radius.eip-asn":            "radius.endpointASN",
  "radius.fip":                "radius.framedIp",
  "radius.fip-cnt":            "radius.framedIpCnt",
  "radius.fip-geo":            "radius.framedGEO",
  "radius.fip-rir":            "radius.framedRIR",
  "radius.fip-asn":            "radius.framedASN",

//SMB
  "smbsh":                     "smb.share",
  "smbshcnt":                  "smb.shareCnt",
  "smbos":                     "smb.os",
  "smboscnt":                  "smb.osCnt",
  "smbdm":                     "smb.domain",
  "smbdmcnt":                  "smb.domainCnt",
  "smbver":                    "smb.version",
  "smbvercnt":                 "smb.versionCnt",
  "smbuser":                   "smb.user",
  "smbusercnt":                "smb.userCnt",
  "smbho":                     "smb.host",
  "smbhocnt":                  "smb.hostCnt",
  "smbfn":                     "smb.filename",
  "smbfncnt":                  "smb.filenameCnt",

//SOCKS
  "socksip":                   "socks.ip",
  "gsocksip":                  "socks.GEO",
  "assocksip":                 "socks.ASN",
  "rirsocksip":                "socks.RIR",
  "socksho":                   "socks.host",
  "sockspo":                   "socks.port",
  "socksuser":                 "socks.user",

//SSH
  "sshver":                    "ssh.version",
  "sshvercnt":                 "ssh.versionCnt",
  "sshkey":                    "ssh.key",
  "sshkeycnt":                 "ssh.keyCnt",

//DNS
  "dnsip":                     "dns.ip",
  "dnsipcnt":                  "dns.ipCnt",
  "gdnsip":                    "dns.GEO",
  "asdnsip":                   "dns.ASN",
  "rirdnsip":                  "dns.RIR",
  "dnsho":                     "dns.host",
  "dnshocnt":                  "dns.hostCnt",
  "dns.status-term":           "dns.status",
  "dns.status-term-cnt":       "dns.statusCnt",
  "dns.opcode-term":           "dns.opcode",
  "dns.opcode-term-cnt":       "dns.opcodeCnt",
  "dns.qt-term":               "dns.qt",
  "dns.qt-term-cnt":           "dns.qtCnt",
  "dns.qc-term":               "dns.qc",
  "dns.qc-term-cnt":           "dns.qcCnt",

//CERT
  "tlscnt":                    "certCnt",
  "tls.alt":                   "cert.alt",
  "tls.altcnt":                "cert.altCnt",
  "tls.hash":                  "cert.hash",
  "tls.notBefore":             "cert.notBefore",
  "tls.notAfter":              "cert.notAfter",
  "tls.diffDays":              "cert.validDays",
  "tls.sn":                    "cert.serial",
  "tls.iCn":                   "cert.issuerCN",
  "tls.sCn":                   "cert.subjectCN",
  "tls.iOn":                   "cert.issuerON",
  "tls.sOn":                   "cert.subjectON",

//TLS
  "tlsver-term":               "tls.version",
  "tlsver-termcnt":            "tls.versionCnt",
  "tlscipher-term":            "tls.cipher",
  "tlscipher-termcnt":         "tls.cipherCnt",
  "tlsja3-term":               "tls.ja3",
  "tlsja3-termcnt":            "tls.ja3Cnt",
  "ja3comment-term":           "tls.ja3Comment",
  "tlsdstid-term":             "tls.dstSessionId",
  "tlssrcid-term":             "tls.srcSessionId",

//ICMP
  "icmpCode":                  "icmp.code",
  "icmpType":                  "icmp.type",

//THREATSTREAM
  "threatstream.maltype-term":        "threatstream.maltype",
  "threatstream.maltype-term-cnt":    "threatstream.maltypeCnt",
  "threatstream.type-term":           "threatstream.type",
  "threatstream.type-term-cnt":       "threatstream.typeCnt",
  "threatstream.id":                  "threatstream.id",
  "threatstream.id-cnt":              "threatstream.idCnt",
  "threatstream.confidence":          "threatstream.confidence",
  "threatstream.confidence-cnt":      "threatstream.confidenceCnt",
  "threatstream.importId":            "threatstream.importId",
  "threatstream.importId-cnt":        "threatstream.importIdCnt",
  "threatstream.sessionid":           "ignore",
  "threatstream.sessionid-cnt":       "ignore",
  "threatstream.severity-term":       "threatstream.severity",
  "threatstream.severity-term-cnt":   "threatstream.severityCnt",
  "threatstream.source-term":         "threatstream.source",
  "threatstream.source-term-cnt":     "threatstream.sourceCnt",

//VIRUSTOTAL
  "virustotal.kaspersky-term":      "virustotal.kasperksky",
  "virustotal.kaspersky-term-cnt":  "virustotal.kasperkskyCnt",
  "virustotal.mcafee-term":         "virustotal.mcafee",
  "virustotal.mcafee-term-cnt":     "virustotal.mcafeeCnt",
  "virustotal.symantec-term":       "virustotal.symantec",
  "virustotal.symantec-term-cnt":   "virustotal.symantecCnt",
  "virustotal.microsoft-term":      "virustotal.microsoft",
  "virustotal.microsoft-term-cnt":  "virustotal.microsoftCnt",
  "virustotal.links-term":          "virustotal.links",
  "virustotal.links-term-cnt":      "virustotal.linksCnt",
  "virustotal.hits":                "virustotal.hits",
  "virustotal.hits-cnt":            "virustotal.hitsCnt",

//THREATQ
  "threatq.camapign-term":          "ignore", // spelled wrong
  "threatq.camapign-term-cnt":      "ignore", // spelled wrong
  "threatq.campaign-term":          "threatq.campaign",
  "threatq.campaign-term-cnt":      "threatq.campaign",
  "threatq.id":                     "threatq.id",
  "threatq.id-cnt":                 "threatq.idCnt",
  "threatq.source-term":            "threatq.source",
  "threatq.source-term-cnt":        "threatq.sourceCnt",
  "threatq.type-term":              "threatq.type",
  "threatq.type-term-cnt":          "threatq.typeCnt",

//ALIENVAULT
  "alienvault.threatlevel":         "alienvault.threatlevel",
  "alienvault.threatlevel-cnt":     "alienvault.threatlevelCnt",
  "alienvault.reliability":         "alienvault.reliability",
  "alienvault.reliability-cnt":     "alienvault.reliabilityCnt",
  "alienvault.activity-term":       "alienvault.activity",
  "alienvault.activity-term-cnt":   "alienvault.activityCnt",
  "alienvault.id":                  "alienvault.id",
  "alienvault.id-cnt":              "alienvault.idCnt",

//ET
  "et.score":                       "et.score",
  "et.score-cnt":                   "et.scoreCnt",
  "et.category-term":               "et.category",
  "et.category-term-cnt":           "et.categoryCnt",

//OPENDNS
  "opendns.dmstatus-term":          "opendns.status",
  "opendns.dmstatus-term-cnt":      "opendns.statusCnt",
  "opendns.dmccat-term":            "opendns.contentCategory",
  "opendns.dmccat-term-cnt":        "opendns.contentCategoryCnt",
  "opendns.dmscat-term":            "opendns.securityCategory",
  "opendns.dmscat-term-cnt":        "opendns.securityCategoryCnt",

//PASSIVETOTAL
  "passivetotal.tags-term":         "passivetotal.tags",
  "passivetotal.tags-term-cnt":     "passivetotal.tagsCnt",
};

var typesMap = {
  "tls.iOn":                   "termfield",
  "tls.sOn":                   "termfield",
  "eua":                       "termfield",
  "tls.notBefore":             "date",
  "tls.notAfter":              "date",
};

var countryMap = {
  "AFG": "AF",
  "ALA": "AX",
  "ALB": "AL",
  "DZA": "DZ",
  "ASM": "AS",
  "AND": "AD",
  "AGO": "AO",
  "AIA": "AI",
  "ATA": "AQ",
  "ATG": "AG",
  "ARG": "AR",
  "ARM": "AM",
  "ABW": "AW",
  "AUS": "AU",
  "AUT": "AT",
  "AZE": "AZ",
  "BHS": "BS",
  "BHR": "BH",
  "BGD": "BD",
  "BRB": "BB",
  "BLR": "BY",
  "BEL": "BE",
  "BLZ": "BZ",
  "BEN": "BJ",
  "BMU": "BM",
  "BTN": "BT",
  "BOL": "BO",
  "BES": "BQ",
  "BIH": "BA",
  "BWA": "BW",
  "BVT": "BV",
  "BRA": "BR",
  "IOT": "IO",
  "BRN": "BN",
  "BGR": "BG",
  "BFA": "BF",
  "BDI": "BI",
  "CPV": "CV",
  "KHM": "KH",
  "CMR": "CM",
  "CAN": "CA",
  "CYM": "KY",
  "CAF": "CF",
  "TCD": "TD",
  "CHL": "CL",
  "CHN": "CN",
  "CXR": "CX",
  "CCK": "CC",
  "COL": "CO",
  "COM": "KM",
  "COG": "CG",
  "COD": "CD",
  "COK": "CK",
  "CRI": "CR",
  "CIV": "CI",
  "HRV": "HR",
  "CUB": "CU",
  "CUW": "CW",
  "CYP": "CY",
  "CZE": "CZ",
  "DNK": "DK",
  "DJI": "DJ",
  "DMA": "DM",
  "DOM": "DO",
  "ECU": "EC",
  "EGY": "EG",
  "SLV": "SV",
  "GNQ": "GQ",
  "ERI": "ER",
  "EST": "EE",
  "ETH": "ET",
  "FLK": "FK",
  "FRO": "FO",
  "FJI": "FJ",
  "FIN": "FI",
  "FRA": "FR",
  "GUF": "GF",
  "PYF": "PF",
  "ATF": "TF",
  "GAB": "GA",
  "GMB": "GM",
  "GEO": "GE",
  "DEU": "DE",
  "GHA": "GH",
  "GIB": "GI",
  "GRC": "GR",
  "GRL": "GL",
  "GRD": "GD",
  "GLP": "GP",
  "GUM": "GU",
  "GTM": "GT",
  "GGY": "GG",
  "GIN": "GN",
  "GNB": "GW",
  "GUY": "GY",
  "HTI": "HT",
  "HMD": "HM",
  "VAT": "VA",
  "HND": "HN",
  "HKG": "HK",
  "HUN": "HU",
  "ISL": "IS",
  "IND": "IN",
  "IDN": "ID",
  "IRN": "IR",
  "IRQ": "IQ",
  "IRL": "IE",
  "IMN": "IM",
  "ISR": "IL",
  "ITA": "IT",
  "JAM": "JM",
  "JPN": "JP",
  "JEY": "JE",
  "JOR": "JO",
  "KAZ": "KZ",
  "KEN": "KE",
  "KIR": "KI",
  "PRK": "KP",
  "KOR": "KR",
  "KWT": "KW",
  "KGZ": "KG",
  "LAO": "LA",
  "LVA": "LV",
  "LBN": "LB",
  "LSO": "LS",
  "LBR": "LR",
  "LBY": "LY",
  "LIE": "LI",
  "LTU": "LT",
  "LUX": "LU",
  "MAC": "MO",
  "MKD": "MK",
  "MDG": "MG",
  "MWI": "MW",
  "MYS": "MY",
  "MDV": "MV",
  "MLI": "ML",
  "MLT": "MT",
  "MHL": "MH",
  "MTQ": "MQ",
  "MRT": "MR",
  "MUS": "MU",
  "MYT": "YT",
  "MEX": "MX",
  "FSM": "FM",
  "MDA": "MD",
  "MCO": "MC",
  "MNG": "MN",
  "MNE": "ME",
  "MSR": "MS",
  "MAR": "MA",
  "MOZ": "MZ",
  "MMR": "MM",
  "NAM": "NA",
  "NRU": "NR",
  "NPL": "NP",
  "NLD": "NL",
  "NCL": "NC",
  "NZL": "NZ",
  "NIC": "NI",
  "NER": "NE",
  "NGA": "NG",
  "NIU": "NU",
  "NFK": "NF",
  "MNP": "MP",
  "NOR": "NO",
  "OMN": "OM",
  "PAK": "PK",
  "PLW": "PW",
  "PSE": "PS",
  "PAN": "PA",
  "PNG": "PG",
  "PRY": "PY",
  "PER": "PE",
  "PHL": "PH",
  "PCN": "PN",
  "POL": "PL",
  "PRT": "PT",
  "PRI": "PR",
  "QAT": "QA",
  "REU": "RE",
  "ROU": "RO",
  "RUS": "RU",
  "RWA": "RW",
  "BLM": "BL",
  "SHN": "SH",
  "KNA": "KN",
  "LCA": "LC",
  "MAF": "MF",
  "SPM": "PM",
  "VCT": "VC",
  "WSM": "WS",
  "SMR": "SM",
  "STP": "ST",
  "SAU": "SA",
  "SEN": "SN",
  "SRB": "RS",
  "SYC": "SC",
  "SLE": "SL",
  "SGP": "SG",
  "SXM": "SX",
  "SVK": "SK",
  "SVN": "SI",
  "SLB": "SB",
  "SOM": "SO",
  "ZAF": "ZA",
  "SGS": "GS",
  "SSD": "SS",
  "ESP": "ES",
  "LKA": "LK",
  "SDN": "SD",
  "SUR": "SR",
  "SJM": "SJ",
  "SWZ": "SZ",
  "SWE": "SE",
  "CHE": "CH",
  "SYR": "SY",
  "TWN": "TW",
  "TJK": "TJ",
  "TZA": "TZ",
  "THA": "TH",
  "TLS": "TL",
  "TGO": "TG",
  "TKL": "TK",
  "TON": "TO",
  "TTO": "TT",
  "TUN": "TN",
  "TUR": "TR",
  "TKM": "TM",
  "TCA": "TC",
  "TUV": "TV",
  "UGA": "UG",
  "UKR": "UA",
  "ARE": "AE",
  "GBR": "GB",
  "USA": "US",
  "UMI": "UM",
  "URY": "UY",
  "UZB": "UZ",
  "VUT": "VU",
  "VEN": "VE",
  "VNM": "VN",
  "VGB": "VG",
  "VIR": "VI",
  "WLF": "WF",
  "ESH": "EH",
  "YEM": "YE",
  "ZMB": "ZM",
  "ZWE": "ZW",
  "---": "--"
};

//////////////////////////////////////////////////////////////////////////////////
function ipString(ip) {
  if (Array.isArray(ip)) {
    return ip.map(ipString);
  }

  return (ip>>24 & 0xff) + '.' + (ip>>16 & 0xff) + '.' + (ip>>8 & 0xff) + '.' + (ip & 0xff);
}

//////////////////////////////////////////////////////////////////////////////////
function setField(result, path, value) {
  for (var i = 0, ilen = path.length -1; i < ilen; i++) {
    if (result[path[i]] === undefined) {
      result[path[i]] = {};
    }
    result = result[path[i]];
  }

  if ((path[ilen] === "ip" || path[ilen].endsWith("Ip")) && typeof(value) !== "string") {
    result[path[ilen]] = ipString(value);
  } else if (path[ilen].endsWith("GEO")) {
    if (typeof(value) !== "string") {
      result[path[ilen]] = value.map((i) => {return countryMap[i]});
    } else {
      result[path[ilen]] = countryMap[value];
    }
  } else {
    result[path[ilen]] = value;
  }
}
//////////////////////////////////////////////////////////////////////////////////
function appendField(result, field, value) {
  if (result[field] === undefined) {
    result[field] = [];
    result[field+"Cnt"] = 0;
  }

  result[field].push(value);
  result[field+"Cnt"]++;
}

//////////////////////////////////////////////////////////////////////////////////
function remap (result, item, prefix) {
  for (var key in item) {
    var value = item[key];
    var fullkey = prefix + key;

    if (fieldsMap[fullkey] !== undefined) {
      if (fieldsMap[fullkey][0] === "ignore") {continue;}
      setField(result, fieldsMap[fullkey], value);

      if (fullkey.startsWith("hdrs.")) {
        if (fullkey.startsWith("hdrs.hres-")) {
          if (!fullkey.endsWith("cnt")) {
            appendField(result.http, "responseHeader", fullkey.substring(10));
          }
        } else if (fullkey.startsWith("hdrs.hreq-")) {
          if (!fullkey.endsWith("cnt")) {
            appendField(result.http, "requestHeader", fullkey.substring(10));
          }
        } else if (fullkey.startsWith("hdrs.ehead-")) {
          if (!fullkey.endsWith("cnt")) {
            appendField(result.email, "header", fullkey.substring(11));
          }
        }
      }
    } else if (key === "tls") {
      result.cert = [];
      for (var j = 0; j < value.length; j++) {
        var tmp = {};
        remap(tmp, value[j], "tls.");
        result.cert.push(tmp.cert);
      }
    } else if (typeof (value) === "object" && !Array.isArray(value)) {
      remap(result, value, fullkey + ".");
    } else if (fullkey.startsWith("hdrs.hres-")) {
      var path = ["http", "response-" + fullkey.substring(10).replace(/cnt$/, "Cnt")];
      fieldsMap[fullkey] = path;
      setField(result, path, value);
      if (!fullkey.endsWith("cnt")) {
        appendField(result.http, "responseHeader", fullkey.substring(10));
      }
    } else if (fullkey.startsWith("hdrs.hreq-")) {
      var path = ["http", "request-" + fullkey.substring(10).replace(/cnt$/, "Cnt")];
      fieldsMap[fullkey] = path;
      setField(result, path, value);
      if (!fullkey.endsWith("cnt")) {
        appendField(result.http, "requestHeader", fullkey.substring(10));
      }
    } else if (fullkey.startsWith("hdrs.ehead-")) {
      var path = ["email", "header-" + fullkey.substring(11).replace(/cnt$/, "Cnt")];
      fieldsMap[fullkey] = path;
      setField(result, path, value);
      if (!fullkey.endsWith("cnt")) {
        appendField(result.email, "header", fullkey.substring(11));
      }
    } else {
      console.log("Not sure", key);
      console.log(fullkey, " ", item);
    }
  }

  // Now special remaps
  if (prefix === "") {
    if (result.firstPacket === undefined) {
      result.firstPacket = item.fpd || item.fp*1000;
      result.lastPacket = item.lpd || item.lp*1000;
    }

    if (result.cert !== undefined) {
      for (let cert of result.cert) {
        if (cert.notAfter) {cert.notAfter *= 1000;}
        if (cert.notBefore) {cert.notBefore *= 1000;}
      }
    }

    if (result.http && result.http.uri) {
      for (let i = 0; i < result.http.uri.length; i++) {
        result.http.uri[i] = result.http.uri[i].substring(2);
      }
    }

    if (item["tipv61-term"] !== undefined) {
      result.srcIp = item["tipv61-term"].match(/.{4}/g).join(':');
      result.dstIp = item["tipv62-term"].match(/.{4}/g).join(':');
    } else if (item.ipSrc === undefined) {
      result.srcIp = ipString(item.a1);
      result.dstIp = ipString(item.a2);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////
function datestr() {
  return new Date().toString().substring(0,24)
}


//////////////////////////////////////////////////////////////////////////////////
function stats (msg) {
  if (internals.stats[msg.index] === undefined) {
    internals.stats[msg.index] = []
    internals.statsTime[msg.index] = Date.now();
  }

  let stats = internals.stats[msg.index];
  stats[msg.pos] = msg;

  var count = 0;
  var diff = 0;
  var done = true;
  for (var i in stats) {
    count += stats[i].count;
    diff += stats[i].diff;
    done &= stats[i].done;
  }

  const remaining = (msg.total - count)*(diff/count)/1000.0/internals.sliceMax;


  if (done) {
    console.log (datestr(), msg.index, `Finished converting ${count} sessions`);
    if (internals.deleteOnDone) {
      // Wait 3 seconds before deleting
      setTimeout(() => { Db.deleteIndex(msg.oldIndex, function () {}); }, 3000);
    }
  } else if ((Date.now() - internals.statsTime[msg.index]) >= 10000) {
    internals.statsTime[msg.index] = Date.now();
    console.log (datestr(), msg.index, `${count}/${msg.total} (${Math.floor(count*100/msg.total)}%) remaining:`, Math.trunc(remaining/3600), "hours", Math.trunc((remaining%3600)/60), "mins", Math.trunc(remaining%60), "seconds");
  }
}

//////////////////////////////////////////////////////////////////////////////////
function reindex (item, index2, cb) {
  console.log(datestr(), internals.sliceNum, "- STARTING", item.index);
  var count = 0;

  const startTime = Date.now();
  Db.search(item.index, 'session', {size:internals.scrollSize, slice: {id: internals.sliceNum, max: internals.sliceMax}, "sort": ["_doc"]}, {scroll: '1m'}, function getMoreUntilDone(err, result) {
    function sendStats(done) {
      const endTime = Date.now();
      process.send({index: index2,
                    oldIndex: item.index,
                    count: count,
                    total: item['docs.count'],
                    diff: (endTime-startTime),
                    pos: internals.sliceNum,
                    done: done
                   });
    }

    function doNext() {
      count += result.hits.hits.length;

      if ((count % internals.scrollSize) == 0) {
        sendStats(false);
      }

      Db.scroll({body: {scroll_id: result._scroll_id}, scroll: '2m'}, getMoreUntilDone);
    }

    if (err || result.error) {
      console.log("ERROR", err, (result?result.error:null));
      return cb();
    }

    // No more data, all done
    if (result.hits.hits.length === 0) {
      sendStats(true);
      return cb();
    }

    setTimeout(() => {doNext();}, internals.scrollPause);
    var body = [];
    var len = 0;
    for (var i = 0, ilen = result.hits.hits.length; i < ilen; i++) {
      var obj = {};
      remap(obj, result.hits.hits[i]._source, "");

      var hdr = `{"index": {"_index": "${index2}", "_type": "session", "_id": "${result.hits.hits[i]._id}"}}`;
      var str = JSON.stringify(obj);

      // ES has a limit of 100MB for bulk inserts
      if (len + hdr.length + str.length > 100*1000*1000) {
        Db.bulk({body: body}, function(err, msg) {
          if (err) {console.log("ERROR", index2, err, msg);}
        });
        len = 0;
        body = [];
      }

      len += hdr.length + str.length;
      body.push(hdr);
      body.push(str);
    }

    if (len > 0) {
      Db.bulk({body: body}, function(err, msg) {
        if (err) {console.log("ERROR", index2, err, msg);}
      });
    }
  });
}

//////////////////////////////////////////////////////////////////////////////////
function checkFields(fields) {
  var error = 0;

  for (const field of fields) {
  try {
    if (field._source.regex !== undefined || field._source.dbField == undefined) {
      continue;
    }

    var dbField = field._source.dbField;
    if (dbField.match(/\.snow$/) || dbField.match(/\.snowcnt$/)) {
      dbField = dbField.replace(".snow", "");
    }

    if (dbField.match(/hdrs.hres-/)) {
      fieldsMap[dbField] = "http.response-" + dbField.substring(10).replace(/cnt$/, "Cnt");
    } else if (dbField.match(/hdrs.hreq-/)) {
      fieldsMap[dbField] = "http.request-" + dbField.substring(10).replace(/cnt$/, "Cnt");
    } else if (dbField.match(/hdrs.ehead-/)) {
      fieldsMap[dbField] = "email.header-" + dbField.substring(11).replace(/cnt$/, "Cnt");
    }

    if (internals.sliceNum === 0) {
      let doc = {dbField2: fieldsMap[dbField]};

      if (typesMap[dbField]) {
        doc.type2 = typesMap[dbField];
      } else if (!doc.type2 && field._source.type.includes("text")) {
        doc.type2 = field._source.type.replace("text", "term");
      }

      if (fieldsMap[dbField] === "ignore") {
      } else if (field._source.dbField2 === undefined) {
        Db.update("fields", "field", field._id, {doc: doc});
      } else if (field._source.dbField2 !== fieldsMap[dbField]) {
        console.log("WARNING - dbfield2", field._source.dbField2, "doesn't match", fieldsMap[dbField]);
        Db.update("fields", "field", field._id, {doc: doc});
      }
    }

    if (fieldsMap[dbField] === undefined) {
      if (internals.sliceNum !== 0) {process.exit();}
      console.log(field);
      console.log("Unknown field",dbField);
      error++;
    }

  } catch(e) {
    console.trace(e, field);
  }
  }

  if (internals.sliceNum === 0) {
    Db.update("fields", "field", "ip.dst", {doc: {dbField2: "dstIp", portField2: "dstPort"}});
    Db.update("fields", "field", "ip.src", {doc: {dbField2: "srcIp", portField2: "srcPort"}});
    Db.update("fields", "field", "ip.socks", {doc: {portField2: "socks.port"}});
    Db.update("fields", "field", "id", {doc: {dbField2: "_id"}});
    Db.update("fields", "field", "tags", {doc: {dbField2: "tags"}});
    Db.update("fields", "field", "file", {doc: {dbField2: "fileand"}});
    Db.update("fields", "field", "view", {doc: {dbField2: "viewand"}});

    /*
    Db.update("fields", "field", "port", {doc: {regex2: "(Port|\\.port)$"}});
    Db.update("fields", "field", "rir", {doc: {regex2: "(RIR)$"}});
    Db.update("fields", "field", "asn", {doc: {regex2: "(ASN)$"}});
    Db.update("fields", "field", "country", {doc: {regex2: "(GEO)$"}});
    Db.update("fields", "field", "host", {doc: {regex2: "\\.host$"}});
    Db.update("fields", "field", "payload8.hex", {doc: {regex2: "Payload8$"}});
    */
  }

  for (const field in fieldsMap) {
    fieldsMap[field] = fieldsMap[field].split(".");
  }

  if (error) {
    console.log("Fix errors first");
    process.exit(1);
  }
}

//////////////////////////////////////////////////////////////////////////////////
function mainWorker () {
  async.forEachSeries(internals.sessions, function(item, cb) {
    let index2 = item.index.replace("sessions-", "sessions2-");
    reindex(item, index2, cb);
  }, function () {
    Db.flush();
    setTimeout(() => {
      Promise.all([Db.flush(),
                   Db.refresh()
                  ]).then(() => {
        console.log(datestr(), internals.sliceNum, "- worker done");
        setTimeout(() => {process.exit()}, 5000); // Force quit in 5 seconds
      });
    }, 5000);
  });
}
//////////////////////////////////////////////////////////////////////////////////
function mainMaster () {
  // Load local fields and merge into fieldsMap
  try {
    let content = fs.readFileSync("reindex2.local.json");
    let json = JSON.parse(content);
    Db.merge(fieldsMap, json);
  } catch (err) {
    console.log("reindex2.local.json not loaded!");
    console.log("If you need to map fields, create a file called 'reindex2.local.json' that is json format");
    console.log('Sample:\n{\n"ipam.zone-term":                 "ipam.zone",\n"ipam.dc-term":                   "ipam.dc",\n}');
  }

  Db.checkVersion(MIN_DB_VERSION, false);
  Promise.all([Db.indices(undefined, internals.index),
               Db.indices(undefined, "sessions2-*"),
               Db.loadFields()
             ]).then(([sessions, sessions2, fields]) => {

    fields = fields.hits.hits;
    checkFields(fields);

    // Array 2 Map for faster lookups
    var sessions2Map = sessions2.reduce(function(map, obj) {
      map[obj.index] = obj;
      return map;
    }, {});

    let newSessions = [];
    sessions = sessions.sort(function(a,b){return b.index.localeCompare(a.index);});

    // Go thru all the sessions
    sessions.forEach((item) => {
      let index2 = item.index.replace("sessions-", "sessions2-");

      if (sessions2Map[index2] === undefined) {
        newSessions.push(item);
      } else if (sessions2Map[index2]['docs.count'] === item['docs.count']) {
        return;
      } else {
        if (internals.deleteExisting) {
          Db.deleteIndex(index2);
        }
        newSessions.push(item);
      }
    });

    Db.flush();
    Db.refresh();

    // Now tell all the workers
    for (let i = 0; i < internals.sliceMax; i++) {
      let worker = cluster.fork();

      worker.send({sliceMax: internals.sliceMax,
                   sliceNum: i,
                   scrollSize: internals.scrollSize,
                   scrollPause: internals.scrollPause,
                   sessions: newSessions,
                   fieldsMap: fieldsMap,
                   host: internals.elasticBase,
                   prefix: Config.get("prefix", ""),
                   nodeName: Config.nodeName(),
      });
      worker.on('message', stats);
    }
  });
}
//////////////////////////////////////////////////////////////////////////////////
//// Start
//////////////////////////////////////////////////////////////////////////////////
for (let i = 0; i < process.argv.length; i++) {
  switch(process.argv[i]) {
  case "--slices":
    i++;
    internals.sliceMax = +process.argv[i] || 2;
    break;
  case "--size":
    i++;
    internals.scrollSize = +process.argv[i] || 500;
    break;
  case "--pause":
    i++;
    internals.scrollPause = +process.argv[i] || 0;
    break;
  case "--index":
    i++;
    internals.index = process.argv[i] || "sessions-*";
    break;
  case "--deleteExisting":
    internals.deleteExisting = true;
    break;
  case "--deleteOnDone":
    internals.deleteOnDone = true;
    break;
  case "--help":
    console.log("--config <file>                = moloch config.ini file");
    console.log(`--size <scroll size>           = batch size, lower is slower and less load on ES [${internals.scrollSize}]`);
    console.log(`--slices <parallel processes>  = number of parallel processes, lower is slower and less load on ES [${internals.sliceMax}]`);
    console.log(`--pause <pause ms>             = Pause between each ES call, higher is slower and less load on ES [${internals.scrollPause}]`);
    console.log("--index <index pattern>        = indices to process [sessions-*]");
    console.log("--deleteExisting               = delete existing sessions2 indices before reindexing");
    console.log("--deleteOnDone                 = delete sessions index after reindexing");
    process.exit(0);
  }
}

if (internals.sliceMax <= 1) {internals.sliceMax = 2;}
if (internals.sliceMax > 24) {internals.sliceMax = 24;}
if (internals.scrollSize <= 50) {internals.scrollSize = 50;}
if (internals.scrollSize > 2000) {internals.scrollSize = 2000;}
if (internals.scrollPause < 0) {internals.scrollPause = 0;}
if (internals.scrollPause > 5000) {internals.scrollPause = 5000;}

if (cluster.isMaster) {
// If master connect to DB and call mainMaster
  var Config            = require('./config.js');
  internals.elasticBase = Config.get("elasticsearch", "http://localhost:9200").split(",");

  Db.initialize({host:        internals.elasticBase,
                 prefix:      Config.get("prefix", ""),
                 nodeName:    Config.nodeName(),
                 dontMapTags: false}, mainMaster);

  cluster.on('exit', (worker, code, signal) => {
    console.log(`worker ${worker.process.pid} died`);
  });
} else {
// If worker wait for msg from master and then connect to DB and call mainWorker
  process.on('message', function(msg) {
    internals.sliceMax    = msg.sliceMax;
    internals.sliceNum    = msg.sliceNum;
    internals.scrollSize  = msg.scrollSize;
    internals.scrollPause = msg.scrollPause;
    internals.sessions    = msg.sessions;
    fieldsMap             = msg.fieldsMap

    Db.initialize({host: msg.host,
                   prefix: msg.prefix,
                   nodeName: msg.nodeName,
                   dontMapTags: false}, mainWorker);
  });
}
