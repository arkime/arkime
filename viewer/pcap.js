/******************************************************************************/
/* pcap.js -- represent a pcap file
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';

const fs = require('fs');
const cryptoLib = require('crypto');
const ipaddr = require('ipaddr.js');
const zlib = require('zlib');
const async = require('async');
const ArkimeUtil = require('../common/arkimeUtil');
const { LRUCache } = require('lru-cache');

const pr2name = {
  1: 'icmp',
  2: 'igmp',
  6: 'tcp',
  17: 'udp',
  47: 'gre',
  50: 'esp',
  51: 'ah',
  58: 'icmpv6',
  89: 'ospf',
  103: 'pim',
  132: 'sctp'
};

const EMPTY_BUFFER = Buffer.alloc(0);
const pcaps = new Map();

class Pcap {
  #count = 0;
  #closingTimeout = null;
  #lastMsg;

  static #etherCBs = new Map();

  // --------------------------------------------------------------------------
  constructor (key) {
    this.key = key;
    this.blockCache = new LRUCache({ max: 11 });
    return this;
  };

  /// ///////////////////////////////////////////////////////////////////////////////
  /// / High Level
  /// ///////////////////////////////////////////////////////////////////////////////

  // --------------------------------------------------------------------------
  static get (key) {
    if (pcaps.has(key)) {
      return pcaps.get(key);
    }

    const pcap = new Pcap(key);
    pcaps.set(key, pcap);
    return pcap;
  };

  // --------------------------------------------------------------------------
  static getOrOpen (info) {
    const key = `${info.node}:${info.num}`;
    if (pcaps.has(key)) {
      return pcaps.get(key);
    }

    const pcap = new Pcap(key);
    pcap.open(info);
    pcaps.set(key, pcap);
    return pcap;
  };

  // --------------------------------------------------------------------------
  static make (key, header) {
    const pcap = new Pcap(key);
    pcap.headBuffer = header;

    const magic = pcap.headBuffer.readUInt32LE(0);
    pcap.bigEndian = (magic === 0xd4c3b2a1 || magic === 0x4d3cb2a1);
    pcap.nanosecond = (magic === 0xa1b23c4d || magic === 0x4d3cb2a1);
    if (magic === 0xa1b2c3d5) {
      this.shortHeader = pcap.headBuffer.readUInt32LE(8);
    }

    if (pcap.bigEndian) {
      pcap.linkType = pcap.headBuffer.readUInt32BE(20);
    } else {
      pcap.linkType = pcap.headBuffer.readUInt32LE(20);
    }
    return pcap;
  };

  // --------------------------------------------------------------------------
  isOpen () {
    return this.fd !== undefined;
  };

  // --------------------------------------------------------------------------
  isCorrupt () {
    return this.corrupt;
  };

  // --------------------------------------------------------------------------
  get headerLen () {
    return this.shortHeader === undefined ? 16 : 6;
  }

  // --------------------------------------------------------------------------
  open (info) {
    if (this.fd) {
      return;
    }
    this.filename = info.name;
    this.encoding = info.encoding ?? 'normal';

    if (info.dek) {
      const decipher = ArkimeUtil.createDecipherAES192NoIV(info.kek);
      this.encKey = Buffer.concat([decipher.update(Buffer.from(info.dek, 'hex')), decipher.final()]);
    }

    if (info.uncompressedBits) {
      this.uncompressedBits = info.uncompressedBits;
      this.uncompressedBitsSize = Math.pow(2, this.uncompressedBits);
      this.compression = 'gzip';
    }

    if (info.compression) {
      this.compression = info.compression;
    }

    if (info.iv) {
      const iv = Buffer.from(info.iv, 'hex');
      this.iv = Buffer.alloc(16);
      iv.copy(this.iv);
    }

    this.fd = fs.openSync(info.name, 'r');
    try {
      this.readHeader();
    } catch (err) {
      delete this.fd;
      throw err;
    }
  };

  // --------------------------------------------------------------------------
  openReadWrite (info) {
    if (info.uncompressedBits !== undefined) {
      this.corrupt = true;
      throw new Error(`Can't write gzip files`);
    }
    if (info.encoding !== undefined) {
      this.corrupt = true;
      throw new Error(`Can't write encrypted files`);
    }

    if (this.fd) {
      return;
    }
    this.filename = info.name;
    this.fd = fs.openSync(info.name, 'r+');
    try {
      this.readHeader();
    } catch (err) {
      delete this.fd;
      throw err;
    }
  };

  // --------------------------------------------------------------------------
  ref () {
    this.#count++;
    if (this.#closingTimeout) {
      clearTimeout(this.#closingTimeout);
      this.#closingTimeout = null;
    }
  };

  // --------------------------------------------------------------------------
  unref () {
    this.#count--;
    if (this.#count > 0) {
      return;
    }

    if (this.#closingTimeout) {
      return;
    }

    this.#closingTimeout = setTimeout(() => {
      this.#closingTimeout = null;
      pcaps.delete(this.key);
      if (this.fd) {
        fs.close(this.fd, () => {});
      }
      delete this.fd;
      this.blockCache.clear();
    }, 2000);
  };

  // --------------------------------------------------------------------------
  createDecipher (pos) {
    this.iv.writeUInt32BE(pos, 12);
    return cryptoLib.createDecipheriv(this.encoding, this.encKey, this.iv);
  };

  // --------------------------------------------------------------------------
  readHeader (cb) {
    if (this.headBuffer) {
      if (cb) {
        cb(this.headBuffer);
      }
      return this.headBuffer;
    }

    // pcap header is 24 but reading extra becaue of gzip/encryption
    this.headBuffer = Buffer.alloc(64);
    const len = fs.readSync(this.fd, this.headBuffer, 0, this.headBuffer.length, 0);

    // Need to read in at least 24
    if (len < 24) {
      this.corrupt = true;
      fs.close(this.fd, () => {});
      throw new Error(`Missing PCAP header, only have ${len} bytes`);
    }

    if (this.encoding === 'aes-256-ctr') {
      const decipher = this.createDecipher(0);
      this.headBuffer = Buffer.concat([decipher.update(this.headBuffer),
        decipher.final()]);
    } else if (this.encoding === 'xor-2048') {
      for (let i = 0; i < this.headBuffer.length; i++) {
        this.headBuffer[i] ^= this.encKey[i % 256];
      }
    };

    if (this.uncompressedBits) {
      try {
        if (this.compression === 'gzip') {
          this.headBuffer = zlib.gunzipSync(this.headBuffer, { finishFlush: zlib.constants.Z_SYNC_FLUSH });
        } else if (this.compression === 'zstd') {
          this.headBuffer = zlib.zstdDecompressSync(this.headBuffer, { finishFlush: zlib.constants.Z_SYNC_FLUSH });
        }
      } catch (e) {
        this.corrupt = true;
        fs.close(this.fd, () => {});
        throw new Error(`Missing PCAP header, couldn't uncompress`);
      }
    }

    // Actual pcap header is 24, that is all we need
    this.headBuffer = this.headBuffer.slice(0, 24);

    const magic = this.headBuffer.readUInt32LE(0);
    this.bigEndian = (magic === 0xd4c3b2a1 || magic === 0x4d3cb2a1);
    this.nanosecond = (magic === 0xa1b23c4d || magic === 0x4d3cb2a1);

    if (!this.bigEndian && magic !== 0xa1b2c3d4 && magic !== 0xa1b23c4d && magic !== 0xa1b2c3d5) {
      this.corrupt = true;
      fs.close(this.fd, () => {});
      throw new Error('Corrupt PCAP header');
    }

    // arkime short header
    if (magic === 0xa1b2c3d5) {
      this.shortHeader = this.headBuffer.readUInt32LE(8);
      this.headBuffer[0] = 0xd4; // Reset header to normal since all apis pretend it is
    }

    if (this.bigEndian) {
      this.linkType = this.headBuffer.readUInt32BE(20);
    } else {
      this.linkType = this.headBuffer.readUInt32LE(20);
    }

    if (cb) {
      cb(this.headBuffer);
    }
    return this.headBuffer;
  };

  // --------------------------------------------------------------------------
  readPacket (pos, cb) {
    // Hacky!! File isn't actually opened, try again soon
    if (!this.fd) {
      setTimeout(() => this.readPacket(pos, cb), 10);
      return;
    }

    return this.readPacketInternal(pos, cb);
  };

  // --------------------------------------------------------------------------
  async readAndSliceBlock (posArg) {
    let pos = posArg;
    let insideOffset = 0;
    const blockSize = this.uncompressedBits ? this.uncompressedBitsSize * 2 : 128 * 1024;

    // Get the start offset and inside offset
    if (this.uncompressedBits) {
      insideOffset = pos & (this.uncompressedBitsSize - 1);
      pos = Math.floor(pos / this.uncompressedBitsSize);
    } else {
      // For uncompressed files, align to block boundaries
      insideOffset = pos % blockSize;
      pos = pos - insideOffset;
    }

    // Calculate block start position accounting for encryption
    let posoffset = 0;
    if (this.encoding === 'aes-256-ctr') {
      posoffset = pos % 16;
      pos = pos - posoffset;
    } else if (this.encoding === 'xor-2048') {
      posoffset = pos % 256;
      pos = pos - posoffset;
    }

    const blockStart = pos;

    // Check cache first (could be a promise or resolved buffer)
    const cached = this.blockCache.get(blockStart);
    if (cached) {
      if (cached instanceof Promise) {
        const block = await cached;
        return block ? block.slice(insideOffset) : null;
      } else {
        return cached.slice(insideOffset);
      }
    }

    // Create promise for this read
    const promise = new Promise((resolve) => {
      const buffer = Buffer.alloc(blockSize);

      try {
        fs.read(this.fd, buffer, 0, buffer.length, blockStart, (err, bytesRead, readBuffer) => {
          readBuffer = readBuffer.slice(0, bytesRead);

          // Make sure we have at least some data
          if (readBuffer.length - posoffset < 1) {
            resolve(null);
            return;
          }

          // Decrypt if needed
          if (this.encoding === 'aes-256-ctr') {
            const decipher = this.createDecipher(blockStart / 16);
            readBuffer = Buffer.concat([
              decipher.update(readBuffer),
              decipher.final()
            ]).slice(posoffset);
          } else if (this.encoding === 'xor-2048') {
            for (let i = posoffset; i < readBuffer.length; i++) {
              readBuffer[i] ^= this.encKey[i % 256];
            }
            readBuffer = readBuffer.slice(posoffset);
          }

          // Uncompress if needed
          if (this.uncompressedBits) {
            try {
              if (this.compression === 'gzip') {
                readBuffer = zlib.inflateRawSync(readBuffer, { finishFlush: zlib.constants.Z_SYNC_FLUSH });
              } else if (this.compression === 'zstd') {
                readBuffer = zlib.zstdDecompressSync(readBuffer, { finishFlush: zlib.constants.Z_SYNC_FLUSH });
              }
            } catch (e) {
              console.log('PCAP uncompress issue', this.key, blockStart, buffer.length, bytesRead, e);
              resolve(null);
              return;
            }
          }

          // Replace cache with full buffer
          this.blockCache.set(blockStart, readBuffer);
          // Resolve with full buffer
          resolve(readBuffer);
        });
      } catch (e) {
        resolve(null);
      }
    });

    // Cache the promise so others can wait on it
    this.blockCache.set(blockStart, promise);

    const result = await promise;
    return result ? result.slice(insideOffset) : null;
  };

  // --------------------------------------------------------------------------
  async readPacketInternal (posArg, cb) {
    try {
      let readBuffer = await this.readAndSliceBlock(posArg);
      if (!readBuffer) {
        return cb(undefined);
      }

      // Get the packetLen
      let packetLen;
      const headerLen = (this.shortHeader === undefined) ? 16 : 6;

      // Wasn't enough for header, add next block
      if (readBuffer.length < headerLen) {
        if (this.uncompressedBits) { return cb(undefined); }
        const readBuffer2 = await this.readAndSliceBlock(posArg + readBuffer.length);
        if (!readBuffer2 || readBuffer2.length < headerLen) {
          const msg = `Not enough data ${readBuffer.length} for header ${headerLen} in ${this.filename} - See https://arkime.com/faq#zero-byte-pcap-files`;
          if (Pcap.#lastMsg !== msg) {
            Pcap.#lastMsg = msg;
            console.log(msg);
          }
          return cb(undefined);
        }
        readBuffer = Buffer.concat([readBuffer, readBuffer2]);
      }

      if (this.shortHeader === undefined) {
        packetLen = (this.bigEndian ? readBuffer.readUInt32BE(8) : readBuffer.readUInt32LE(8));
      } else {
        packetLen = (this.bigEndian ? readBuffer.readUInt16BE(0) : readBuffer.readUInt16LE(0));
      }

      if (packetLen < 0 || packetLen > 0xffff) {
        return cb(undefined);
      }

      // Wasn't enough for packet data, add next block
      if (readBuffer.length < (headerLen + packetLen)) {
        if (this.uncompressedBits) { return cb(undefined); }
        const readBuffer2 = await this.readAndSliceBlock(posArg + readBuffer.length);
        if (!readBuffer2 || readBuffer.length + readBuffer2.length < headerLen + packetLen) {
          const msg = `Not enough data ${readBuffer.length} for packet ${headerLen + packetLen} in ${this.filename} - See https://arkime.com/faq#zero-byte-pcap-files`;
          if (Pcap.#lastMsg !== msg) {
            Pcap.#lastMsg = msg;
            console.log(msg);
          }
          return cb(undefined);
        }
        readBuffer = Buffer.concat([readBuffer, readBuffer2]);
      }

      // Full packet fit
      if ((headerLen + packetLen) <= readBuffer.length) {
        if (this.shortHeader !== undefined) {
          const t = readBuffer.readUInt32LE(2);
          const sec = (t >>> 20) + this.shortHeader;
          const usec = t & 0xfffff;

          // Make a new buffer with standard pcap header and packet data
          const newBuffer = Buffer.allocUnsafe(16 + packetLen);
          newBuffer.writeUInt32LE(sec, 0);
          newBuffer.writeUInt32LE(usec, 4);
          newBuffer.writeUInt32LE(packetLen, 8);
          newBuffer.writeUInt32LE(packetLen, 12);
          readBuffer.copy(newBuffer, 16, 6, packetLen + 6);
          return cb(newBuffer);
        }
        return cb(readBuffer.slice(0, headerLen + packetLen));
      }

      return cb(null);
    } catch (err) {
      return cb(undefined);
    }
  };

  // --------------------------------------------------------------------------
  async readPacketPromise (pos) {
    return new Promise((resolve, reject) => { this.readPacket(pos, data => resolve(data)); });
  };

  // --------------------------------------------------------------------------
  scrubPacket (packet, pos, buf, entire) {
    this.blockCache.clear();
    const headerLen = (this.shortHeader === undefined) ? 16 : 6;

    let len = packet.pcap.incl_len + headerLen;
    if (entire) {
      pos += headerLen; // Don't delete pcap header
      len -= headerLen;
    } else {
      switch (packet.ip.p) {
      case 1:
        pos += (packet.icmp._pos + 8);
        len -= (packet.icmp._pos + 8);
        break;
      case 6:
        pos += (packet.tcp._pos + 4 * packet.tcp.off);
        len -= (packet.tcp._pos + 4 * packet.tcp.off);
        break;
      case 17:
        pos += (packet.udp._pos + 8);
        len -= (packet.udp._pos + 8);
        break;
      case 132:
        pos += (packet.sctp._pos + 8);
        len -= (packet.sctp._pos + 8);
        break;
      default:
        throw new Error("Unknown packet type, can't scrub");
      }
    }

    fs.writeSync(this.fd, buf, 0, len, pos);
    fs.fsyncSync(this.fd);
  };

  /// ///////////////////////////////////////////////////////////////////////////////
  /// / Utilities
  /// ///////////////////////////////////////////////////////////////////////////////

  // --------------------------------------------------------------------------
  static protocol2Name (num) {
    return pr2name[num] || ('' + num);
  };

  // --------------------------------------------------------------------------
  static inet_ntoa (num) {
    return (num >> 24 & 0xff) + '.' + (num >> 16 & 0xff) + '.' + (num >> 8 & 0xff) + '.' + (num & 0xff);
  };

  /// ///////////////////////////////////////////////////////////////////////////////
  /// / Decode pcap buffers and build up simple objects
  /// ///////////////////////////////////////////////////////////////////////////////

  // --------------------------------------------------------------------------
  icmp (buffer, obj, pos) {
    obj.icmp = {
      _pos: pos,
      length: buffer.length,
      type: buffer[0],
      code: buffer[1],
      sum: buffer.readUInt16BE(2)
      // id:        buffer.readUInt16BE(4),
      // sequence:  buffer.readUInt16BE(6)
    };

    obj.icmp.data = buffer;
  };

  // --------------------------------------------------------------------------
  tcp (buffer, obj, pos) {
    try {
      obj.tcp = {
        _pos: pos,
        length: buffer.length,
        sport: buffer.readUInt16BE(0),
        dport: buffer.readUInt16BE(2),
        seq: buffer.readUInt32BE(4),
        ack: buffer.readUInt32BE(8),
        off: ((buffer[12] >> 4) & 0xf),
        res1: (buffer[12] & 0xf),
        flags: buffer[13],
        res2: (buffer[13] >> 6 & 0x3),
        urgflag: (buffer[13] >> 5 & 0x1),
        ackflag: (buffer[13] >> 4 & 0x1),
        pshflag: (buffer[13] >> 3 & 0x1),
        rstflag: (buffer[13] >> 2 & 0x1),
        synflag: (buffer[13] >> 1 & 0x1),
        finflag: (buffer[13] >> 0 & 0x1),
        win: buffer.readUInt16BE(14),
        sum: buffer.readUInt16BE(16),
        urp: buffer.readUInt16BE(18)
      };

      if (4 * obj.tcp.off > buffer.length) {
        obj.tcp.data = EMPTY_BUFFER;
      } else {
        obj.tcp.data = buffer.slice(4 * obj.tcp.off);
      }
    } catch (e) {
      console.trace("Couldn't parse tcp", e);
    }
  };

  // --------------------------------------------------------------------------
  udp (buffer, obj, pos) {
    obj.udp = {
      _pos: pos,
      length: buffer.length,
      sport: buffer.readUInt16BE(0),
      dport: buffer.readUInt16BE(2),
      ulen: buffer.readUInt16BE(4),
      sum: buffer.readUInt16BE(6)
    };

    obj.udp.data = buffer.slice(8);
    const data = obj.udp.data;

    // vxlan
    if ((obj.udp.dport === 4789) && (data.length > 8) && ((data[0] & 0x77) === 0) && ((data[1] & 0xb7) === 0)) {
      this.ether(buffer.slice(16), obj, pos + 16);
    }

    // vxlan gpe
    if ((obj.udp.dport === 4790) && (data.length > 8) && ((data[0] & 0xf0) === 0) && ((data[1] & 0xff) === 0)) {
      switch (data[3]) {
      case 1:
        return this.ip4(buffer.slice(16), obj, pos + 16);
      case 2:
        return this.ip6(buffer.slice(16), obj, pos + 16);
      case 3:
        return this.ether(buffer.slice(16), obj, pos + 16);
      case 4:
      // TODO NSH
        break;
      }
    }

    // geneve
    if ((obj.udp.dport === 6081) && (data.length > 8) && ((data[0] & 0xc0) === 0) && ((data[1] & 0x3f) === 0)) {
      const optlen = data[0] & 0x3f;
      const protocol = (data[2] << 8) | data[3];
      const offset = 8 + optlen * 4;

      if (8 + offset < buffer.length) {
        this.ethertyperun(protocol, buffer.slice(8 + offset), obj, pos + 8 + offset);
      }
    }

    // gtp
    if ((obj.udp.dport === 2152) && (data.length > 8) && ((data[0] & 0xf0) === 0x30) && (data[1] === 0xff)) {
      let offset = 8;
      let next = 0;
      if (data[0] & 0x7) {
        offset += 3;
        next = data[offset];
        offset++;
      }
      while (next !== 0) {
        const extlen = data[offset];
        offset++;
        offset += extlen * 4 - 2;
        next = data[offset];
        offset++;
      }

      if ((data[offset] & 0xf0) === 0x60) {
        this.ip6(data.slice(offset), obj, pos + offset);
      } else {
        this.ip4(data.slice(offset), obj, pos + offset);
      }
    }
  };

  // --------------------------------------------------------------------------
  sctp (buffer, obj, pos) {
    obj.sctp = {
      _pos: pos,
      length: buffer.length,
      sport: buffer.readUInt16BE(0),
      dport: buffer.readUInt16BE(2)
    };

    obj.sctp.data = buffer.slice(12);
  };

  // --------------------------------------------------------------------------
  esp (buffer, obj, pos) {
    obj.esp = {
      _pos: pos,
      length: buffer.length
    };

    obj.esp.data = buffer;
  };

  // --------------------------------------------------------------------------
  gre (buffer, obj, pos) {
    obj.gre = {
      flags_version: buffer.readUInt16BE(0),
      type: buffer.readUInt16BE(2)
    };

    let bpos = 4;
    if (obj.gre.flags_version & (0x8000 | 0x4000)) {
      bpos += 4;
    }

    // key
    if (obj.gre.flags_version & 0x2000) {
      bpos += 4;
    }

    // sequence number
    if (obj.gre.flags_version & 0x1000) {
      bpos += 4;
    }

    // routing
    if (obj.gre.flags_version & 0x4000) {
      while (true) {
        bpos += 3;
        const len = buffer.readUInt16BE(bpos);
        bpos++;
        if (len === 0) { break; }
        bpos += len;
      }
    }

    // ack number
    if (obj.gre.flags_version & 0x0080) {
      bpos += 4;
    }

    if (obj.gre.type === 0x88be && (obj.gre.flags_version & 0x1000) === 0) {
      this.ethertyperun(0, buffer.slice(bpos), obj, pos + bpos);
    } else if (!this.ethertyperun(obj.gre.type, buffer.slice(bpos), obj, pos + bpos)) {
      console.log('gre Unknown type', obj.gre.type, 'Please open a new protocol issue with sample pcap - https://github.com/arkime/arkime/issues/new/choose');
    }
  };

  // --------------------------------------------------------------------------
  erspan (buffer, obj, pos) {
    this.ether(buffer.slice(8), obj, pos + 8);
  }

  // --------------------------------------------------------------------------
  erspan3 (buffer, obj, pos) {
    obj.erspan3 = {
      subheader: buffer.readUInt16BE(10)
    };

    let bpos = 12;
    if (obj.erspan3.subheader & 0x0001) {
      bpos += 8;
    }

    this.ethertyperun(0, buffer.slice(bpos), obj, pos + bpos);
  }

  // --------------------------------------------------------------------------
  ip4 (buffer, obj, pos) {
    obj.ip = {
      length: buffer.length,
      hl: (buffer[0] & 0xf),
      v: ((buffer[0] >> 4) & 0xf),
      tos: buffer[1],
      len: buffer.readUInt16BE(2),
      id: buffer.readUInt16BE(4),
      off: buffer.readUInt16BE(6),
      ttl: buffer[8],
      p: buffer[9],
      sum: buffer.readUInt16BE(10),
      addr1: Pcap.inet_ntoa(buffer.readUInt32BE(12)),
      addr2: Pcap.inet_ntoa(buffer.readUInt32BE(16))
    };

    switch (obj.ip.p) {
    case 1:
      this.icmp(buffer.slice(obj.ip.hl * 4, obj.ip.len), obj, pos + obj.ip.hl * 4);
      break;
    case 6:
      this.tcp(buffer.slice(obj.ip.hl * 4, obj.ip.len), obj, pos + obj.ip.hl * 4);
      break;
    case 17:
      this.udp(buffer.slice(obj.ip.hl * 4, obj.ip.len), obj, pos + obj.ip.hl * 4);
      break;
    case 41: // IPPROTO_IPV6
      this.ip6(buffer.slice(obj.ip.hl * 4, obj.ip.len), obj, pos + obj.ip.hl * 4);
      break;
    case 50: // IPPROTO_ESP
      this.esp(buffer.slice(obj.ip.hl * 4, obj.ip.len), obj, pos + obj.ip.hl * 4);
      break;
    case 47:
      this.gre(buffer.slice(obj.ip.hl * 4, obj.ip.len), obj, pos + obj.ip.hl * 4);
      break;
    case 132:
      this.sctp(buffer.slice(obj.ip.hl * 4, obj.ip.len), obj, pos + obj.ip.hl * 4);
      break;
    default:
      obj.ip.data = buffer.slice(obj.ip.hl * 4, obj.ip.len);
  // console.log("v4 Unknown ip.p", obj, 'Please open a new protocol issue with sample pcap - https://github.com/arkime/arkime/issues/new/choose');
    }
  };

  // --------------------------------------------------------------------------
  ip6 (buffer, obj, pos) {
    obj.ip = {
      length: buffer.length,
      v: ((buffer[0] >> 4) & 0xf),
      tc: ((buffer[0] & 0xf) << 4) | ((buffer[1] >> 4) & 0xf),
      flow: ((buffer[1] & 0xf) << 16) | (buffer[2] << 8) | buffer[3],
      len: buffer.readUInt16BE(4),
      p: buffer[6],
      hopLimt: buffer[7],
      addr1: ipaddr.fromByteArray(buffer.slice(8, 24)).toString(),
      addr2: ipaddr.fromByteArray(buffer.slice(24, 40)).toString()
    };

    let offset = 40;
    while (offset < buffer.length) {
      switch (obj.ip.p) {
      case 0: // IPPROTO_HOPOPTS:
      case 60: // IPPROTO_DSTOPTS:
      case 43: // IPPROTO_ROUTING:
        obj.ip.p = buffer[offset];
        offset += ((buffer[offset + 1] + 1) << 3);
        break;
      case 1:
      case 58:
        this.icmp(buffer.slice(offset, offset + obj.ip.len), obj, pos + offset);
        return;
      case 4: // IPPROTO_IPV4
        this.ip4(buffer.slice(offset, offset + obj.ip.len), obj, pos + offset);
        return;
      case 6:
        this.tcp(buffer.slice(offset, offset + obj.ip.len), obj, pos + offset);
        return;
      case 17:
        this.udp(buffer.slice(offset, offset + obj.ip.len), obj, pos + offset);
        return;
      case 47:
        this.gre(buffer.slice(offset, offset + obj.ip.len), obj, pos + offset);
        return;
      case 50: // IPPROTO_ESP
        this.esp(buffer.slice(offset, offset + obj.ip.len), obj, pos + offset);
        return;
      case 132:
        this.sctp(buffer.slice(offset, offset + obj.ip.len), obj, pos + offset);
        return;
      default:
        obj.ip.data = buffer.slice(offset, offset + obj.ip.len);
        // console.log("v6 Unknown ip.p", obj, 'Please open a new protocol issue with sample pcap - https://github.com/arkime/arkime/issues/new/choose');
        return;
      }
    }
  };

  // --------------------------------------------------------------------------
  pppoe (buffer, obj, pos) {
    obj.pppoe = {
      len: buffer.readUInt16BE(4) - 2,
      type: buffer.readUInt16BE(6)
    };

    switch (obj.pppoe.type) {
    case 0x21:
      this.ip4(buffer.slice(8, 8 + obj.pppoe.len), obj, pos + 8);
      return;
    case 0x57:
      this.ip6(buffer.slice(8, 8 + obj.pppoe.len), obj, pos + 8);
      return;
    default:
      console.log('Unknown pppoe.type', obj, 'Please open a new protocol issue with sample pcap - https://github.com/arkime/arkime/issues/new/choose');
    }
  };

  // --------------------------------------------------------------------------
  ppp (buffer, obj, pos) {
    obj.pppoe = {
      type: buffer.readUInt16BE(2)
    };

    switch (obj.pppoe.type) {
    case 0x21:
      this.ip4(buffer.slice(4), obj, pos + 4);
      return;
    case 0x57:
      this.ip6(buffer.slice(4), obj, pos + 4);
      return;
    default:
      console.log('Unknown ppp.type', obj, 'Please open a new protocol issue with sample pcap - https://github.com/arkime/arkime/issues/new/choose');
    }
  };

  // --------------------------------------------------------------------------
  mpls (buffer, obj, pos) {
    let offset = 0;
    while (offset + 5 < buffer.length) {
      const S = buffer[offset + 2] & 0x1;
      offset += 4;
      if (S) {
        switch (buffer[offset] >> 4) {
        case 4:
          this.ip4(buffer.slice(offset), obj, pos + offset);
          return;
        case 6:
          this.ip6(buffer.slice(offset), obj, pos + offset);
          return;
        default:
          console.log('Unknown mpls.type', buffer[offset] >> 4, obj, offset);
          return;
        }
      }
    }
  };

  // --------------------------------------------------------------------------
  static setEtherCB (type, cb) {
    Pcap.#etherCBs.set(type, cb);
  };

  // --------------------------------------------------------------------------
  ethertyperun (type, buffer, obj, pos) {
    if (Pcap.#etherCBs.has(type)) {
      return Pcap.#etherCBs.get(type)(this, buffer, obj, pos);
    }

    switch (type) {
    case 0x88be: // ERSPAN I && II
      this.erspan(buffer, obj, pos);
      break;
    case 0x22eb: // ERSPAN3 III
      this.erspan3(buffer, obj, pos);
      break;
    case 0x0800:
      this.ip4(buffer, obj, pos);
      break;
    case 0x0806: // arp
      obj.ether ??= { data: buffer };
      break;
    case 0x86dd:
      this.ip6(buffer, obj, pos);
      break;
    case 0x8864:
      this.pppoe(buffer, obj, pos);
      break;
    case 0x8847:
      this.mpls(buffer, obj, pos);
      break;
    case 0:
    case 0x6558:
      this.ether(buffer, obj, pos);
      break;
    case 0x6559:
      this.framerelay(buffer, obj, pos);
      break;
    case 0x880b:
      this.ppp(buffer, obj, pos);
      break;
    default:
      return false;
    }
    return true;
  };

  // --------------------------------------------------------------------------
  ethertype (buffer, obj, pos) {
    obj.ether.type = buffer.readUInt16BE(0);

    if (this.ethertyperun(obj.ether.type, buffer.slice(2), obj, pos + 2)) { return; }

    switch (obj.ether.type) {
    case 0x8100: // VLAN
    case 0x88a8: // Q-in-Q
      this.ethertype(buffer.slice(4), obj, pos + 4);
      break;
    default:
      obj.ether.data = buffer.slice(2);
      // console.trace("Unknown ether.type", obj);
      break;
    }
  };

  // --------------------------------------------------------------------------
  ether (buffer, obj, pos) {
    obj.ether = {
      length: buffer.length,
      addr1: buffer.slice(0, 6).toString('hex', 0, 6),
      addr2: buffer.slice(6, 12).toString('hex', 0, 6)
    };
    this.ethertype(buffer.slice(12), obj, pos + 12);
  };

  // --------------------------------------------------------------------------
  radiotap (buffer, obj, pos) {
    const l = buffer[2] + 24 + 6;
    const ethertype = buffer.readUInt16BE(l);

    if (this.ethertyperun(ethertype, buffer.slice(l + 2), obj, pos + l + 2)) { return; }
  };

  // --------------------------------------------------------------------------
  nflog (buffer, obj, pos) {
    let offset = 4;
    while (offset + 4 < buffer.length) {
      const len = buffer.readUInt16LE(offset);
      if (buffer[offset + 3] === 0 && buffer[offset + 2] === 9) {
        if (buffer[0] === 2) {
          return this.ip4(buffer.slice(offset + 4), obj, pos + offset + 4);
        } else {
          return this.ip6(buffer.slice(offset + 4), obj, pos + offset + 4);
        }
      } else {
        offset += (len + 3) & 0xfffc;
      }
    }

    const l = buffer[2] + 24;
    if (buffer[l + 6] === 0x08 && buffer[l + 7] === 0x00) {
      this.ip4(buffer.slice(l + 8), obj, pos + l + 8);
    } else if (buffer[l + 6] === 0x86 && buffer[l + 7] === 0xdd) {
      this.ip6(buffer.slice(l + 8), obj, pos + l + 8);
    }
  };

  // --------------------------------------------------------------------------
  framerelay (buffer, obj, pos) {
    if (buffer[2] === 0x03 || buffer[3] === 0xcc) {
      this.ip4(buffer.slice(4), obj, pos + 4);
    } else if (buffer[2] === 0x08 || buffer[3] === 0x00) {
      this.ip4(buffer.slice(4), obj, pos + 4);
    } else if (buffer[2] === 0x86 || buffer[3] === 0xdd) {
      this.ip6(buffer.slice(4), obj, pos + 4);
    }
  };

  // --------------------------------------------------------------------------
  pcap (buffer, obj) {
    if (this.bigEndian) {
      obj.pcap = {
        ts_sec: buffer.readUInt32BE(0),
        ts_usec: buffer.readUInt32BE(4),
        incl_len: buffer.readUInt32BE(8),
        orig_len: buffer.readUInt32BE(12)
      };
    } else {
      obj.pcap = {
        ts_sec: buffer.readUInt32LE(0),
        ts_usec: buffer.readUInt32LE(4),
        incl_len: buffer.readUInt32LE(8),
        orig_len: buffer.readUInt32LE(12)
      };
    }

    if (this.nanosecond) {
      obj.pcap.ts_usec = Math.floor(obj.pcap.ts_usec / 1000);
    }

    switch (this.linkType) {
    case 0: // NULL
      if (buffer[16] === 30) {
        this.ip6(buffer.slice(20, obj.pcap.incl_len + 16), obj, 20);
      } else {
        this.ip4(buffer.slice(20, obj.pcap.incl_len + 16), obj, 20);
      }
      break;
    case 1: // Ether
      this.ether(buffer.slice(16, obj.pcap.incl_len + 16), obj, 16);
      break;
    case 12: // LOOP
    case 101: // RAW
      if ((buffer[16] & 0xF0) === 0x60) {
        this.ip6(buffer.slice(16, obj.pcap.incl_len + 16), obj, 16);
      } else {
        this.ip4(buffer.slice(16, obj.pcap.incl_len + 16), obj, 16);
      }
      break;
    case 107: // Frame Relay
      this.framerelay(buffer.slice(16, obj.pcap.incl_len + 16), obj, 16);
      break;
    case 113: // SLL
      obj.ether = {};
      this.ethertype(buffer.slice(30, obj.pcap.incl_len + 16), obj, 30);
      break;
    case 127: // radiotap
      this.radiotap(buffer.slice(16, obj.pcap.incl_len + 16), obj, 16);
      break;
    case 228: // RAW
      this.ip4(buffer.slice(16, obj.pcap.incl_len + 16), obj, 16);
      break;
    case 239: // NFLOG
      this.nflog(buffer.slice(16, obj.pcap.incl_len + 16), obj, 16);
      break;
    case 276: // SLL2
      this.ip4(buffer.slice(36, obj.pcap.incl_len + 20), obj, 36);
      break;
    default:
      console.log('Unsupported pcap file', this.filename, 'link type', this.linkType, 'Please open a new protocol issue with sample pcap - https://github.com/arkime/arkime/issues/new/choose');
      break;
    }
  };

  // --------------------------------------------------------------------------
  decode (buffer, obj) {
    this.readHeader();
    this.pcap(buffer, obj);
  };

  // --------------------------------------------------------------------------
  getHeaderNg () {
    const buffer = this.readHeader();
    const b = Buffer.alloc(32 + 24);

    b.writeUInt32LE(0x0A0D0D0A, 0); // Block Type
    b.writeUInt32LE(32, 4); // Block Len 1
    b.writeUInt32LE(0x1A2B3C4D, 8); // Byte Order Magic
    b.writeUInt16LE(1, 12); // Major
    b.writeUInt16LE(0, 14); // Minor
    b.writeUInt32LE(0xffffffff, 16); // Unknown Section Length 1
    b.writeUInt32LE(0xffffffff, 20); // Unknown Section Length 2
    b.writeUInt32LE(0, 24); // Options
    b.writeUInt32LE(32, 28); // Block Len 2

    b.writeUInt32LE(0x00000001, 32); // Block Type
    b.writeUInt32LE(24, 36); // Block Len 1
    b.writeUInt16LE(buffer.readUInt32LE(20), 40); // Link Type
    b.writeUInt16LE(0, 42); // Reserved
    b.writeUInt32LE(buffer.readUInt32LE(16), 44); // SnapLen
    b.writeUInt32LE(0, 48); // Options
    b.writeUInt32LE(24, 52); // Block Len 2

    return b;
  };

  /// ///////////////////////////////////////////////////////////////////////////////
  /// / Reassembly array of packets
  /// ///////////////////////////////////////////////////////////////////////////////

  // --------------------------------------------------------------------------
  static reassemble_icmp (packets, numPackets, cb) {
    const results = [];
    packets.length = Math.min(packets.length, numPackets);
    for (const packet of packets) {
      const key = packet.ip.addr1;
      if (results.length === 0 || key !== results[results.length - 1].key) {
        results.push({
          key,
          buffers: [packet.icmp.data],
          ts: packet.pcap.ts_sec * 1000 + Math.round(packet.pcap.ts_usec / 1000)
        });
      } else {
        results[results.length - 1].buffers.push(packet.icmp.data);
      }
    }
    for (const result of results) {
      result.data = Buffer.concat(result.buffers);
      delete result.buffers;
    }
    cb(null, results);
  };

  // --------------------------------------------------------------------------
  static reassemble_udp (packets, numPackets, cb) {
    const results = [];
    try {
      packets.length = Math.min(packets.length, numPackets);
      for (const packet of packets) {
        const key = packet.ip.addr1 + ':' + packet.udp.sport;
        if (results.length === 0 || key !== results[results.length - 1].key) {
          results.push({
            key,
            buffers: [packet.udp.data],
            ts: packet.pcap.ts_sec * 1000 + Math.round(packet.pcap.ts_usec / 1000)
          });
        } else {
          results[results.length - 1].buffers.push(packet.udp.data);
        }
      }
      for (const result of results) {
        result.data = Buffer.concat(result.buffers);
        delete result.buffers;
      }
      cb(null, results);
    } catch (e) {
      cb(e, results);
    }
  };

  // --------------------------------------------------------------------------
  static reassemble_sctp (packets, numPackets, cb) {
    const results = [];
    try {
      packets.length = Math.min(packets.length, numPackets);
      for (const packet of packets) {
        const key = packet.ip.addr1 + ':' + packet.sctp.sport;
        if (results.length === 0 || key !== results[results.length - 1].key) {
          results.push({
            key,
            buffers: [packet.sctp.data],
            ts: packet.pcap.ts_sec * 1000 + Math.round(packet.pcap.ts_usec / 1000)
          });
        } else {
          results[results.length - 1].buffers.push(packet.sctp.data);
        }
      }
      for (const result of results) {
        result.data = Buffer.concat(result.buffers);
        delete result.buffers;
      }
      cb(null, results);
    } catch (e) {
      cb(e, results);
    }
  };

  // --------------------------------------------------------------------------
  static reassemble_esp (packets, numPackets, cb) {
    const results = [];
    packets.length = Math.min(packets.length, numPackets);
    for (const packet of packets) {
      const key = packet.ip.addr1;
      if (results.length === 0 || key !== results[results.length - 1].key) {
        results.push({
          key,
          buffers: [packet.esp.data],
          ts: packet.pcap.ts_sec * 1000 + Math.round(packet.pcap.ts_usec / 1000)
        });
      } else {
        results[results.length - 1].buffers.push(packet.esp.data);
      }
    }
    for (const result of results) {
      result.data = Buffer.concat(result.buffers);
      delete result.buffers;
    }
    cb(null, results);
  };

  // --------------------------------------------------------------------------
  static reassemble_generic_ip (packets, numPackets, cb) {
    const results = [];
    packets.length = Math.min(packets.length, numPackets);
    for (const packet of packets) {
      const key = packet.ip.addr1;
      if (results.length === 0 || key !== results[results.length - 1].key) {
        results.push({
          key,
          buffers: [packet.ip.data],
          ts: packet.pcap.ts_sec * 1000 + Math.round(packet.pcap.ts_usec / 1000)
        });
      } else {
        results[results.length - 1].buffers.push(packet.ip.data);
      }
    }
    for (const result of results) {
      result.data = Buffer.concat(result.buffers);
      delete result.buffers;
    }
    cb(null, results);
  };

  // --------------------------------------------------------------------------
  static reassemble_generic_ether (packets, numPackets, cb) {
    const results = [];
    packets.length = Math.min(packets.length, numPackets);
    for (const packet of packets) {
      const key = packet.ether.addr1;
      if (results.length === 0 || key !== results[results.length - 1].key) {
        results.push({
          key,
          buffers: [packet.ether.data],
          ts: packet.pcap.ts_sec * 1000 + Math.round(packet.pcap.ts_usec / 1000)
        });
      } else {
        results[results.length - 1].buffers.push(packet.ether.data);
      }
    }
    for (const result of results) {
      result.data = Buffer.concat(result.buffers);
      delete result.buffers;
    }
    cb(null, results);
  };

  // Needs to be rewritten since its possible for packets to be
  // dropped by windowing and other things to actually be displayed allowed.
  // If multiple tcp sessions in one arkime session display can be wacky/wrong.

  // --------------------------------------------------------------------------
  static reassemble_tcp (packets, numPackets, skey, cb) {
    try {
    // Remove syn, rst, 0 length packets and figure out min/max seq number
      const packets2 = [];
      const info = {};
      const keys = [];
      for (const packet of packets) {
        if (packet.tcp.data.length === 0 || packet.tcp.rstflag || packet.tcp.synflag) {
          continue;
        }
        const key = packet.ip.addr1 + ':' + packet.tcp.sport;
        if (!info[key]) {
          info[key] = { min: packet.tcp.seq, max: packet.tcp.seq, wrapseq: false, wrapack: false };
          keys.push(key);
        } else if (info[key].min > packet.tcp.seq) {
          info[key].min = packet.tcp.seq;
        } else if (info[key].max < packet.tcp.seq) {
          info[key].max = packet.tcp.seq;
        }

        packets2.push(packet);
      }

      if (keys.length === 1) {
        const key = packets[0].ip.addr2 + ':' + packets[0].tcp.dport;
        info[key] = { min: packets[0].tcp.ack, max: packets[0].tcp.ack, wrapseq: false, wrapack: false };
        keys.push(key);
      }

      packets = packets2;

      if (packets.length === 0) {
        return cb(null, packets);
      }

      // Do we need to wrap the packets
      let needwrap = false;
      if (info[keys[0]] && info[keys[0]].max - info[keys[0]].min > 0x7fffffff) {
        info[keys[0]].wrapseq = true;
        info[keys[1]].wrapack = true;
        needwrap = true;
      }

      if (info[keys[1]] && info[keys[1]].max - info[keys[1]].min > 0x7fffffff) {
        info[keys[1]].wrapseq = true;
        info[keys[0]].wrapack = true;
        needwrap = true;
      }

      // Wrap the packets
      if (needwrap) {
        for (const packet of packets) {
          const key = packet.ip.addr1 + ':' + packet.tcp.sport;
          if (info[key].wrapseq && packet.tcp.seq < 0x7fffffff) {
            packet.tcp.seq += 0xffffffff;
          }

          if (info[key].wrapack && packet.tcp.ack < 0x7fffffff) {
            packet.tcp.ack += 0xffffffff;
          }
        }
      }

      // Sort Packets
      const clientKey = packets[0].ip.addr1 + ':' + packets[0].tcp.sport;
      packets.sort((a, b) => {
        if ((a.ip.addr1 === b.ip.addr1) && (a.tcp.sport === b.tcp.sport)) {
          return (a.tcp.seq - b.tcp.seq);
        }

        if (clientKey === a.ip.addr1 + ':' + a.tcp.sport) {
          return ((a.tcp.seq + a.tcp.data.length - 1) - b.tcp.ack);
        }

        return (a.tcp.ack - (b.tcp.seq + b.tcp.data.length - 1));
      });

      // Truncate
      packets.length = Math.min(packets.length, numPackets);

      // Now divide up conversation
      let clientSeq = 0;
      let serverSeq = 0;
      let start = 0;
      let previous = 0;

      // We use async here so that the main event loop isnt blocked for large reassemblies
      // We could have just done for (packet of packets) but that would block the event loop
      const results = [];
      async.forEachSeries(packets, (packet, nextCb) => {
        const pkey = packet.ip.addr1 + ':' + packet.tcp.sport;
        if (pkey === clientKey) {
          if (clientSeq >= (packet.tcp.seq + packet.tcp.data.length)) {
            return nextCb();
          }
          clientSeq = (packet.tcp.seq + packet.tcp.data.length);
        } else {
          if (serverSeq >= (packet.tcp.seq + packet.tcp.data.length)) {
            return nextCb();
          }
          serverSeq = (packet.tcp.seq + packet.tcp.data.length);
        }

        if (results.length === 0 || pkey !== results[results.length - 1].key) {
          previous = start = packet.tcp.seq;
          results.push({
            key: pkey,
            buffers: [packet.tcp.data],
            length: packet.tcp.data.length,
            ts: packet.pcap.ts_sec * 1000 + Math.round(packet.pcap.ts_usec / 1000)
          });
        } else if (packet.tcp.seq - previous > 0xffff) {
          results.push({ key: '', buffers: [], length: 0, ts: packet.pcap.ts_sec * 1000 + Math.round(packet.pcap.ts_usec / 1000) });
          // Larger then max window size packets missing
          previous = start = packet.tcp.seq;
          results.push({
            key: pkey,
            buffers: [packet.tcp.data],
            length: packet.tcp.data.length,
            ts: packet.pcap.ts_sec * 1000 + Math.round(packet.pcap.ts_usec / 1000)
          });
        } else {
          previous = packet.tcp.seq;
          const lastResult = results[results.length - 1];
          const gapSize = packet.tcp.seq - start - lastResult.length;
          if (gapSize > 0) {
            // Missing data, zero fill
            lastResult.buffers.push(Buffer.alloc(gapSize));
            lastResult.length += gapSize;
          } else if (gapSize < 0) {
            // Retransmitted data, trim off front
            if (-gapSize >= packet.tcp.data.length) {
              packet.tcp.data = packet.tcp.data.slice(-gapSize);
            } else {
              packet.tcp.data = EMPTY_BUFFER;
            }
          }
          lastResult.buffers.push(packet.tcp.data);
          lastResult.length += packet.tcp.data.length;
        }
        setImmediate(nextCb);
      }, (err) => {
        for (const result of results) {
          result.data = Buffer.concat(result.buffers);
          delete result.buffers;
        }
        if (skey !== results[0].key) {
          results.unshift({ data: EMPTY_BUFFER, key: skey });
        }
        cb(null, results);
      });
    } catch (e) {
      cb(e, null);
    }
  };

  // --------------------------------------------------------------------------
  static packetFlow (session, packets, numPackets, cb) {
    packets = packets.slice(0, numPackets);

    let sKey = Pcap.keyFromSession(session);
    if (!packets[0].ip || packets[0].ip.p !== 6) {
      sKey = Pcap.key(packets[0]);
    }
    let dKey;

    const results = packets.map((packet) => {
      const result = {
        key: Pcap.key(packet),
        ts: packet.pcap.ts_sec * 1000 + Math.round(packet.pcap.ts_usec / 1000)
      };

      const match = result.key === sKey;
      if (!dKey && !match) { dKey = result.key; }
      result.src = match;

      if (!packet.ip) {
        result.data = packet.ether.data;
      } else {
        switch (packet.ip.p) {
        case 1:
        case 58:
          result.data = packet.icmp.data;
          break;
        case 6:
          result.data = packet.tcp.data;
          result.tcpflags = {
            syn: packet.tcp.synflag,
            ack: packet.tcp.ackflag,
            psh: packet.tcp.pshflag,
            rst: packet.tcp.rstflag,
            fin: packet.tcp.finflag,
            urg: packet.tcp.urgflag
          };
          break;
        case 17:
          result.data = packet.udp.data;
          break;
        case 132:
          result.data = packet.sctp.data;
          break;
        case 50:
          result.data = packet.esp.data;
          break;
        default:
          result.data = packet.ip.data;
          break;
        }
      }

      return result;
    });

    return cb(null, results, sKey, dKey);
  };

  // --------------------------------------------------------------------------
  static key (packet) {
    if (!packet.ip) { return packet.ether.addr1; }
    const sep = packet.ip.addr1.includes(':') ? '.' : ':';
    const addr1 = ipaddr.parse(packet.ip.addr1).toString();
    switch (packet.ip.p) {
    case 6: // tcp
      return `${addr1}${sep}${packet.tcp.sport}`;
    case 17: // udp
      return `${addr1}${sep}${packet.udp.sport}`;
    case 132: // sctp
      return `${addr1}${sep}${packet.sctp.sport}`;
    default:
      return addr1;
    }
  };

  // --------------------------------------------------------------------------
  static keyFromSession (session) {
    switch (session.ipProtocol) {
    case 6: // tcp
    case 'tcp':
    case 17: // udp
    case 'udp':
    case 132: // sctp
    case 'sctp':
      const sep = session.source.ip.includes(':') ? '.' : ':';
      return `${ipaddr.parse(session.source.ip).toString()}${sep}${session.source.port}`;
    default:
      return session.source.ip;
    }
  };
};

module.exports = Pcap;
