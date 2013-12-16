/******************************************************************************/
/* config.js -- Code dealing with the config file, command line arguments, 
 *              and dropping privileges
 *
 * Copyright 2012-2013 AOL Inc. All rights reserved.
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
/*jshint
  node: true, plusplus: false, curly: true, eqeqeq: true, immed: true, latedef: true, newcap: true, nonew: true, undef: true, strict: true, trailing: true
*/
"use strict";

//////////////////////////////////////////////////////////////////////////////////
//// Command Line Parsing
//////////////////////////////////////////////////////////////////////////////////
var ini    = require('iniparser'),
    os     = require('os'),
    fs     = require('fs'),
    crypto = require('crypto');

var internals = {
    configFile: "/data/moloch/etc/config.ini",
    nodeName: os.hostname().split(".")[0],
    fields: [],
    ipFields: [],
    fieldsMap: {}
  };

function processArgs() {
  var args = [];
  for (var i = 0, ilen = process.argv.length; i < ilen; i++) {
    if (process.argv[i] === "-c") {
      i++;
      internals.configFile = process.argv[i];
    } else if (process.argv[i] === "-n") {
      i++;
      internals.nodeName = process.argv[i];
    } else {
      args.push(process.argv[i]);
    }
  }
  process.argv = args;
}
processArgs();

//////////////////////////////////////////////////////////////////////////////////
// Encryption stuff
//////////////////////////////////////////////////////////////////////////////////
exports.md5 = function (str, encoding){
  return crypto
    .createHash('md5')
    .update(str)
    .digest(encoding || 'hex');
};

exports.pass2store = function(userid, password) {
  var m = exports.md5(userid + ":" + exports.getFull("default", "httpRealm", "Moloch") + ":" + password);
  var c = crypto.createCipher('aes192', exports.getFull("default", "passwordSecret", "password"));
  var e = c.update(m, "binary", "hex");
  e += c.final("hex");
  return e;
};

exports.store2ha1 = function(passstore) {
  try {
    var c = crypto.createDecipher('aes192', exports.getFull("default", "passwordSecret", "password"));
    var d = c.update(passstore, "hex", "binary");
    d += c.final("binary");
  } catch (e) {
    console.log("passwordSecret set in the [default] section can not decrypt information.  You may need to re-add users if you've changed the secret.", e);
    process.exit(1);
  }
  return d;
};

exports.obj2auth = function(obj, secret) {
  secret = secret || exports.getFull("default", "passwordSecret", "password");
  var c = crypto.createCipher('aes192', secret);
  var e = c.update(JSON.stringify(obj), "binary", "hex");
  e += c.final("hex");
  return e;
};

exports.auth2obj = function(auth, secret) {
  secret = secret || exports.getFull("default", "passwordSecret", "password");
  var c = crypto.createDecipher('aes192', secret);
  var d = c.update(auth, "hex", "binary");
  d += c.final("binary");
  return JSON.parse(d);
};

//////////////////////////////////////////////////////////////////////////////////
// Config File & Dropping Privileges
//////////////////////////////////////////////////////////////////////////////////

if (!fs.existsSync(internals.configFile)) {
  console.log("ERROR - Couldn't open config file '" + internals.configFile + "' maybe use the -c <configfile> option");
  process.exit(1);
}
internals.config = ini.parseSync(internals.configFile);


if (internals.config["default"] === undefined) {
  console.log("ERROR - [default] section missing from", internals.configFile);
  process.exit(1);
}

exports.getFull = function(node, key, defaultValue) {
  if (internals.config[node] && internals.config[node][key] !== undefined ) {
    return internals.config[node][key];
  }

  if (internals.config[node] && internals.config[node].nodeClass && internals.config[internals.config[node].nodeClass] && internals.config[internals.config[node].nodeClass][key]) {
    return internals.config[internals.config[node].nodeClass][key];
  }

  if (internals.config["default"][key]) {
    return internals.config["default"][key];
  }

  return defaultValue;
};

exports.get = function(key, defaultValue) {
  return exports.getFull(internals.nodeName, key, defaultValue);
};

exports.getObj = function(key, defaultValue) {
  var full = exports.getFull(internals.nodeName, key, defaultValue);
  if (!full) {
    return null;
  }

  var obj = {};
  full.split(';').forEach(function(element) {
    var parts = element.split("=");
    if (parts && parts.length === 2) {
      if (parts[1] === "true") {
        parts[1] = true;
      } else if (parts[1] === "false") {
        parts[1] = false;
      }
      obj[parts[0]] = parts[1];
    }
  });
  return obj;
};

function dropPrivileges() {
  if (process.getuid() !== 0) {
    return;
  }

  var group = exports.get("dropGroup", null);
  if (group !== null) {
    process.setgid(group);
  }

  var user = exports.get("dropUser", null);
  if (user !== null) {
    process.setuid(user);
  }
}

exports.getConfigFile = function() {
  return internals.configFile;
};

exports.isHTTPS = function(node) {
  return exports.getFull(node || internals.nodeName, "keyFile") &&
         exports.getFull(node || internals.nodeName, "certFile");
};

exports.basePath = function(node) {
  return exports.getFull(node || internals.nodeName, "webBasePath", "/");
};

exports.nodeName = function() {
  return internals.nodeName;
};

exports.keys = function(section) {
  if (internals.config[section] === undefined) {return [];}
  return Object.keys(internals.config[section]);
};

exports.headers = function(section) {
  if (internals.config[section] === undefined) {return [];}
  var keys = Object.keys(internals.config[section]);
  if (!keys) {return [];}
  var headers = Object.keys(internals.config[section]).map(function(key) {
    var obj = {name: key};
    internals.config[section][key].split(';').forEach(function(element) {
      var parts = element.split(":");
      if (parts && parts.length === 2) {
        if (parts[1] === "true") {
          parts[1] = true;
        } else if (parts[1] === "false") {
          parts[1] = false;
        }
        obj[parts[0]] = parts[1];
      }
    });
    return obj;
  });

  return headers;
};

dropPrivileges();

//////////////////////////////////////////////////////////////////////////////////
// Fields
//////////////////////////////////////////////////////////////////////////////////
function addField(db, exp, cat, type, help) {
  internals.fields.push({db: db, exp: exp, cat: cat, type: type, help:help});
  internals.fieldsMap[exp] = {db: db, cat: cat, type: type};
}

function addIpField(name, addr, port, geo, asn, rir) {
  internals.ipFields.push({name: name, addr: addr, port: port, geo: geo, asn: asn, rir: rir});
}


exports.getFields = function() {
  return internals.fields;
};

exports.getIpFields = function() {
  return internals.ipFields;
};

exports.getFieldsMap = function() {
  return internals.fieldsMap;
};

addField(null, "ip", "general", "ip", "Shorthand for ip.src, ip.dst, ip.dns, ip.email, ip.socks, or ip.xff");
addField("a1", "ip.src", "general", "ip", "Source ip");
addField("a2", "ip.dst", "general", "ip", "Destination ip");
addField("dnsip", "ip.dns", "dns", "ip", "IP from DNS result");
addField("dnsipcnt", "ip.dns.cnt", "dns", "integer", "Unique number of IPs from DNS result");
addField("eip", "ip.email", "email", "ip", "IP from email hop");
addField("eipcnt", "ip.email.cnt", "email", "integer", "Unqiue number of IPs from email hop");
addField("socksip", "ip.socks", "socks", "ip", "Socks destination ip");
addField("xff", "ip.xff", "http", "ip", "IP from x-forwarded-for header");
addField("xffscnt", "ip.xff.cnt", "http", "integer", "Unique number of IPs from x-forwarded-for header");

addField(null, "port", "general", "integer", "Shorthand for port.src, port.socks, or port.dst");
addField("p1", "port.src", "general", "integer", "Source port");
addField("p2", "port.dst", "general", "integer", "Destination port");
addField("sockspo", "port.socks", "socks", "integer", "Socks destination port");

addField(null, "payload8", "general", "lotermfield", "Shorthand for payload8.src, payload8.dst");
addField("fb1", "payload8.src", "general", "lotermfield", "First 8 bytes of source payload in hex");
addField("fb2", "payload8.dst", "general", "lotermfield", "First 8 bytes of destination payload in hex");

addField(null, "asn", "general", "textfield", "Shorthand for the GeoIP ASNum string from the asn.src, asn.dst, asn.dns, asn.email, asn.socks, or asn.xff fields");
addField("as1", "asn.src", "general", "textfield", "GeoIP ASNum string calculated from the source ip address");
addField("as2", "asn.dst", "general", "textfield", "GeoIP ASNum string calculated from the destination ip address");
addField("asdnsip", "asn.dns", "dns", "textfield", "GeoIP ASNum string calculated from the DNS result ip address");
addField("aseip", "asn.email", "email", "textfield", "GeoIP ASNum string calculated from the SMTP ip address");
addField("assocksip", "asn.socks", "socks", "textfield", "GeoIP ASNum string calculated from the socks destination ip address");
addField("asxff", "asn.xff", "http", "textfield", "GeoIP ASNum string calculated from the HTTP x-forwarded-for header");

addField(null, "country", "general", "uptermfield", "Shorthand for the GeoIP string from the country.src, country.dst, country.dns, country.email, country.socks, or country.xff fields");
addField("g1", "country.src", "general", "uptermfield", "GeoIP country string calculated from the source ip address");
addField("g2", "country.dst", "general", "uptermfield", "GeoIP country string calculated from the destination ip address");
addField("gdnsip", "country.dns", "dns", "uptermfield", "GeoIP country string calculated from the DNS result ip address");
addField("geip", "country.email", "email", "uptermfield", "GeoIP country string calculated from the SMTP ip address");
addField("gsocksip", "country.socks", "socks", "uptermfield", "GeoIP country string calculated from the socks destination ip address");
addField("gxff", "country.xff", "http", "uptermfield", "GeoIP country string calculated from the HTTP x-forwarded-for header");

addField(null, "rir", "general", "uptermfield", "Shorthand for the Regional Internet Registry from the rir.src, rir.dst, rir.dns, rir.email, rir.socks, or country.xff fields");
addField("rir1", "rir.src", "general", "uptermfield", "Regional Internet Registry string calculated from the source ip address");
addField("rir2", "rir.dst", "general", "uptermfield", "Regional Internet Registry string calculated from the destination ip address");
addField("rirdnsip", "rir.dns", "dns", "uptermfield", "Regional Internet Registry string calculated from the DNS result ip address");
addField("rireip", "rir.email", "email", "uptermfield", "Regional Internet Registry string calculated from the SMTP ip address");
addField("rirsocksip", "rir.socks", "socks", "uptermfield", "Regional Internet Registry string calculated from the socks destination ip address");
addField("rirxff", "rir.xff", "http", "uptermfield", "Regional Internet Registry string calculated from the HTTP x-forwarded-for header");

addField("by", "bytes", "general", "integer", "Total number of raw bytes sent AND received in a session");
addField("db", "databytes", "general", "integer", "Total number of data bytes sent AND received in a session");
addField("pa", "packets", "general", "integer", "Total number of packets sent AND received in a session");
addField("pr", "protocol", "general", "integer", "IP protocol number");
addField("id", "id", "general", "termfield", "Moloch ID for the session");
addField("ro", "rootId", "general", "termfield", "Moloch ID of the first session in a multi session stream");
addField("no", "node", "general", "termfield", "Moloch node name the session was recorded on");
addField("ta", "tags", "general", "lotermfield", "Tests if the session has the tag");
addField("tacnt", "tags.cnt", "general", "integer", "Number of unique tags");
addField("user", "user", "general", "lotermfield", "External user set for session");
addField(null, "file", "general", "termfield", "File name of offline added files");
addField("socksuser", "socks.user", "socks", "termfield", "Socks user");

addField(null, "host", "general", "termfield", "Shorthand for host.dns, host.email. host.http");
addField("ho", "host.http", "http", "lotermfield", "HTTP host header field");
addField("hocnt", "host.http.cnt", "http", "integer", "Unique number of HTTP host headers");
addField("dnsho", "host.dns", "dns", "lotermfield", "DNS host response");
addField("dnshocnt", "host.dns.cnt", "dns", "integer", "Unique number DNS host responses");
addField("eho", "host.email", "email", "lotermfield", "EMAIL host proxy");
addField("ehocnt", "host.email.cnt", "email", "integer", "Unique number of EMAIL host proxies");
addField("socksho", "host.socks", "socks", "lotermfield", "Socks destination host");

addField("tls.iCn", "cert.issuer.cn", "cert", "lotermfield", "Issuer's common name");
addField("tls.iOn", "cert.issuer.on", "cert", "lotextfield", "Issuer's organization name");
addField("tls.sCn", "cert.subject.cn", "cert", "lotermfield", "Subject's common name");
addField("tls.sOn", "cert.subject.on", "cert", "lotextfield", "Subject's organization name");
addField("tls.sn",  "cert.serial", "cert", "termfield", "Serial Number");
addField("tls.alt",  "cert.alt", "cert", "lotermfield", "Alternative names");
addField("tls.altcnt", "cert.alt.cnt", "cert", "integer", "Number of unique alternative names");
addField("tlscnt", "cert.cnt", "cert", "integer", "Number of unique certificates in session");

addField("ircnck",  "irc.nick", "irc", "termfield", "Nicknames set");
addField("ircnckcnt",  "irc.nick.cnt", "irc", "integer", "Unique number of nicknames set");
addField("ircch",  "irc.channel", "irc", "termfield", "Channels joined ");
addField("ircchcnt",  "irc.channel.cnt", "irc", "integer", "Unique number of channels joined");

addField("ect", "email.content-type", "email", "termfield", "Content-Type header of message");
addField("ectcnt", "email.content-type.cnt", "email", "integer", "Unique number of content-type headers");
addField("eid", "email.message-id", "email", "termfield", "Message-Id header of message");
addField("eidcnt", "email.message-id.cnt", "email", "integer", "Unique number of Message-Id headers");
addField("edst", "email.dst", "email", "lotermfield", "To and CC email destinations");
addField("edstcnt", "email.dst.cnt", "email", "integer", "Unique number of To and CC email destinations");
addField("esrc", "email.src", "email", "lotermfield", "Email from address");
addField("esrccnt", "email.src.cnt", "email", "integer", "Unique number of email from addresses");
addField("efn", "email.fn", "email", "termfield", "Email attachment filenames");
addField("efncnt", "email.fn.cnt", "email", "integer", "Unique number of email attachment filenames");
addField("emd5", "email.md5", "email", "lotermfield", "Email md5 of attachments ");
addField("emd5cnt", "email.md5.cnt", "email", "integer", "Unique number of md5s of attachments");
addField("emv", "email.mime-version", "email", "lotermfield", "Email mime-version header");
addField("emvcnt", "email.mime-version.cnt", "email", "integer", "Unique number of mime-version header values");
addField("esub", "email.subject", "email", "lotextfield", "Email subject header");
addField("esubcnt", "email.subject.cnt", "email", "integer", "Unique number of subject header values");
addField("eua", "email.x-mailer", "email", "lotextfield", "Email x-mailer header");
addField("euacnt", "email.x-mailer.cnt", "email", "integer", "Unique number of x-mailer header values");

addField(null, "http.hasheader", "http", "lotermfield", "Shorthand for http.hasheader.src or http.hasheader.dst");
addField("hh1", "http.hasheader.src", "http", "lotermfield", "Check if the request has a header present");
addField("hh1cnt", "http.hasheader.src.cnt", "http", "integer", "Unique number of headers the request has");
addField("hh2", "http.hasheader.dst", "http", "lotermfield", "Check if the response has a header present");
addField("hh2cnt", "http.hasheader.dst.cnt", "http", "integer", "Unique number of headers the response has");

addField("hmd5", "http.md5", "http", "termfield", "MD5 of http body response");
addField("hmd5cnt", "http.md5.cnt", "http", "integer", "Unique number of MD5 of http body responses");
addField(null, "http.version", "http", "termfield", "Shorthand for http.version.src or http.version.dst");
addField("hsver", "http.version.src", "http", "termfield", "Request HTTP version number");
addField("hsvercnt", "http.version.src.cnt", "http", "integer", "Unique number of request HTTP versions");
addField("hdver", "http.version.dst", "http", "termfield", "Response HTTP version number");
addField("hdvercnt", "http.version.src.cnt", "http", "integer", "Unique number of response HTTP versions");
addField("ua", "http.user-agent", "http", "textfield", "User-Agent header");
addField("uacnt", "http.user-agent.cnt", "http", "integer", "Unique number of User-Agent headers");
addField("us", "http.uri", "http", "textfield", "URIs for request");
addField("uscnt", "http.uri.cnt", "http", "integer", "Unique number of request URIs");

addField("hpathcnt", "http.uri.path.cnt", "http", "integer", "Unique number of paths of all URIs in session");
addField("hpath", "http.uri.path", "http", "termfield", "Path portion of URI");
addField("hkeycnt", "http.uri.key.cnt", "http", "integer", "Unique number of querystring keys of all URIs in session");
addField("hkey", "http.uri.key", "http", "termfield", "Keys from querystring of URI");
addField("hvalcnt", "http.uri.value.cnt", "http", "integer", "Unique number of querystring values of all URIs in session");
addField("hval", "http.uri.value", "http", "termfield", "Values from querystring of URI");

addField("sshkey", "ssh.key", "ssh", "termfield", "Base64 encoded host key");
addField("sshkeycnt", "ssh.key.cnt", "ssh", "integer", "Number of unique Base64 encoded host keys");
addField("sshver", "ssh.ver", "ssh", "lotermfield", "SSH version string");
addField("sshvercnt", "ssh.ver.cnt", "ssh", "integer", "Number of unique ssh version strings");

addField("smbsh", "smb.share", "smb", "termfield", "SMB shares connected to");
addField("smbshcnt", "smb.share.cnt", "smb", "integer", "Number of unique SMB shares connected to");
addField("smbfn", "smb.fn", "smb", "termfield", "SMB files opened, created, deleted");
addField("smbfncnt", "smb.fn.cnt", "smb", "integer", "Number of unique SMB files opened, created, deleted");
addField("smbho", "smb.host", "smb", "termfield", "SMB Hostnames");
addField("smbhocnt", "smb.host.cnt", "smb", "integer", "Number of unique SMB Hostnames");
addField("smbos", "smb.os", "smb", "termfield", "SMB OS");
addField("smboscnt", "smb.os.cnt", "smb", "integer", "Number of unique SMB OSes");
addField("smbdm", "smb.domain", "smb", "termfield", "SMB domain");
addField("smbdmcnt", "smb.domain.cnt", "smb", "integer", "Number of unique SMB domains");
addField("smbver", "smb.ver", "smb", "termfield", "SMB verison");
addField("smbvercnt", "smb.ver.cnt", "smb", "integer", "Number of unique SMB versions");
addField("smbuser", "smb.user", "smb", "termfield", "SMB user");
addField("smbusercnt", "smb.usre.cnt", "smb", "integer", "Number of unique SMB users");

exports.headers("headers-http-request").forEach(function(item) {
  addField("hdrs.hreq-" + item.name + (item.type === "integer"?"":".snow"), "http." + item.name, "http", (item.type === "integer"?"integer":"textfield"), "Request header " + item.name);
  if (item.count === true) {
    addField("hdrs.hreq-" + item.name + "cnt", "http." + item.name + ".cnt", "http", "integer", "Unique number of request header " + item.name);
  }
});

exports.headers("headers-http-response").forEach(function(item) {
  addField("hdrs.hres-" + item.name + (item.type === "integer"?"":".snow"), "http." + item.name, "http", (item.type === "integer"?"integer":"textfield"), "Response header " + item.name);
  if (item.count === true) {
    addField("hdrs.hres-" + item.name + "cnt", "http." + item.name + ".cnt", "http", "integer", "Unique number of response header " + item.name);
  }
});

exports.headers("headers-email").forEach(function(item) {
  addField("hdrs.ehead-" + item.name + (item.type === "integer"?"":".snow"), "email." + item.name, "email", (item.type === "integer"?"integer":"textfield"), "Email header " + item.name);
  if (item.count === true) {
    addField("hdrs.ehead-" + item.name + "cnt", "email." + item.name + ".cnt", "http", "integer", "Unique number of email header " + item.name);
  }
});

exports.headers("plugin-fields").forEach(function(item) {
  var p = "plugin." + item.name;
  if (item.type === "ip") {
    addField(p, p, "plugin", "ip", "Plugin field " + item.name);
    addField(p + ".geo.snow", p + ".country", "plugin", "ip", "Plugin field " + item.name + " GEO");
    addField(p + ".rir.snow", p + ".rir", "plugin", "ip", "Plugin field " + item.name + " RIR");
    addField(p + ".asn.snow", p + ".asn", "plugin", "ip", "Plugin field " + item.name + " ASN");
    addIpField(p, p, null, p + ".geo.raw", p + ".asn.snow", p + ".rir.raw");
  } else {
    addField(p + (item.type === "integer"?"":".snow"), p, "plugin", (item.type === "integer"?"integer":"textfield"), "Plugin field " + item.name);
  }

  if (item.count === true) {
    addField("plugin." + item.name + "cnt", "plugin." + item.name + ".cnt", "http", "integer", "Unique number of plugin field " + item.name);
  }
});

addIpField("Src", "a1", "p1", "g1", "as1", "rir1");
addIpField("Dst", "a2", "p2", "g2", "as2", "rir2");
addIpField("DNS", "dnsip", null, "gdnsip", "asdnsip", "rirdnsip");
addIpField("XFF", "xff", null, "gxff", "asxff", "rirxff");
addIpField("Socks", "socksip", "sockspo", "gsocksip", "assocksip", "rirsocksip");
addIpField("Email", "eip", null, "geip", "aseip", "rireip");
