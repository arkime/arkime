/* decode.js  -- The pcap decoding code.
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
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

var util       = require('util');
var Stream     = require('stream');
var Readable   = Stream.Readable;
var Transform  = Stream.Transform;
var Writable   = Stream.Writable;
var HTTPParser = process.binding('http_parser').HTTPParser;
var zlib       = require('zlib');
var through    = require('through2');
var peek       = require('peek-stream');
var sprintf    = require('./public/sprintf.js');
var async      = require('async');

var internals  = {registry: {},
                  settings: {}};

////////////////////////////////////////////////////////////////////////////////
function mkname (stream, name) {
  if (!mkname.cnt) {mkname.cnt = {};}
  if (!mkname.cnt[name]) {mkname.cnt[name] = 0;}
  mkname.cnt[name]++;
  stream.molochName = name + "-" + mkname.cnt[name];
}
////////////////////////////////////////////////////////////////////////////////
function safeStr(str) {
  return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/\"/g,'&quot;').replace(/\'/g, '&#39;').replace(/\//g, '&#47;');
}
////////////////////////////////////////////////////////////////////////////////
function ItemTransform(options) {
  Transform.call(this, {objectMode: true});
  this._itemTransform = {state: 0,
                            max: options.maxPeekItems || 1,
                          items: []};
}
util.inherits(ItemTransform, Transform);
ItemTransform.prototype._transform = function (item, encoding, callback) {
  var self = this;
  switch (self._itemTransform.state) {
  case 0:
    self._itemTransform.items.push(item);
    if (self._shouldProcess(item)) {
      self._itemTransform.state = 1;
      async.each(self._itemTransform.items, function (item, cb) {
        try {
          self._process(item, function(err, data) {
            if (data) {
              self.push(data);
            }
            return cb();
          });
        } catch (err) {
          cb(err);
          console.log("Couldn't decode", err);
        };
      }, function (err) {
        self._itemTransform.items = [];
        return callback();
      });
      return;
    }
    if (self._itemTransform.items.length === self._itemTransform.max) {
      self._itemTransform.state = 2;
      while ((item = self._itemTransform.items.shift())) {
        self.push(item);
      }
      self._itemTransform.items = [];
    }
    return callback();
  case 1:
    return self._process(item, callback);
  case 2:
    return callback(null, item);
  }
};

ItemTransform.prototype._flush = function (callback) {
  if (this._itemTransform.state === 1) {
    return this._finish(callback);
  }

  var item;
  while ((item = this._itemTransform.items.shift())) {
    this.push(item);
  }

  callback();
};

////////////////////////////////////////////////////////////////////////////////
function Pcap2ItemStream(options, pcap) {
  Readable.call(this, {objectMode: true});
  mkname(this, "Pcap2ItemStream");
  this.data = pcap || [];
}
util.inherits(Pcap2ItemStream, Readable);
Pcap2ItemStream.prototype._read = function (size) {
  var data = this.data;
  for (var i = 0; i < data.length; i++) {
    data[i].client = (i % 2);
    this.push(data[i]);
  }
  this.push(null);
};

////////////////////////////////////////////////////////////////////////////////
function createUncompressStream (options, context) {
  return peek({newline:false, maxBuffer:3}, function(data, swap) {
    if (data.length < 3) {
      return swap(null, through());
    }

    if (data[0] === 0x1f && data[1] === 0x8b && data[2] === 8) {
      if (context.headersMap && context.headersMap["content-encoding"] === "deflate") {
        context.headersMap["content-encoding"] = "moloch-gzip";
      }
      var s = zlib.createGunzip();
      return swap(null, s);
    }

    /*if (context.headersMap && context.headersMap["content-encoding"] === "deflate") {
      context.headersMap["content-encoding"] = "moloch-deflate";
      return swap(null, zlib.createDeflate());
    }*/

    return swap(null, through());
  });
}

////////////////////////////////////////////////////////////////////////////////
function createUnbase64Stream (options, context) {
  return through(function(data, encoding, callback) {
    callback(null, new Buffer(data.toString("binary"), "base64"));
  });
}

////////////////////////////////////////////////////////////////////////////////
function createKeyUnxorStream (options, context) {
  return through(function(data, encoding, callback) {
    if (this.state === undefined) {
      if (!options["BODY-UNXOR"]) {
        this.state = 2;
        return callback(null, data);
      }

      this.pos = 0;
      this.state = 1;

      if (+options["BODY-UNXOR"].skip) {
        data = data.slice(+options["BODY-UNXOR"].skip);

      }
      if (options["BODY-UNXOR"].key !== undefined) {
        this.key = new Buffer(options["BODY-UNXOR"].key, "hex");
      } else if (+options["BODY-UNXOR"].keyLength) {
        this.key = data.slice(0, +options["BODY-UNXOR"].keyLength);
        data = data.slice(+options["BODY-UNXOR"].keyLength);
      } else {
        this.state = 2;
      }
    }

    if (this.state === 1) {
      var pos = this.pos;
      var key = this.key;
      for (var i = 0; i < data.length; i++) {
        data[i] ^= key[pos];
        pos = (pos + 1) % key.length;
      }
      this.pos = pos;
    }

    callback(null, data);
  });
}
////////////////////////////////////////////////////////////////////////////////
function createUnxorBruteGzip (options, context) {
  return through(function(data, encoding, callback) {
    if (this.state === undefined) {
      if (!options["BODY-UNXORBRUTEGZ"]) {
        this.state = 2;
        return callback(null, data);
      }

      var gzip = new Buffer("1f8b08000000000002", "hex");
      var tmp = Buffer.alloc(gzip.length*2);
      this.state = 2;

      done:
      for (var klen = 1; klen <= 4; klen++) {
        var key = Buffer.alloc(klen);
        for (var d = 0; d < data.length - gzip.length; d++) {
          for (var k = 0; k < klen; k++) {
            key[k] = data[d+k] ^ gzip[k];
          }
          for (var i = 0; i < gzip.length; i++) {
            tmp[i] = data[d+i] ^ key[i%key.length];
          }
          for (var j = 0; j < gzip.length; j++) {
            if (tmp[j] !== gzip[j]) {
              break;
            }
          }
          if (j === gzip.length) {
            data = data.slice(d);
            for (var i = 0; i < gzip.length+4; i++) {
              tmp[i] = data[d+i] ^ key[(i%key.length)];
            }
            this.key = key;
            this.pos = 0;
            this.state = 1;
            break done;
          }
        }
      }
    }

    if (this.state === 1) {
      var pos = this.pos;
      var key = this.key;
      for (var i = 0; i < data.length; i++) {
        data[i] ^= key[pos];
        pos = (pos + 1) % key.length;
      }
      this.pos = pos;
    }

    callback(null, data);
  });
}

////////////////////////////////////////////////////////////////////////////////
function CollectBodyStream(collector, item, headerInfo) {
  Writable.call(this);
  mkname(this, "CollectBodyStream");

  this.collector = collector;
  this.item = item;
  this.headerInfo = headerInfo;

  this.buffers = [];

  this.on('finish', function(err) {
    this.collector.bodyDone(item, Buffer.concat(this.buffers), this.headerInfo);
  });
}
util.inherits(CollectBodyStream, Writable);
CollectBodyStream.prototype._write = function(chunk, encoding, callback) {
  this.buffers.push(chunk);
  callback(null);
};

////////////////////////////////////////////////////////////////////////////////
function ItemSMTPStream(options) {
  ItemTransform.call(this, {maxPeekItems: 3});
  mkname(this, "ItemSMTPStream");
  this.states = [ItemSMTPStream.STATES.cmd, ItemSMTPStream.STATES.cmd];
  this.buffers = [];
  this.options = options;
  this.bodyNum = 0;
  this.runningStreams = 0;
  this.itemPos = 0;
}
util.inherits(ItemSMTPStream, ItemTransform);
ItemSMTPStream.STATES = {
    cmd: 1,
    header: 2,
    data: 3,
    mime: 4,
    mime_data: 5,
    ignore: 6
};

ItemSMTPStream.prototype._shouldProcess = function(item) {
  return (item.data.length >= 4 && item.data.slice(0,4).toString().match(/(HELO|EHLO)/));
}

ItemSMTPStream.prototype._process = function(item, callback) {
  var self = this;
  var lines = item.data.toString("binary").replace(/\r?\n$/, '').split(/\r?\n|\r/);
  var state = this.states[item.client];
  var header = "";
  var mime;
  var boundaries = {};
  var matches;
  var bodyType = "file";
  var bodyName = "unknown";
  var boundary;

  function addBuffer(newState, mimeData) {

    if (mimeData) {
      var headerInfo = {bodyType: bodyType, bodyName: bodyName, bodyNum: ++self.bodyNum, itemPos: ++self.itemPos};
      var bufferStream = new Stream.PassThrough();
      var order = self.options["ITEM-SMTP"]?self.options["ITEM-SMTP"].order || []:[];
      var pipes = exports.createPipeline(self.options, order, bufferStream, headerInfo);
      self.runningStreams++;
      var heb = new CollectBodyStream(self, item, headerInfo);
      pipes[pipes.length-1].pipe(heb);
      bufferStream.end(new Buffer(self.buffers.join("\n")+"\n"));
    } else {
      var buf = {client: item.client,
                     ts: item.ts,
                   data: new Buffer(self.buffers.join("\n")+"\n"),
                itemPos: ++self.itemPos};

      self.push(buf);
    }
    state = newState;
    self.buffers = [];
  }

  linesloop:
  for (var l = 0, llen = lines.length; l < llen; l++) {
    switch (state) {
    case ItemSMTPStream.STATES.cmd:
      this.buffers.push(lines[l]);

      if (lines[l].toUpperCase() === "DATA") {
        state = ItemSMTPStream.STATES.header;
        header = "";
        boundaries = {};
      } else if (lines[l].toUpperCase() === "STARTTLS") {
        state = ItemSMTPStream.STATES.ignore;
      }
      break;
    case ItemSMTPStream.STATES.header:
      this.buffers.push(lines[l]);
      if (lines[l][0] === " " || lines[l][0] === "\t") {
        header += lines[l];
        continue;
      }
      if (header.substr(0, 13).toLowerCase() === "content-type:") {
        if ((matches = header.match(/boundary\s*=\s*("?)([^"]*)\1/))) {
          boundaries[matches[2]] = 1;
        }
      }
      if (lines[l] === "") {
        state = ItemSMTPStream.STATES.data;
        continue;
      }
      header = lines[l];
      break;
    case ItemSMTPStream.STATES.data:
      this.buffers.push(lines[l]);
      if (lines[l] === ".") {
        state = ItemSMTPStream.STATES.cmd;
        continue;
      }

      if (lines[l][0] === '-') {
        boundary = lines[l].substr(2);
        if (boundary.substr(-2) === "--" && boundaries[boundary.slice(0, -2)]) {
          addBuffer(ItemSMTPStream.STATES.data, false);
          this.buffers.push(lines[l]);
          mime = {line:"", base64:0, doit:0};
          continue linesloop;
        } else if (boundaries[boundary]) {
          addBuffer(ItemSMTPStream.STATES.mime, false);
          this.buffers.push(lines[l]);
          mime = {line:"", base64:0, doit:0};
          continue linesloop;
        }
      }
      break;
    case ItemSMTPStream.STATES.mime:
      if (lines[l] === ".") {
        state = ItemSMTPStream.STATES.cmd;
        this.buffers.push(lines[l]);
        continue;
      }

      this.buffers.push(lines[l]);

      if (lines[l][0] === " " || lines[l][0] === "\t") {
        mime.line += lines[l];
        continue;
      }
      if (!mime) {
          mime = {line:"", base64:0, doit:0};
      }

      if (mime.line.substr(0, 13).toLowerCase() === "content-type:") {
        if ((matches = mime.line.match(/boundary\s*=\s*("?)([^"]*)\1/))) {
          boundaries[matches[2]] = 1;
        }
        if ((matches = mime.line.match(/name\s*=\s*("?)([^"]*)\1/))) {
          bodyName = matches[2];
        }

        if (mime.line.match(/content-type: image/i)) {
          bodyType = "image";
        } else if (mime.line.match(/content-type: text/i)) {
          bodyType = "text";
        } else {
          bodyType = "file";
        }

      } else if (mime.line.match(/content-disposition:/i)) {
        if ((matches = mime.line.match(/filename\s*=\s*("?)([^"]*)\1/))) {
          bodyName = matches[2];
        }
      } else if (mime.line.match(/content-transfer-encoding:.*base64/i)) {
        mime.base64 = 1;
        mime.doit = 1;
      }
      if (lines[l] === "") {
        addBuffer(ItemSMTPStream.STATES.mimedata, false);
        continue;
      }
      mime.line = lines[l];
      break;
    case ItemSMTPStream.STATES.mimedata:
      if (lines[l] === ".") {
        addBuffer(ItemSMTPStream.STATES.cmd, true);
        this.buffers.push(lines[l]);
        continue;
      }

      if (lines[l][0] === '-') {
        boundary = lines[l].substr(2);
        if (boundary.substr(-2) === "--" && boundaries[boundary.slice(0, -2)]) {
          addBuffer(ItemSMTPStream.STATES.data, mime.doit === 1);
          this.buffers.push(lines[l]);
          mime = {line:"", base64:0, doit:0};
          continue linesloop;
        } else if (boundaries[boundary]) {
          addBuffer(ItemSMTPStream.STATES.mime, mime.doit === 1);
          this.buffers.push(lines[l]);
          mime = {line:"", base64:0, doit:0};
          continue linesloop;
        }
      }

      this.buffers.push(lines[l]);
      break;
    }
  }
  this.states[item.client] = state;

  if (this.buffers.length > 0) {
    addBuffer(ItemSMTPStream.STATES.cmd, false);
  }
  callback();
};
ItemSMTPStream.prototype.bodyDone = function (item, data, headerInfo) {
  this.push({client: item.client, ts: item.ts, data: data, bodyNum: headerInfo.bodyNum, bodyType: headerInfo.bodyType, bodyName: headerInfo.bodyName, itemPos: headerInfo.itemPos});
  this.runningStreams--;
  if (this.runningStreams === 0 && this.endCb) {
    this.endCb();
  }
};
ItemSMTPStream.prototype._finish = function(callback) {
  if (this.runningStreams > 0) {
    this.endCb = callback;
  } else {
    setImmediate(callback);
  }
};
////////////////////////////////////////////////////////////////////////////////
function ItemHTTPStream(options) {
  ItemTransform.call(this, {maxPeekItems: 2});
  mkname(this, "ItemHTTPStream");
  this.bodyNum = 0;
  this.options = options;
  this.runningStreams = 0;
}
util.inherits(ItemHTTPStream, ItemTransform);
ItemHTTPStream.onBody = function(buf, start, len) {
  //console.log("onBody", start, len, item);
  var item = this.curitem;
  delete this.curitem;
  if (!this.bufferStream) {
    this.httpstream.push({client: item.client, ts: item.ts, data: buf.slice(0, start)});
    this.httpstream.runningStreams++;
    this.bufferStream = new Stream.PassThrough();
    mkname(this.bufferStream, "bufferStream");

    var order = this.httpstream.options["ITEM-HTTP"]?this.httpstream.options["ITEM-HTTP"].order || []:[];
    var pipes = exports.createPipeline(this.httpstream.options, order, this.bufferStream, this.headerInfo);

    item.bodyNum = ++this.httpstream.bodyNum;
    item.bodyName = this.httpstream.bodyName;
    var heb = new CollectBodyStream(this.httpstream, item, this.headerInfo);
    pipes[pipes.length-1].pipe(heb);

    delete this.headerInfo;
  }

  this.bufferStream.write(buf.slice(start, start+len));
};

ItemHTTPStream.onMessageComplete = function() {
  //console.log("onMessageComplete", this.bufferStream?"bufferStream":"no bufferStream");
  if (this.bufferStream) {
    this.bufferStream.end();
    delete this.bufferStream;
  } else {
    this.httpstream.push(this.curitem);
  }
  delete this.curitem;
};

ItemHTTPStream.onHeadersComplete = function(major, minor, headers, method, url) {
  //console.log("onHeadersComplete", headers, method, url);
  var info = {headersMap: {}};
  for (var i = 0; i < headers.length; i += 2) {
    info.headersMap[headers[i].toLowerCase()] = headers[i+1];
  }
  this.headerInfo = info;
  if (url)
    this.httpstream.bodyName = url.split(/[\/?=]/).pop();
};

ItemHTTPStream.prototype._shouldProcess = function (item) {
  return (item.data.length >= 4 && item.data.slice(0,4).toString() === "HTTP");
};

ItemHTTPStream.prototype._process = function (item, callback) {
  //console.trace("_process", item);
  if (this.parsers === undefined) {
    if (item.data.slice(0,4).toString() === "HTTP") {
      this.parsers = [new HTTPParser(HTTPParser.RESPONSE), new HTTPParser(HTTPParser.REQUEST)];
    } else {
      this.parsers = [new HTTPParser(HTTPParser.REQUEST), new HTTPParser(HTTPParser.RESPONSE)];
    }
    this.parsers[0].httpstream = this.parsers[1].httpstream = this;
    this.parsers[0][HTTPParser.kOnBody] = this.parsers[1][HTTPParser.kOnBody] = ItemHTTPStream.onBody;
    this.parsers[0][HTTPParser.kOnMessageComplete] = this.parsers[1][HTTPParser.kOnMessageComplete] = ItemHTTPStream.onMessageComplete;
    this.parsers[0][HTTPParser.kOnHeadersComplete] = this.parsers[1][HTTPParser.kOnHeadersComplete] = ItemHTTPStream.onHeadersComplete;
  }

  if (item.data.length === 0) {
    this.push(item);
  } else {
    this.parsers[item.client].curitem = item;
    var out = this.parsers[item.client].execute(item.data, 0, item.data.length);
    if (typeof out === "object") {
      this.push(item);
    }
  }
  setImmediate(callback);
};

ItemHTTPStream.prototype.bodyDone = function (item, data, headerInfo) {
  var bodyType = "file";
  if (headerInfo.headersMap && headerInfo.headersMap['content-type']) {
    if (headerInfo.headersMap['content-type'].match(/^image/i)) {
      bodyType = "image";
    } else if (headerInfo.headersMap['content-type'].match(/^text/i)) {
      bodyType = "text";
    }
  }

  var bodyName = item.bodyName || bodyType + item.bodyNum;
  this.push({client: item.client, ts: item.ts, data: data, bodyNum: item.bodyNum, bodyType: bodyType, bodyName: bodyName});
  this.runningStreams--;
  if (this.runningStreams === 0 && this.endCb) {
    this.endCb();
  }
};
ItemHTTPStream.prototype._finish = function (callback) {
  var self = this;
  this.parsers[0].finish();
  this.parsers[1].finish();
  if (this.parsers[0].curitem) {
    this.push(this.parsers[0].curitem);
  }
  if (this.parsers[1].curitem) {
    this.push(this.parsers[1].curitem);
  }
  var streams = 0;
  async.each([this.parsers[0].bufferStream, this.parsers[1].bufferStream], function (stream, cb) {
    if (stream) {
      return stream.end(cb);
    }
    cb();
  }, function () {
    if (self.runningStreams > 0 || streams > 0) {
      self.endCb = callback;
    } else {
      setImmediate(callback);
    }
  });
};

////////////////////////////////////////////////////////////////////////////////
function ItemHexFormaterStream(options) {
  Transform.call(this, {objectMode: true});
  mkname(this, "ItemHexFormaterStream");
  this.showOffsets = options["ITEM-HEX"]?options["ITEM-HEX"].showOffsets||false:false;
}
util.inherits(ItemHexFormaterStream, Transform);
ItemHexFormaterStream.prototype._transform = function (item, encoding, callback) {
  if (item.html !== undefined) {
    return callback(null, item);
  }

  var out = "<pre>";
  var i, ilen;

  var input = item.data;
  for (var pos = 0, poslen = input.length; pos < poslen; pos += 16) {
    var line = input.slice(pos, Math.min(pos+16, input.length));
    if (this.showOffsets) {
      out += sprintf.sprintf("<span class=\"sessionln\">%08d:</span> ", pos);
    }

    for (i = 0; i < 16; i++) {
      if (i % 2 === 0 && i > 0) {
        out += " ";
      }
      if (i < line.length) {
        out += sprintf.sprintf("%02x", line[i]);
      } else {
        out += "  ";
      }
    }

    out += " ";

    for (i = 0, ilen = line.length; i < ilen; i++) {
      if (line[i] <= 32 || line[i]  > 128) {
        out += ".";
      } else {
        out += safeStr(line.toString("ascii", i, i+1));
      }
    }
    out += "\n";
  }
  item.html = out + "</pre>";
  callback(null, item);
};
////////////////////////////////////////////////////////////////////////////////
function createItemSorterStream (options) {
  var stream = through.obj(function(item, encoding, callback) {
    if (item.itemPos === undefined) {
      item.itemPos = this.items.length;
    }
    this.items.push(item);
    callback();
  }, function (callback) {
    this.items.sort(function (a, b) {
      if (a.ts === b.ts) {
        return a.itemPos - b.itemPos;
      }
      return a.ts - b.ts;
    });

    for (var i = 0; i < this.items.length; i++) {
      this.push(this.items[i]);
    }

    callback();
  });
  stream.items = [];
  return stream;
}
////////////////////////////////////////////////////////////////////////////////

module.exports = exports = {};

exports.register = function(name, classOrCreate, settings) {
  internals.registry[name] = classOrCreate;
  if (settings) {
    internals.settings[name] = settings;
  }
};
exports.settings = function() {
  return internals.settings;
}

exports.register("BODY-UNXORBRUTEGZ", createUnxorBruteGzip, {name: "UnXOR Brute GZip Header"});
exports.register("BODY-UNXOR", createKeyUnxorStream,
  {name: "UnXOR",
   title: "Only set keyLength or key",
  fields: [{key: "skip", name: "Skip Bytes", type: "text"},
           {key:"keyLength", name:"Key is in data length", type: "text"},
           {key:"key", name:"Fixed key in hex", type: "text"}
          ]});

exports.register("BODY-UNCOMPRESS", createUncompressStream);
exports.register("BODY-UNBASE64", createUnbase64Stream, {name: "Unbase64"});

exports.register("ITEM-HTTP", ItemHTTPStream);
exports.register("ITEM-SMTP", ItemSMTPStream);
exports.register("ITEM-SORTER", createItemSorterStream);
exports.register("ITEM-PRINTER", through.ctor({objectMode: true}, function(item, encoding, callback) {
  var data = item.html || item.data.toString();
  console.log(item.ts, item.client, item.itemPos, item.bodyNum, item.bodyType, item.bodyName, data.length, data);
  console.log();
  callback(null, item);
}));
exports.register("ITEM-HEX", ItemHexFormaterStream);
exports.register("ITEM-UTF8", through.ctor({objectMode: true}, function(item, encoding, callback) {
  if (item.html === undefined) {
    item.html = "<pre>" + safeStr(item.data.toString("utf8")) + "</pre>";
  }
  callback(null, item);
}));
exports.register("ITEM-ASCII", through.ctor({objectMode: true}, function(item, encoding, callback) {
  if (item.html === undefined) {
    item.html = "<pre>" + safeStr(item.data.toString("binary")) + "</pre>";
  }
  callback(null, item);
}));
exports.register("ITEM-NATURAL", through.ctor({objectMode: true}, function(item, encoding, callback) {
  if (item.html === undefined) {
    item.html = safeStr(item.data.toString()).replace(/\r?\n/g, '<br>');
  }
  callback(null, item);
}));
exports.register("ITEM-BYTES", through.ctor({objectMode: true}, function(item, encoding, callback) {
  item.bytes = item.data.length;
  callback(null, item);
}));
exports.register("ITEM-RAWBODY", through.ctor({objectMode: true}, function(item, encoding, callback) {
  if ((item.bodyNum !== undefined) && (item.bodyNum === this.options["ITEM-RAWBODY"].bodyNumber)) {
    return callback(null, item);
  }
  return callback();
}));
exports.register("ITEM-LINKBODY", through.ctor({objectMode: true}, function(item, encoding, callback) {
  if (item.bodyType === undefined || item.bodyType === "text") {
    return callback(null, item);
  }
  var url = this.options.nodeName + "/" +
            this.options.id + "/body/" +
            item.bodyType + "/" +
            item.bodyNum + "/" +
            item.bodyName + ".pellet";

  if (item.bodyType === "image") {
    item.html = "<img src=\"" + url + "\">";
  } else {
    item.html = "<a target='_blank' class='imagetag file' href=\"" + url + "\">" + item.bodyName + "</a>";
  }
  callback(null, item);
}));
exports.register("ITEM-CB", through.ctor({objectMode: true}, function(item, encoding, callback) {
  if (this.items === undefined) {
    this.items = [];
  }
  this.items.push(item);
  callback();
}, function(callback) {
  this.options["ITEM-CB"].cb(null, this.items);
  callback();
}));


exports.Pcap2ItemStream = Pcap2ItemStream;
exports.createPipeline = function(options, order, stream, context) {
  var pipes = [stream];

  function link (p) {
    pipes[p].pipe(pipes[p+1]).on("error", function (err) {
      console.trace("ERROR", order[p], err);
    });
  }

  for (var i = 0; i < order.length; i++) {
    var classOrCreate = internals.registry[order[i]];
    if (!classOrCreate) {
      console.trace("ERROR - Couldn't find", order[i], "in decode registry");
      return;
    }
    if (classOrCreate.super_) {
      pipes.push(new classOrCreate(options, context));
    } else {
      pipes.push(classOrCreate(options, context));
    }

    // We are really i+1 in pipes when we push above
    link(i);
  }
  return pipes;
};

// Run directly, testing code
if(require.main === module) {
  var options = {
    nodeName: "nodeName",
    id: "id",
    order: [],
    "ITEM-HTTP": {
      order: []
    },
    "ITEM-SMTP": {
      order: []
    }
  };

  var base = "ITEM-NATURAL";
  var filename;
  var ending = ["ITEM-SORTER", "ITEM-PRINTER"];
  for (var aa = 2; aa < process.argv.length; aa++) {
    if (process.argv[aa] === "--hex") {
      base = "ITEM-HEX";
    } else if (process.argv[aa] === "--ascii") {
      base = "ITEM-ASCII";
    } else if (process.argv[aa] === "--natural") {
      base = "ITEM-NATURAL";
    } else if (process.argv[aa] === "--utf8") {
      base = "ITEM-UTF8";
    } else if (process.argv[aa] === "--line") {
      options["ITEM-HEX"] = {showOffsets: true};
    } else if (process.argv[aa] === "--uncompress") {
      options["ITEM-HTTP"].order.push("BODY-UNCOMPRESS");
      options["ITEM-SMTP"].order.push("BODY-UNCOMPRESS");
    } else if (process.argv[aa] === "--unbase64") {
      options["ITEM-SMTP"].order.push("BODY-UNBASE64");
    } else if (process.argv[aa] === "--unxorbrutegz") {
      options["ITEM-HTTP"].order.push("BODY-UNXORBRUTEGZ");
      options["BODY-UNXORBRUTEGZ"] = {};
    } else if (process.argv[aa] === "--unxor") {
      options["ITEM-HTTP"].order.push("BODY-UNXOR");
      options["BODY-UNXOR"] = {skip: +process.argv[aa+1], keyLength: process.argv[aa+2]};
      aa += 2;
    } else if (process.argv[aa] === "--links") {
      base = "ITEM-LINKBODY";
    } else if (process.argv[aa] === "--bodynum") {
      aa++;
      options["ITEM-RAWBODY"] = {bodyNumber: +process.argv[aa]};
      base = "ITEM-RAWBODY";
    } else {
      filename = process.argv[aa];
    }
  }


  options.order.push("ITEM-HTTP");
  options.order.push("ITEM-SMTP");

  options.order = options.order.concat("ITEM-BYTES", base, ending);
  console.log(options);

  if (!filename) {
    console.log("ERROR, must provide a file");
  } else if (filename.match(/\.pcap$/)) {
    var Pcap = require('./pcap.js');
    var fs = require('fs');
    var pcap = new Pcap(filename);
    pcap.open(filename);
    var stat = fs.statSync(filename);
    var pos = 24;
    var packets = [];

    async.whilst(
        function () { return pos < stat.size;},
        function (callback) {
          pcap.readPacket(pos, function(packet) {
            var obj = {};
            pcap.decode(packet, obj);
            packets.push(obj);
            pos += packet.length;
            callback();
          });
        },
        function (err, n) {
          Pcap.reassemble_tcp(packets, packets[0].ip.addr1 + ':' + packets[0].tcp.sport, function(err, results) {
            exports.createPipeline(options, options.order, new Pcap2ItemStream(options, results));
          });
        }
    );

    console.log("process", filename);
  } else {
    var data   = require("./" + filename);
    exports.createPipeline(options, options.order, new Pcap2ItemStream(options, data));
  }


}
