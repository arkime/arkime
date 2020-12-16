/******************************************************************************/
/*
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

const request = require('request');
const wiseSource = require('./wiseSource.js');
const util = require('util');

// ----------------------------------------------------------------------------
function WiseProxySource (api, section) {
  WiseProxySource.super_.call(this, { api: api, section: section });

  this.url = api.getConfig(section, 'url');
  this.types = api.getConfig(section, 'types');
  this.mapping = [];
  this.buffer = Buffer.alloc(10000);
  this.offset = 0;
  this.bufferInfo = [];

  if (this.url === undefined) {
    console.log(this.section, '- ERROR not loading since no url specified in config file');
    return;
  }

  const types = this.types.split(',').map(item => item.trim());

  for (let i = 0; i < types.length; i++) {
    switch (types[i]) {
    case 'domain':
      this.getDomain = getDomain;
      break;
    case 'md5':
      this.getMd5 = getMd5;
      break;
    case 'ip':
      this.getIp = getIp;
      break;
    case 'email':
      this.getEmail = getEmail;
      break;
    case 'url':
      this.getURL = getURL;
      break;
    }
  }

  this.updateInfo();
  setTimeout(this.updateInfo.bind(this), 5 * 60 * 1000);

  this.api.addSource(this.section, this);
  setInterval(this.performQuery.bind(this), 500);
}
util.inherits(WiseProxySource, wiseSource);
// ----------------------------------------------------------------------------
WiseProxySource.prototype.performQuery = function () {
  if (this.bufferInfo.length === 0) {
    return;
  }

  const options = {
      url: this.url + '/get',
      method: 'POST',
      body: this.buffer.slice(0, this.offset)
  };

  const bufferInfo = this.bufferInfo;
  this.bufferInfo = [];
  this.offset = 0;

  let i;
  request(options, (err, response, body) => {
    if (err || response.statusCode !== 200) {
      console.log(this.section, 'error', this.section, err || response);
      for (i = 0; i < bufferInfo.length; i++) {
        bufferInfo[i].cb('Error');
      }

      return;
    }

    body = Buffer.from(body, 'binary');
    let offset = 0;
    const fieldsTS = body.readUInt32BE(offset); offset += 4;
    if (fieldsTS !== this.fieldsTS) {
      this.updateInfo();
    }
    // const ver = body.readUInt32BE(offset); offset += 4;
    for (i = 0; i < bufferInfo.length; i++) {
      const num = body[offset]; offset += 1;
      const bi = bufferInfo[i];

      if (num === 0) {
        return bi.cb(null, wiseSource.emptyResult);
      }

      const args = [];
      for (let n = 0; n < num; n++) {
        const field = body[offset]; offset += 1;
        const len = body[offset]; offset += 1;
        const str = body.toString('ascii', offset, offset + len - 1); offset += len;
        args.push(this.mapping[field], str);
      }
      const result = { num: args.length / 2, buffer: wiseSource.encode.apply(null, args) };
      return bi.cb(null, result);
    }
  });
};
// ----------------------------------------------------------------------------
WiseProxySource.prototype.fetch = function (type, item, cb) {
  this.buffer[this.offset] = type; this.offset++;
  this.buffer.writeUInt16BE(item.length, this.offset); this.offset += 2;
  this.buffer.write(item, this.offset); this.offset += item.length;
  this.bufferInfo.push({ type: type, item: item, cb: cb });

  if (this.bufferInfo.length > 100) {
    this.performQuery();
  }
};
// ----------------------------------------------------------------------------
WiseProxySource.prototype.updateInfo = function () {
  let options = {
      url: this.url + '/fields',
      method: 'GET'
  };

  request(options, (err, response, body) => {
    if (err) {
      console.log(this.section, 'problem fetching /fields', this.section, err || response);
      return;
    }
    const buf = Buffer.from(body, 'binary');
    let offset = 0;
    this.fieldsTS = buf.readUInt32BE(offset); offset += 4;
    // const version = buf.readUInt32BE(offset); offset += 4;
    const length = buf[offset]; offset += 1;
    for (let i = 0; i < length; i++) {
      offset++;
      const len = buf[offset]; offset += 1;
      const str = buf.toString('ascii', offset, offset + len);

      offset += len;
      this.mapping[i] = this.api.addField(str);
    }
  });

  options = {
      url: this.url + '/views',
      method: 'GET',
      json: true
  };
  request(options, (err, response, body) => {
    if (err) {
      console.log(this.section, 'problem fetching /views', this.section, err || response);
      return;
    }
     for (const name in body) {
       this.api.addView(name, body[name]);
     }
  });

  options = {
      url: this.url + '/rightClicks',
      method: 'GET',
      json: true
  };
  request(options, (err, response, body) => {
    if (err) {
      console.log(this.section, 'problem fetching /rightClicks', this.section, err || response);
      return;
    }
     for (const name in body) {
       this.api.addView(name, body[name]);
     }
  });
};
// ----------------------------------------------------------------------------
function getIp (item, cb) {
  this.fetch(0, item, cb);
}
// ----------------------------------------------------------------------------
function getDomain (item, cb) {
  this.fetch(1, item, cb);
}
// ----------------------------------------------------------------------------
function getMd5 (item, cb) {
  this.fetch(2, item, cb);
}
// ----------------------------------------------------------------------------
function getEmail (item, cb) {
  this.fetch(3, item, cb);
}
// ----------------------------------------------------------------------------
function getURL (item, cb) {
  this.fetch(4, item, cb);
}
// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('wiseproxy', {
    singleton: false,
    name: 'wiseproxy',
    description: 'Link to the wiseproxy data',
    fields: [
      { name: 'url', required: true, help: 'The URl' },
      { name: 'types', required: true, help: 'The type of data, such as ip,domain,md5,ja3,email, or something defined in [wise-types]' }
    ]
  });

  const sections = api.getConfigSections().filter((e) => { return e.match(/^wiseproxy:/); });
  sections.forEach((section) => {
    return new WiseProxySource(api, section);
  });
};
// ----------------------------------------------------------------------------
