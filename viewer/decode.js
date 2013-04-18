/******************************************************************************/
/* decode.js -- The pcap decoding functions
 *
 * Copyright 2012 The AOL Moloch Authors.  All Rights Reserved.
 *
 * Copyright 2012 AOL Inc. All rights reserved.
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
//// Decode pcap buffers and build up simple objects
//////////////////////////////////////////////////////////////////////////////////

var internals = {
  pr2name: {
    1:  "icmp",
    6:  "tcp",
    17: "udp",
    58: "icmpv6"
  }
};

exports.protocol2Name = function(num) {
  return internals.pr2name[num] || "" + num;
};

exports.inet_ntoa = function(num) {
  return (num >> 24 & 0xff) + '.' + (num>>16 & 0xff) + '.' + (num>>8 & 0xff) + '.' + (num & 0xff);
};


exports.icmp = function (buffer, obj) {
  obj.icmp = {
    length:    buffer.length,
    type:      buffer[0],
    code:      buffer[1],
    sum:       buffer.readUInt16BE(2),
    id:        buffer.readUInt16BE(4),
    sequence:  buffer.readUInt16BE(6)
  };

  obj.icmp.data = buffer.slice(8);
};

exports.tcp = function (buffer, obj) {
  obj.tcp = {
    length:     buffer.length,
    sport:      buffer.readUInt16BE(0),
    dport:      buffer.readUInt16BE(2),
    seq:        buffer.readUInt32BE(4),
    ack:        buffer.readUInt32BE(8),
    off:        ((buffer[12] >> 4) & 0xf),
    res1:       (buffer[12] & 0xf),
    flags:      buffer[13],
    res2:       (buffer[13] >> 6 & 0x3),
    urgflag:    (buffer[13] >> 5 & 0x1),
    ackflag:    (buffer[13] >> 4 & 0x1),
    pshflag:    (buffer[13] >> 3 & 0x1),
    rstflag:    (buffer[13] >> 2 & 0x1),
    synflag:    (buffer[13] >> 1 & 0x1),
    finflag:    (buffer[13] >> 0 & 0x1),
    win:        buffer.readUInt16BE(14),
    sum:        buffer.readUInt16BE(16),
    urp:        buffer.readUInt16BE(18)
  };

  if (4*obj.tcp.off > buffer.length) {
    obj.tcp.data = new Buffer(0);
  } else {
    obj.tcp.data = buffer.slice(4*obj.tcp.off);
  }
};

exports.udp = function (buffer, obj) {
  obj.udp = {
    length:     buffer.length,
    sport:      buffer.readUInt16BE(0),
    dport:      buffer.readUInt16BE(2),
    ulen:       buffer.readUInt16BE(4),
    sum:        buffer.readUInt16BE(6)
  };

  obj.udp.data = buffer.slice(8);
};

exports.ip4 = function (buffer, obj) {
  obj.ip = {
    length: buffer.length,
    hl:     (buffer[0] & 0xf),
    v:      ((buffer[0] >> 4) & 0xf),
    tos:    buffer[1],
    len:    buffer.readUInt16BE(2),
    id:     buffer.readUInt16BE(4),
    off:    buffer.readUInt16BE(6),
    ttl:    buffer[8],
    p:      buffer[9],
    sum:    buffer.readUInt16BE(10),
    addr1:  exports.inet_ntoa(buffer.readUInt32BE(12)),
    addr2:  exports.inet_ntoa(buffer.readUInt32BE(16))
  };

  switch(obj.ip.p) {
  case 1:
    exports.icmp(buffer.slice(obj.ip.hl*4, obj.ip.len), obj);
    break;
  case 6:
    exports.tcp(buffer.slice(obj.ip.hl*4, obj.ip.len), obj);
    break;
  case 17:
    exports.udp(buffer.slice(obj.ip.hl*4, obj.ip.len), obj);
    break;
  default:
    console.log("Unknown ip.p", obj);
  }
};

exports.ip6 = function (buffer, obj) {
  obj.ip = {
    length: buffer.length,
    v:      ((buffer[0] >> 4) & 0xf),
    tc:     ((buffer[0] & 0xf) << 4) | ((buffer[1] >> 4) & 0xf),
    flow:   ((buffer[1] & 0xf) << 16) | (buffer[2] << 8) | buffer[3],
    len:    buffer.readUInt16BE(4),
    nextHeader: buffer[6],
    hopLimt:  buffer[7]
  };
};

exports.ethertype = function(buffer, obj) {
  obj.ether.type = buffer.readUInt16BE(0);

  switch(obj.ether.type) {
  case 0x0800:
    exports.ip4(buffer.slice(2), obj);
    break;
  case 0x86dd:
    exports.ip6(buffer.slice(2), obj);
    break;
  case 0x8100: // VLAN
    exports.ethertype(buffer.slice(4), obj);
    break;
  default:
    console.log("Unknown ether.type", obj);
    break;
  }
};

exports.ether = function (buffer, obj) {
  obj.ether = {
    length: buffer.length,
    addr1:  buffer.slice(0, 6).toString('hex', 0, 6),
    addr2:  buffer.slice(6, 12).toString('hex', 0, 6)
  };
  exports.ethertype(buffer.slice(12), obj);
};


exports.pcap = function (buffer, obj) {
  obj.pcap = {
    ts_sec:   buffer.readUInt32LE(0),
    ts_usec:  buffer.readUInt32LE(4),
    incl_len: buffer.readUInt32LE(8),
    orig_len: buffer.readUInt32LE(12)
  };

  exports.ether(buffer.slice(16, obj.pcap.incl_len + 16), obj);
};

exports.reassemble_icmp = function (packets, cb) {
  var results = [];
  packets.forEach(function (item) {
    var key = item.ip.addr1;
    if (results.length === 0 || key !== results[results.length-1].key) {
      var result = {
        key: key,
        data: item.icmp.data,
        ts: item.pcap.ts_sec*1000 + Math.round(item.pcap.ts_usec/1000)
      };
      results.push(result);
    } else {
      var newBuf = new Buffer(results[results.length-1].data.length + item.icmp.data.length);
      results[results.length-1].data.copy(newBuf);
      item.icmp.data.copy(newBuf, results[results.length-1].data.length);
      results[results.length-1].data = newBuf;
    }
  });
  cb(null, results);
};

exports.reassemble_udp = function (packets, cb) {
  var results = [];
  packets.forEach(function (item) {
    var key = item.ip.addr1 + ':' + item.udp.sport;
    if (results.length === 0 || key !== results[results.length-1].key) {
      var result = {
        key: key,
        data: item.udp.data,
        ts: item.pcap.ts_sec*1000 + Math.round(item.pcap.ts_usec/1000)
      };
      results.push(result);
    } else {
      var newBuf = new Buffer(results[results.length-1].data.length + item.udp.data.length);
      results[results.length-1].data.copy(newBuf);
      item.udp.data.copy(newBuf, results[results.length-1].data.length);
      results[results.length-1].data = newBuf;
    }
  });
  cb(null, results);
};

exports.reassemble_tcp = function (packets, a1, cb) {

  // Remove syn, rst, 0 length packets
  var packets2 = [];
  for (var i = 0; i < packets.length; i++) {
    if (packets[i].tcp.data.length === 0 || packets[i].tcp.rstflag || packets[i].tcp.synflag) {
      continue;
    }
    packets2.push(packets[i]);
  }

  packets = packets2;
  packets2 = undefined;

  if (packets.length === 0) {
      return cb(null, packets);
  }

  // Sort Packets
  var clientKey = packets[0].ip.addr1 + ':' + packets[0].tcp.sport;
  packets.sort(function(a,b) {
    if ((a.ip.addr1 === b.ip.addr1) && (a.tcp.sport === b.tcp.sport)) {
      return (a.tcp.seq - b.tcp.seq);
    }

    if (clientKey === a.ip.addr1 + ':' + a.tcp.sport) {
      return ((a.tcp.seq + a.tcp.data.length-1) - b.tcp.ack);
    }

    return (a.tcp.ack - (b.tcp.seq + b.tcp.data.length-1) );
  });

  // Now divide up conversation
  var clientSeq = 0;
  var hostSeq = 0;
  var start = 0;

  var results = [];
  packets.forEach(function (item) {
    var key = item.ip.addr1 + ':' + item.tcp.sport;
    if (key === clientKey) {
      if (clientSeq >= (item.tcp.seq + item.tcp.data.length)) {
        return;
      }
      clientSeq = (item.tcp.seq + item.tcp.data.length);
    } else {
      if (hostSeq >= (item.tcp.seq + item.tcp.data.length)) {
        return;
      }
      hostSeq = (item.tcp.seq + item.tcp.data.length);
    }

    if (results.length === 0 || key !== results[results.length-1].key) {
      start = item.tcp.seq;
      var result = {
        key: key,
        data: item.tcp.data,
        ts: item.pcap.ts_sec*1000 + Math.round(item.pcap.ts_usec/1000)
      };
      results.push(result);
    } else {
      var newBuf = new Buffer(item.tcp.data.length + item.tcp.seq - start);
      results[results.length-1].data.copy(newBuf);
      item.tcp.data.copy(newBuf, item.tcp.seq - start);
      results[results.length-1].data = newBuf;
    }
  });

  if (a1 !== results[0].key) {
    results.unshift({data: new Buffer(0), key: a1});
  }
  cb(null, results);
};
