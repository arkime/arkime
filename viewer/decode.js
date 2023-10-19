/******************************************************************************/
/* decode.js  -- The pcap decoding code.
 *
 * To fix bugs you can just do `node decode.js [OPTIONS] [FILE]`
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const Stream = require('stream');
const Readable = Stream.Readable;
const Transform = Stream.Transform;
const Writable = Stream.Writable;
const zlib = require('zlib');
const through = require('through2');
const peek = require('peek-stream');
const async = require('async');
const cryptoLib = require('crypto');
const ArkimeUtil = require('../common/arkimeUtil');

const internals = {
  registry: {},
  settings: {},
  debug: 0
};

/// /////////////////////////////////////////////////////////////////////////////
function mkname (stream, streamName) {
  if (!mkname.cnt) { mkname.cnt = {}; }
  if (!mkname.cnt[streamName]) { mkname.cnt[streamName] = 0; }
  mkname.cnt[streamName]++;
  stream.arkimeName = streamName + '-' + mkname.cnt[streamName];
}
/// /////////////////////////////////////////////////////////////////////////////
class ItemTransform extends Transform {
  constructor (options) {
    super({ objectMode: true });
    this._itemTransform = {
      state: 0,
      max: options.maxPeekItems || 1,
      items: []
    };
  }

  _transform (item, encoding, callback) {
    if (!item) {
      return callback();
    }

    const self = this;
    switch (self._itemTransform.state) {
    case 0:
      self._itemTransform.items.push(item);
      if (self._shouldProcess(item)) {
        self._itemTransform.state = 1;
        async.each(self._itemTransform.items, function (eachItem, cb) {
          try {
            self._process(eachItem, function (err, data) {
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
      try {
        return self._process(item, callback);
      } catch (err) {
        return callback(err);
      }
    case 2:
      return callback(null, item);
    }
  };

  _flush (callback) {
    if (this._itemTransform.state === 1) {
      return this._finish(callback);
    }

    let item;
    while ((item = this._itemTransform.items.shift())) {
      this.push(item);
    }

    callback();
  };
}

/// /////////////////////////////////////////////////////////////////////////////
class Pcap2ItemStream extends Readable {
  constructor (options, pcap) {
    super({ objectMode: true });
    mkname(this, 'Pcap2ItemStream');
    this.data = pcap || [];
  }

  _read (size) {
    const data = this.data;
    for (let i = 0; i < data.length; i++) {
      data[i].client = (i % 2);
      this.push(data[i]);
    }
    this.push(null);
  };
}

/// /////////////////////////////////////////////////////////////////////////////
function createUncompressStream (options, context) {
  return peek({ newline: false, maxBuffer: 3 }, function (data, swap) {
    if (data.length < 3) {
      return swap(null, through());
    }

    if (data[0] === 0x1f && data[1] === 0x8b && data[2] === 8) {
      if (context.headersMap && context.headersMap['content-encoding'] === 'deflate') {
        context.headersMap['content-encoding'] = 'arkime-gzip';
      }
      const s = zlib.createGunzip();
      return swap(null, s);
    }

    /* if (context.headersMap && context.headersMap["content-encoding"] === "deflate") {
      context.headersMap["content-encoding"] = "arkime-deflate";
      return swap(null, zlib.createDeflate());
    } */

    return swap(null, through());
  });
}

/// /////////////////////////////////////////////////////////////////////////////
function createUnbase64Stream (options, context) {
  return through(function (data, encoding, callback) {
    callback(null, Buffer.from(data.toString('binary'), 'base64'));
  });
}

/// /////////////////////////////////////////////////////////////////////////////
function createKeyUnxorStream (options, context) {
  return through(function (data, encoding, callback) {
    if (this.state === undefined) {
      if (!options['BODY-UNXOR']) {
        this.state = 2;
        return callback(null, data);
      }

      this.pos = 0;
      this.state = 1;

      if (+options['BODY-UNXOR'].skip) {
        data = data.slice(+options['BODY-UNXOR'].skip);
      }
      if (options['BODY-UNXOR'].key !== undefined) {
        this.key = Buffer.from(options['BODY-UNXOR'].key, 'hex');
      } else if (+options['BODY-UNXOR'].keyLength) {
        this.key = data.slice(0, +options['BODY-UNXOR'].keyLength);
        data = data.slice(+options['BODY-UNXOR'].keyLength);
      } else {
        this.state = 2;
      }
    }

    if (this.state === 1) {
      let pos = this.pos;
      const key = this.key;
      for (let i = 0; i < data.length; i++) {
        data[i] ^= key[pos];
        pos = (pos + 1) % key.length;
      }
      this.pos = pos;
    }

    callback(null, data);
  });
}
/// /////////////////////////////////////////////////////////////////////////////
function createUnxorBruteGzip (options, context) {
  return through(function (data, encoding, callback) {
    if (this.state === undefined) {
      if (!options['BODY-UNXORBRUTEGZ']) {
        this.state = 2;
        return callback(null, data);
      }

      const gzip = Buffer.from('1f8b08000000000002', 'hex');
      const tmp = Buffer.alloc(gzip.length * 2);
      this.state = 2;

      done:
      for (let klen = 1; klen <= 4; klen++) {
        const key = Buffer.alloc(klen);
        for (let d = 0; d < data.length - gzip.length; d++) {
          for (let k = 0; k < klen; k++) {
            key[k] = data[d + k] ^ gzip[k];
          }
          for (let g = 0; g < gzip.length; g++) {
            tmp[g] = data[d + g] ^ key[g % key.length];
          }
          let j;
          for (j = 0; j < gzip.length; j++) {
            if (tmp[j] !== gzip[j]) {
              break;
            }
          }
          if (j === gzip.length) {
            data = data.slice(d);
            for (let i = 0; i < gzip.length + 4; i++) {
              tmp[i] = data[d + i] ^ key[(i % key.length)];
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
      let pos = this.pos;
      const key2 = this.key;
      for (let l = 0; l < data.length; l++) {
        data[l] ^= key2[pos];
        pos = (pos + 1) % key2.length;
      }
      this.pos = pos;
    }

    callback(null, data);
  });
}

/// /////////////////////////////////////////////////////////////////////////////
class CollectBodyStream extends Writable {
  constructor (collector, item, headerInfo) {
    super();
    mkname(this, 'CollectBodyStream');

    this.collector = collector;
    this.item = item;
    this.headerInfo = headerInfo;

    this.buffers = [];

    this.on('finish', function (err) {
      this.collector.bodyDone(item, Buffer.concat(this.buffers), this.headerInfo);
    });
  }

  _write (chunk, encoding, callback) {
    this.buffers.push(chunk);
    callback(null);
  };
}

/// /////////////////////////////////////////////////////////////////////////////
class ItemSMTPStream extends ItemTransform {
  static STATES = {
    cmd: 1,
    header: 2,
    data: 3,
    mime: 4,
    mime_data: 5,
    ignore: 6
  };

  constructor (options) {
    super({ maxPeekItems: 3 });
    mkname(this, 'ItemSMTPStream');
    this.states = [ItemSMTPStream.STATES.cmd, ItemSMTPStream.STATES.cmd];
    this.buffers = [];
    this.options = options;
    this.bodyNum = 0;
    this.runningStreams = 0;
    this.itemPos = 0;
  }

  _shouldProcess (item) {
    return (item.data.length >= 4 && item.data.slice(0, 4).toString().match(/(HELO|EHLO)/));
  };

  _process (item, callback) {
    const self = this;
    const lines = item.data.toString('binary').replace(/\r?\n$/, '').split(/\r?\n|\r/);
    let state = this.states[item.client];
    let header = '';
    let mime;
    let boundaries = {};
    let matches;
    let bodyType = 'file';
    let bodyName = 'unknown';
    let boundary;

    if (internals.debug > 0) {
      console.log('ItemSMTPStream._process', item);
    }

    function addBuffer (newState, mimeData) {
      if (mimeData) {
        const headerInfo = { bodyType, bodyName, bodyNum: ++self.bodyNum, itemPos: ++self.itemPos };
        const bufferStream = new Stream.PassThrough();
        const order = self.options['ITEM-SMTP'] ? self.options['ITEM-SMTP'].order || [] : [];
        const pipes = exports.createPipeline(self.options, order, bufferStream, headerInfo);
        self.runningStreams++;
        const heb = new CollectBodyStream(self, item, headerInfo);
        pipes[pipes.length - 1].pipe(heb);
        bufferStream.end(Buffer.from(self.buffers.join('\n') + '\n'));
      } else {
        const buf = {
          client: item.client,
          ts: item.ts,
          data: Buffer.from(self.buffers.join('\n') + '\n'),
          itemPos: ++self.itemPos
        };

        self.push(buf);
      }
      state = newState;
      self.buffers = [];
    }

    linesloop:
    for (let l = 0, llen = lines.length; l < llen; l++) {
      switch (state) {
      case ItemSMTPStream.STATES.cmd:
        this.buffers.push(lines[l]);

        if (lines[l].toUpperCase() === 'DATA') {
          state = ItemSMTPStream.STATES.header;
          header = '';
          boundaries = {};
        } else if (lines[l].toUpperCase() === 'STARTTLS') {
          state = ItemSMTPStream.STATES.ignore;
        }
        break;
      case ItemSMTPStream.STATES.header:
        this.buffers.push(lines[l]);
        if (lines[l][0] === ' ' || lines[l][0] === '\t') {
          header += lines[l];
          continue;
        }
        if (header.substr(0, 13).toLowerCase() === 'content-type:') {
          if ((matches = header.match(/boundary\s*=\s*("?)([^"]*)\1/))) {
            boundaries[matches[2]] = 1;
          }
        }
        if (lines[l] === '') {
          state = ItemSMTPStream.STATES.data;
          continue;
        }
        header = lines[l];
        break;
      case ItemSMTPStream.STATES.data:
        this.buffers.push(lines[l]);
        if (lines[l] === '.') {
          state = ItemSMTPStream.STATES.cmd;
          continue;
        }

        if (lines[l][0] === '-') {
          boundary = lines[l].substr(2);
          if (boundary.substr(-2) === '--' && boundaries[boundary.slice(0, -2)]) {
            addBuffer(ItemSMTPStream.STATES.data, false);
            this.buffers.push(lines[l]);
            mime = { line: '', base64: 0, doit: 0 };
            continue linesloop;
          } else if (boundaries[boundary]) {
            addBuffer(ItemSMTPStream.STATES.mime, false);
            this.buffers.push(lines[l]);
            mime = { line: '', base64: 0, doit: 0 };
            continue linesloop;
          }
        }
        break;
      case ItemSMTPStream.STATES.mime:
        if (lines[l] === '.') {
          state = ItemSMTPStream.STATES.cmd;
          this.buffers.push(lines[l]);
          continue;
        }

        this.buffers.push(lines[l]);

        if (lines[l][0] === ' ' || lines[l][0] === '\t') {
          mime.line += lines[l];
          continue;
        }
        if (!mime) {
          mime = { line: '', base64: 0, doit: 0 };
        }

        if (mime.line.substr(0, 13).toLowerCase() === 'content-type:') {
          if ((matches = mime.line.match(/boundary\s*=\s*("?)([^"]*)\1/))) {
            boundaries[matches[2]] = 1;
          }
          if ((matches = mime.line.match(/name\s*=\s*("?)([^"]*)\1/))) {
            bodyName = matches[2];
          }

          if (mime.line.match(/content-type: image/i)) {
            bodyType = 'image';
          } else if (mime.line.match(/content-type: text/i)) {
            bodyType = 'text';
          } else {
            bodyType = 'file';
          }
        } else if (mime.line.match(/content-disposition:/i)) {
          if ((matches = mime.line.match(/filename\s*=\s*("?)([^"]*)\1/))) {
            bodyName = matches[2];
          }
        } else if (mime.line.match(/content-transfer-encoding:.*base64/i)) {
          mime.base64 = 1;
          mime.doit = 1;
        }
        if (lines[l] === '') {
          addBuffer(ItemSMTPStream.STATES.mimedata, false);
          continue;
        }
        mime.line = lines[l];
        break;
      case ItemSMTPStream.STATES.mimedata:
        if (lines[l] === '.') {
          addBuffer(ItemSMTPStream.STATES.cmd, true);
          this.buffers.push(lines[l]);
          continue;
        }

        if (lines[l][0] === '-') {
          boundary = lines[l].substr(2);
          if (boundary.substr(-2) === '--' && boundaries[boundary.slice(0, -2)]) {
            addBuffer(ItemSMTPStream.STATES.data, mime.doit === 1);
            this.buffers.push(lines[l]);
            mime = { line: '', base64: 0, doit: 0 };
            continue linesloop;
          } else if (boundaries[boundary]) {
            addBuffer(ItemSMTPStream.STATES.mime, mime.doit === 1);
            this.buffers.push(lines[l]);
            mime = { line: '', base64: 0, doit: 0 };
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

  bodyDone (item, data, headerInfo) {
    this.push({ client: item.client, ts: item.ts, data, bodyNum: headerInfo.bodyNum, bodyType: headerInfo.bodyType, bodyName: headerInfo.bodyName, itemPos: headerInfo.itemPos });
    this.runningStreams--;
    if (this.runningStreams === 0 && this.endCb) {
      this.endCb();
    }
  };

  _finish (callback) {
    if (this.runningStreams > 0) {
      this.endCb = callback;
    } else {
      setImmediate(callback);
    }
  };
}
/// /////////////////////////////////////////////////////////////////////////////
class ItemHTTPStream extends ItemTransform {
  static STATES = {
    start: 1,
    req: 2,
    req_body: 3,
    req_body_chunk: 4,

    res: 5,
    res_body: 6,
    res_body_chunk: 7,

    pass: 8
  };

  options;
  states;
  method;
  code;
  buffers = [];
  contentLength = [];
  transferEncoding = [];
  contentType = ['', ''];
  startPos = [0, 0];
  bodyNum = 0;
  runningStreams = 0;
  itemPos = 0;

  constructor (options) {
    super({ maxPeekItems: 3 });
    mkname(this, 'ItemHTTPStream');
    this.options = options;
    this.states = [ItemHTTPStream.STATES.start, ItemHTTPStream.STATES.start];
  }

  _shouldProcess (item) {
    return (item.data.length >= 4 && item.data.slice(0, 4).toString() === 'HTTP');
  };

  add (item, endPos) {
    this.itemPos++;
    const buf = {
      client: item.client,
      ts: item.ts,
      data: item.data.slice(this.startPos[item.client], endPos),
      itemPos: this.itemPos
    };

    this.startPos[item.client] = endPos;
    this.push(buf);
  }

  processText (item, pos) {
    let endPos = pos;
    while (endPos <= item.data.length && item.data[endPos] !== 0x0a) {
      endPos++;
    }
    let line, upper;
    if (endPos > pos && item.data[endPos - 1] === 0x0d) {
      line = item.data.slice(pos, endPos - 1).toString();
    } else {
      line = item.data.slice(pos, endPos).toString();
    }

    switch (this.states[item.client]) {
    case ItemHTTPStream.STATES.start:
      upper = line;
      this.contentLength[item.client] = 0;
      this.transferEncoding[item.client] = '';
      if (upper.startsWith('CONNECT')) {
        this.states[item.client] = ItemHTTPStream.STATES.req;
        this.method = 'CONNECT';
      } else if (upper.startsWith('HTTP')) {
        this.states[item.client] = ItemHTTPStream.STATES.res;
        const parts = upper.split(/ +/);
        this.code = +parts[1];
      } else if (upper.length > 0) {
        this.states[item.client] = ItemHTTPStream.STATES.req;
        this.method = upper.split(' ')[0];
        const parts = upper.split(/ +/);
        this.url = parts[1];
      }
      break;

    case ItemHTTPStream.STATES.req:
      if (line.length === 0) {
        if (this.method.match(/^(GET|HEAD|DELETE|TRACE)$/)) {
          this.states[item.client] = ItemHTTPStream.STATES.start;
        } else if (this.method.match(/^(CONNECT)$/)) {
          this.states[item.client] = ItemHTTPStream.STATES.pass;
        } else if (this.transferEncoding[item.client] === 'CHUNKED') {
          this.states[item.client] = ItemHTTPStream.STATES.req_body_chunk;
        } else {
          this.states[item.client] = ItemHTTPStream.STATES.req_body;
        }
        this.add(item, endPos + 1);
        break;
      }
      upper = line.toUpperCase();
      if (upper.startsWith('CONTENT-LENGTH')) {
        this.contentLength[item.client] = +upper.substring(15);
      } else if (upper.startsWith('CONTENT-TYPE')) {
        this.contentType[item.client] = upper.substring(14);
      } else if (upper.startsWith('TRANSFER-ENCODING')) {
        this.transferEncoding[item.client] = upper.substring(19);
      }
      break;

    case ItemHTTPStream.STATES.req_body_chunk:
      if (line.length === 0) { break; }
      this.contentLength[item.client] = Number.parseInt(line, 16);
      if (isNaN(this.contentLength[item.client])) { throw new Error('Missing Chunk Length'); }
      if (this.contentLength[item.client] === 0) {
        this.msgEnd(item);
      } else {
        this.states[item.client] = ItemHTTPStream.STATES.res_body;
      }
      break;

    case ItemHTTPStream.STATES.res:
      if (line.length === 0) {
        if (this.code / 100 === 1 || this.code === 204 || this.code === 304) {
          this.states[item.client] = ItemHTTPStream.STATES.start;
        } else if (this.method === undefined) {
        } else if (this.method.match(/^(CONNECT)$/)) {
          this.states[item.client] = ItemHTTPStream.STATES.pass;
        } else if (this.transferEncoding[item.client] === 'CHUNKED') {
          this.states[item.client] = ItemHTTPStream.STATES.res_body_chunk;
        } else {
          this.states[item.client] = ItemHTTPStream.STATES.res_body;
        }
        this.add(item, endPos + 1);
        break;
      }
      upper = line.toUpperCase();
      if (upper.startsWith('CONTENT-LENGTH')) {
        this.contentLength[item.client] = +upper.substring(15);
      } else if (upper.startsWith('CONTENT-TYPE')) {
        this.contentType[item.client] = upper.substring(14);
      } else if (upper.startsWith('TRANSFER-ENCODING')) {
        this.transferEncoding[item.client] = upper.substring(19);
      }
      break;

    case ItemHTTPStream.STATES.res_body_chunk:
      if (line.length === 0) { break; }

      this.contentLength[item.client] = Number.parseInt(line, 16);
      if (isNaN(this.contentLength[item.client])) { throw new Error('Missing Chunk Length'); }
      if (this.contentLength[item.client] === 0) {
        this.msgEnd(item);
      } else {
        this.states[item.client] = ItemHTTPStream.STATES.res_body;
      }
      break;
    } /* switch */

    return endPos + 1;
  }

  msgEnd (item) {
    this.states[item.client] = ItemHTTPStream.STATES.start;
    if (this.bufferStream) {
      this.bufferStream.end();
      delete this.bufferStream;
    }
  }

  processBody (item, pos) {
    if (this.contentLength[item.client]) {
      const avail = item.data.length - pos;
      const used = Math.min(avail, this.contentLength[item.client]);

      if (!this.bufferStream) {
        this.runningStreams++;
        this.bufferStream = new Stream.PassThrough();
        mkname(this.bufferStream, 'bufferStream');

        const info = {
          bodyNum: ++this.bodyNum,
          bodyName: this.url.split(/[/?=]/).pop(),
          bodyType: 'file',
          itemPos: ++this.itemPos
        };

        if (!info.bodyName || info.bodyName.length === 0) {
          info.bodyName = info.bodyType + info.bodyNum;
        }

        if (this.contentType[item.client].match(/^image/i)) {
          info.bodyType = 'image';
        } else if (this.contentType[item.client].match(/^text/i)) {
          info.bodyType = 'text';
        }

        const order = this.options['ITEM-HTTP'] ? this.options['ITEM-HTTP'].order || [] : [];
        const pipes = exports.createPipeline(this.options, order, this.bufferStream, info);

        item.bodyNum = info.bodyNum;
        item.bodyName = info.bodyName;
        const heb = new CollectBodyStream(this, item, info);
        pipes[pipes.length - 1].pipe(heb);
      }

      this.bufferStream.write(item.data.slice(pos, pos + used));

      pos += used;
      this.contentLength[item.client] -= used;
    }
    if (this.contentLength[item.client] === 0) {
      if (this.transferEncoding[item.client] === 'CHUNKED') {
        this.states[item.client] = ItemHTTPStream.STATES.res_body_chunk;
      } else {
        this.msgEnd(item);
      }
    }
    return pos;
  }

  _process (item, callback) {
    this.startPos[item.client] = 0;
    let pos = 0;
    while (pos < item.data.length) {
      const state = this.states[item.client];
      if (state === ItemHTTPStream.STATES.pass) {
        this.push({ client: item.client, ts: item.ts, data: item.data.slice(0, item.data.length), itemPos: ++this.itemPos });
        break;
      } else if (state === ItemHTTPStream.STATES.req_body || state === ItemHTTPStream.STATES.res_body) {
        pos = this.processBody(item, pos);
      } else {
        pos = this.processText(item, pos);
      }
    }

    callback();
  }

  bodyDone (item, data, headerInfo) {
    this.push({ client: item.client, ts: item.ts, data, bodyNum: headerInfo.bodyNum, bodyType: headerInfo.bodyType, bodyName: headerInfo.bodyName, itemPos: headerInfo.itemPos });
    this.runningStreams--;
    if (this.runningStreams === 0 && this.endCb) {
      this.endCb();
    }
  }

  _finish (callback) {
    if (this.runningStreams > 0) {
      // Since we might not get all the packets, end any current buffer stream
      if (this.bufferStream) {
        this.bufferStream.end();
        delete this.bufferStream;
      }
      this.endCb = callback;
    } else {
      setImmediate(callback);
    }
  }
};

/// /////////////////////////////////////////////////////////////////////////////
class ItemHexFormaterStream extends Transform {
  constructor (options) {
    super({ objectMode: true });
    mkname(this, 'ItemHexFormaterStream');
    this.showOffsets = options['ITEM-HEX'] ? options['ITEM-HEX'].showOffsets || false : false;
  }

  _transform (item, encoding, callback) {
    if (item.html !== undefined) {
      return callback(null, item);
    }

    let out = '<pre>';
    let i, ilen;

    const input = item.data;
    for (let pos = 0, poslen = input.length; pos < poslen; pos += 16) {
      const line = input.slice(pos, Math.min(pos + 16, input.length));
      if (this.showOffsets) {
        const paddedPos = pos.toString().padStart(8, '0');
        out += '<span class="sessionln">' + paddedPos + ':</span> ';
      }

      for (i = 0; i < 16; i++) {
        if (i % 2 === 0 && i > 0) {
          out += ' ';
        }
        if (i < line.length) {
          const paddedLine = line[i].toString(16).padStart(2, '0');
          out += paddedLine;
        } else {
          out += '  ';
        }
      }

      out += ' ';

      for (i = 0, ilen = line.length; i < ilen; i++) {
        if (line[i] <= 32 || line[i] > 128) {
          out += '.';
        } else {
          out += ArkimeUtil.safeStr(line.toString('ascii', i, i + 1));
        }
      }
      out += '\n';
    }
    item.html = out + '</pre>';
    callback(null, item);
  };
}
/// /////////////////////////////////////////////////////////////////////////////
function createItemSorterStream (options) {
  const stream = through.obj(function (item, encoding, callback) {
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

    for (let i = 0; i < this.items.length; i++) {
      this.push(this.items[i]);
    }

    callback();
  });
  stream.items = [];
  return stream;
}
/// /////////////////////////////////////////////////////////////////////////////

module.exports = exports = {};

exports.register = function (regName, ClassOrCreate, settings) {
  internals.registry[regName] = ClassOrCreate;
  if (settings) {
    internals.settings[regName] = settings;
  }
};
exports.settings = function () {
  return internals.settings;
};

exports.register('BODY-UNXORBRUTEGZ', createUnxorBruteGzip, { name: 'UnXOR Brute GZip Header' });
exports.register('BODY-UNXOR', createKeyUnxorStream,
  {
    name: 'UnXOR',
    title: 'Only set keyLength or key',
    fields: [{ key: 'skip', name: 'Skip Bytes', type: 'text' },
      { key: 'keyLength', name: 'Key is in data length', type: 'text' },
      { key: 'key', name: 'Fixed key in hex', type: 'text' }
    ]
  });

exports.register('BODY-UNCOMPRESS', createUncompressStream);
exports.register('BODY-UNBASE64', createUnbase64Stream, { name: 'Unbase64' });

exports.register('ITEM-HTTP', ItemHTTPStream);
exports.register('ITEM-SMTP', ItemSMTPStream);
exports.register('ITEM-SORTER', createItemSorterStream);
exports.register('ITEM-PRINTER', through.ctor({ objectMode: true }, function (item, encoding, callback) {
  const data = item.html || item.data.toString();
  console.log(item.ts, item.client, item.itemPos, item.bodyNum, item.bodyType, item.bodyName, data.length, data);
  console.log();
  callback(null, item);
}));
exports.register('ITEM-HEX', ItemHexFormaterStream);
exports.register('ITEM-UTF8', through.ctor({ objectMode: true }, function (item, encoding, callback) {
  if (item.html === undefined) {
    item.html = '<pre>' + ArkimeUtil.safeStr(item.data.toString('utf8')) + '</pre>';
  }
  callback(null, item);
}));
exports.register('ITEM-ASCII', through.ctor({ objectMode: true }, function (item, encoding, callback) {
  if (item.html === undefined) {
    item.html = '<pre>' + ArkimeUtil.safeStr(item.data.toString('binary')) + '</pre>';
  }
  callback(null, item);
}));
exports.register('ITEM-NATURAL', through.ctor({ objectMode: true }, function (item, encoding, callback) {
  if (item.html === undefined) {
    item.html = ArkimeUtil.safeStr(item.data.toString()).replace(/\r?\n/g, '<br>');
  }
  callback(null, item);
}));
exports.register('ITEM-BYTES', through.ctor({ objectMode: true }, function (item, encoding, callback) {
  item.bytes = item.data.length;
  callback(null, item);
}));
exports.register('ITEM-HASH', through.ctor({ objectMode: true }, function (item, encoding, callback) {
  if (item.data !== undefined) {
    const md5 = cryptoLib.createHash('md5').update(item.data).digest('hex');
    const sha256 = cryptoLib.createHash('sha256').update(item.data).digest('hex');
    if (this.options['ITEM-HASH'].hash === md5 || this.options['ITEM-HASH'].hash === sha256) { return callback(null, item); }
  }
  return callback();
}));
exports.register('ITEM-RAWBODY', through.ctor({ objectMode: true }, function (item, encoding, callback) {
  if ((item.bodyNum !== undefined) && (item.bodyNum === this.options['ITEM-RAWBODY'].bodyNumber)) {
    return callback(null, item);
  }
  return callback();
}));
exports.register('ITEM-LINKBODY', through.ctor({ objectMode: true }, function (item, encoding, callback) {
  if (item.bodyType === undefined || item.bodyType === 'text') {
    return callback(null, item);
  }

  const url = `api/session/${encodeURIComponent(this.options.nodeName)}/${encodeURIComponent(this.options.id)}/body/${encodeURIComponent(item.bodyType)}/${encodeURIComponent(item.bodyNum)}/${encodeURIComponent(item.bodyName)}.pellet`;

  if (item.bodyType === 'image') {
    item.html = '<img src="' + url + '">';
  } else {
    item.html = "<a target='_blank' class='imagetag file' href=\"" + url + '">' + ArkimeUtil.safeStr(item.bodyName) + '</a>';
  }
  callback(null, item);
}));
exports.register('ITEM-CB', through.ctor({ objectMode: true }, function (item, encoding, callback) {
  if (this.items === undefined) {
    this.items = [];
  }
  this.items.push(item);
  callback();
}, function (callback) {
  this.options['ITEM-CB'].cb(null, this.items);
  callback();
}));

exports.Pcap2ItemStream = Pcap2ItemStream;
exports.createPipeline = function (options, order, stream, context) {
  const pipes = [stream];

  function link (p) {
    pipes[p].pipe(pipes[p + 1]).on('error', function (err) {
      console.trace('ERROR', order[p], err);
    });
  }

  for (let i = 0; i < order.length; i++) {
    const ClassOrCreate = internals.registry[order[i]];
    if (!ClassOrCreate) {
      console.trace("ERROR - Couldn't find", order[i], 'in decode registry');
      return;
    }

    // If through2 or a real class
    if (ClassOrCreate.super_ || Object.getOwnPropertyDescriptor(ClassOrCreate, 'prototype')?.writable === false) {
      pipes.push(new ClassOrCreate(options, context));
    } else {
      pipes.push(ClassOrCreate(options, context));
    }

    // We are really i+1 in pipes when we push above
    link(i);
  }
  return pipes;
};

// Run directly, testing code
if (require.main === module) {
  const options = {
    nodeName: 'nodeName',
    id: 'id',
    order: [],
    'ITEM-HTTP': {
      order: []
    },
    'ITEM-SMTP': {
      order: []
    }
  };

  let base = 'ITEM-NATURAL';
  let filename;
  const ending = ['ITEM-PRINTER'];
  for (let aa = 2; aa < process.argv.length; aa++) {
    if (process.argv[aa] === '--hex') {
      base = 'ITEM-HEX';
    } else if (process.argv[aa] === '--ascii') {
      base = 'ITEM-ASCII';
    } else if (process.argv[aa] === '--natural') {
      base = 'ITEM-NATURAL';
    } else if (process.argv[aa] === '--utf8') {
      base = 'ITEM-UTF8';
    } else if (process.argv[aa] === '--debug') {
      internals.debug++;
    } else if (process.argv[aa] === '--line') {
      options['ITEM-HEX'] = { showOffsets: true };
    } else if (process.argv[aa] === '--uncompress') {
      options['ITEM-HTTP'].order.push('BODY-UNCOMPRESS');
      options['ITEM-SMTP'].order.push('BODY-UNCOMPRESS');
    } else if (process.argv[aa] === '--unbase64') {
      options['ITEM-SMTP'].order.push('BODY-UNBASE64');
    } else if (process.argv[aa] === '--unxorbrutegz') {
      options['ITEM-HTTP'].order.push('BODY-UNXORBRUTEGZ');
      options['BODY-UNXORBRUTEGZ'] = {};
    } else if (process.argv[aa] === '--unxor') {
      options['ITEM-HTTP'].order.push('BODY-UNXOR');
      options['BODY-UNXOR'] = { skip: +process.argv[aa + 1], keyLength: process.argv[aa + 2] };
      aa += 2;
    } else if (process.argv[aa] === '--links') {
      base = 'ITEM-LINKBODY';
    } else if (process.argv[aa] === '--bodynum') {
      aa++;
      options['ITEM-RAWBODY'] = { bodyNumber: +process.argv[aa] };
      base = 'ITEM-RAWBODY';
    } else {
      filename = process.argv[aa];
    }
  }

  options.order.push('ITEM-HTTP');
  options.order.push('ITEM-SMTP');
  options.order.push('ITEM-BYTES');
  options.order.push('ITEM-SORTER');

  options.order = options.order.concat(base, ending);
  console.log(options);

  if (!filename) {
    console.log('ERROR, must provide a file');
  } else if (filename.match(/\.pcap$/)) {
    const Pcap = require('./pcap.js');
    const fs = require('fs');
    const pcap = new Pcap(filename);
    pcap.open({ name: filename });
    const stat = fs.statSync(filename);
    let pos = 24;
    const packets = [];

    async.whilst(
      function (cb) { return cb(null, pos < stat.size); },
      function (callback) {
        pcap.readPacket(pos, function (packet) {
          const obj = {};
          pcap.decode(packet, obj);
          packets.push(obj);
          pos += packet.length;
          callback();
        });
      },
      function (err, n) {
        Pcap.reassemble_tcp(packets, packets.length, packets[0].ip.addr1 + ':' + packets[0].tcp.sport, function (err, results) {
          exports.createPipeline(options, options.order, new Pcap2ItemStream(options, results));
        });
      }
    );

    console.log('process', filename);
  } else {
    const data = require('./' + filename);
    exports.createPipeline(options, options.order, new Pcap2ItemStream(options, data));
  }
}
