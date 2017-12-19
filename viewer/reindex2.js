/******************************************************************************/
/* reindex2.js  -- Reindex all the sessions indices to sessions2, which will
 *                 * rename most fields
 *                 * remove duplicate fields
 *                 * remove all tokenized fields
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
  sliceMax: 3,
  sliceNum: 0,
  scrollSize: 2000,
  stats: {}
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

  "ps":               "packetPosArray",
  "psl":              "packetLenArray",
  "fs":               "fileIds",
  "timestamp":        "timestamp",
  "firstPacket":      "firstPacket",
  "lastPacket":       "lastPacket",

  "ro":               "rootId",
  "no":               "node",
  "ss":               "segments",
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
  "http.statuscode":                "http.statuscode",
  "http.bodymagic-term":            "http.bodyMagic",
  "http.bodymagic-term-cnt":        "http.bodyMagicCnt",
  "http.statuscode":                "http.statusCode",
  "http.statuscode-cnt":            "http.statusCodeCnt",
  "xff":                            "http.xffIp",
  "xffscnt":                        "http.xffIpCnt",
  "gxff":                           "http.xffGEO",
  "asxff":                          "http.xffASN",
  "rirxff":                         "http.xffRIR",
  "hh1":                            "ignore",
  "hh1cnt":                         "ignore",
  "hh2":                            "ignore",
  "hh2cnt":                         "ignore",
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
  "ehh":                            "ignore",
  "ehhcnt":                         "ignore",
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
  "threatq.camapign-term":          "ignore",
  "threatq.camapign-term-cnt":      "ignore",
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
  } else {
    result[path[ilen]] = value;
  }
}

//////////////////////////////////////////////////////////////////////////////////
function remap (result, item, prefix) {
  for (var key in item) {
    var value = item[key];
    var fullkey = prefix + key;

    if (fieldsMap[fullkey] !== undefined) {
      if (fieldsMap[fullkey][0] === "ignore") {continue;}
      setField(result, fieldsMap[fullkey], value);
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
    } else if (fullkey.startsWith("hdrs.hreq-")) {
      var path = ["http", "request-" + fullkey.substring(10).replace(/cnt$/, "Cnt")];
      fieldsMap[fullkey] = path;
      setField(result, path, value);
    } else if (fullkey.startsWith("hdrs.ehead-")) {
      var path = ["email", "header-" + fullkey.substring(11).replace(/cnt$/, "Cnt")];
      fieldsMap[fullkey] = path;
      setField(result, path, value);
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
  }

  let stats = internals.stats[msg.index];
  stats[msg.pos] = msg;

  var count = 0;
  var diff = 0;
  for (var i in stats) {
    count += stats[i].count;
    diff += stats[i].diff;
  }

  if (count % (internals.scrollSize*10) == 0) {
    const remaining = (msg.total - count)*(diff/count)/1000.0/internals.sliceMax;
    console.log (datestr(), msg.index, count, "/", msg.total, "remaining", Math.trunc(remaining/3600), "hours", Math.trunc((remaining%3600)/60), "mins", Math.trunc(remaining%60), "seconds");
  }
}

//////////////////////////////////////////////////////////////////////////////////
function reindex (item, index2, cb) {
  console.log(datestr(), internals.sliceNum, "- STARTING", item.index);
  var count = 0;

  const startTime = Date.now();
  Db.search(item.index, 'session', {size:internals.scrollSize, slice: {id: internals.sliceNum, max: internals.sliceMax}, "sort": ["_doc"]}, {scroll: '1m'}, function getMoreUntilDone(err, result) {
    function doNext() {
      count += result.hits.hits.length;

      const endTime = Date.now();

      if ((count % internals.scrollSize) == 0) {
        process.send({index: index2,
                      count: count,
                      total: item['docs.count'],
                      diff: (endTime-startTime),
                      pos: internals.sliceNum
                     });
      }

      Db.scroll({body: {scroll_id: result._scroll_id}, scroll: '1m'}, getMoreUntilDone);
    }

    if (err || result.error) {
      console.log("ERROR", err, (result?result.error:null));
      return cb();
    }

    // No more data, all done
    if (result.hits.hits.length === 0) {
      console.log(datestr(), internals.sliceNum, "- Finished: ", count);
      return cb();
    }

    doNext();
    var body = [];
    for (var i = 0, ilen = result.hits.hits.length; i < ilen; i++) {
      var obj = {};
      remap(obj, result.hits.hits[i]._source, "");
      body.push(`{"index": {"_index": "${index2}", "_type": "session", "_id": "${result.hits.hits[i]._id}"}}`);
      body.push(obj);
    }

    Db.bulk({body: body}, function(err, msg) { 
      if (err) {console.log("ERROR", index2, err, msg);}
    });
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
      if (field._source.type.includes("text")) {
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
function main () {
  try {
    let content = fs.readFileSync("reindex2.local.json");
    let json = JSON.parse(content);
    Db.merge(fieldsMap, json);
  } catch (err) {
    if (internals.sliceNum !== 0) {process.exit();}
    console.log("reindex2.local.json not loaded", err);
  }

  Db.checkVersion(MIN_DB_VERSION, false);
  Promise.all([Db.indices(undefined, "sessions-*"), 
               Db.indices(undefined, "sessions2-*"),
               Db.loadFields()
             ]).then(([sessions, sessions2, fields]) => { 

      fields = fields.hits.hits;
      checkFields(fields);

      var sessions2Map = sessions2.reduce(function(map, obj) {
        map[obj.index] = obj;
        return map;
      }, {});

      async.forEachSeries(sessions, function(item, cb) {
        var index2 = item.index.replace("sessions-", "sessions2-");
        if (sessions2Map[index2] === undefined) {
          reindex(item, index2, cb);
        } else if (sessions2Map[index2]['docs.count'] === item['docs.count']) {
          return cb();
        } else {
          reindex(item, index2, cb);
        }
      }, function () {
        Db.flush();
        setTimeout(() => {
          Promise.all([Db.flush(), 
                       Db.refresh()
                      ]).then(() => {
            console.log(datestr(), internals.sliceNum, "- DONE");
            setTimeout(() => {process.exit()}, 5000); // Force quit in 5 seconds
          });
        }, 5000);
      });
  });
}
//////////////////////////////////////////////////////////////////////////////////
//// Start
//////////////////////////////////////////////////////////////////////////////////

for (let i = 0; i < process.argv.length; i++) {
  if (process.argv[i] === "--slices") {
	  i++;
    internals.sliceMax = +process.argv[i] || 1;
	}
  if (process.argv[i] === "--size") {
	 i++;
    internals.scrollSize = +process.argv[i] || 1000;
	}
}

if (internals.sliceMax <= 1) {internals.sliceMax = 2;}
if (internals.scrollSize <= 100) {internals.scrollSize = 100;}

if (cluster.isMaster) {
  var Config            = require('./config.js');
  internals.elasticBase = Config.get("elasticsearch", "http://localhost:9200").split(",");

  for (let i = 0; i < internals.sliceMax; i++) {
    let worker = cluster.fork();

    worker.send({sliceMax: internals.sliceMax,
                 sliceNum: i,
                 scrollSize: internals.scrollSize,
                 host: internals.elasticBase,
                 prefix: Config.get("prefix", ""),
                 nodeName: Config.nodeName(),
    });
    worker.on('message', stats);
  }

  cluster.on('exit', (worker, code, signal) => {
    console.log(`worker ${worker.process.pid} died`);
  });
} else {
  process.on('message', function(msg) {
    internals.sliceMax = msg.sliceMax;
    internals.sliceNum = msg.sliceNum;
    internals.scrollSize = msg.scrollSize;

    Db.initialize({host: msg.host,
                   prefix: msg.prefix,
                   nodeName: msg.nodeName,
                   dontMapTags: false}, main);
  });
}
