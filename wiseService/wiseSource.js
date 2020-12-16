/******************************************************************************/
/* Base class for data sources
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

const csv = require('csv');
const request = require('request');
const fs = require('fs');
const iptrie = require('iptrie');

/**
 * All sources need to have the WISESource as their top base class.
 */

class WISESource {
  /**
   * Should only be created by super(api, section, options) call
   * @param {WISESourceAPI} api - the api when source created
   * @param {string} section - the section name
   * @param {object} options - All the options
   * @param {boolean} [options.dontCache=false] - do not cache this source, the source handles itself
   * @param {integer} [options.cacheTimeout=cacheAgeMin*60 or 60] - override the cacheAgeMin setting, -1 same as dont
   * @param {boolean} [options.tagsSetting=false] - load the optional tags setting
   * @param {boolean} [options.typeSetting=false] - load the required type setting
   * @param {boolean} [options.formatSetting=false] - load the format setting
   */
  constructor (api, section, options) {
    this.api = api;
    this.section = section;
    this.view = '';
    this.shortcuts = {};
    if (options.dontCache) {
      this.cacheTimeout = -1;
    } else if (options.cacheTimeout !== undefined) {
      this.cacheTimeout = options.cacheTimeout;
    } else {
      this.cacheTimeout = 60 * +this.api.getConfig(section, 'cacheAgeMin', '60'); // Default an hour
    }
    this.cacheHitStat = 0;
    this.cacheMissStat = 0;
    this.cacheRefreshStat = 0;
    this.cacheDroppedStat = 0;
    this.average100MS = 0;
    this.srcInProgress = {};

    if (options.typeSetting) {
      this.typeSetting();
    }

    if (options.tagsSetting) {
      this.tagsSetting();
    }

    if (options.formatSetting) {
      this.formatSetting();
    }

    // Domain and Email wildcards to exclude from source
    ['excludeDomains', 'excludeEmails', 'excludeURLs'].forEach((type) => {
      const items = api.getConfig(section, type);
      this[type] = [];
      if (!items) { return; }
      items.split(';').map(item => item.trim()).forEach((item) => {
        if (item === '') {
          return;
        }
        this[type].push(RegExp.fromWildExp(item, 'ailop'));
      });
    });

    // IP CIDRs to exclude from source
    this.excludeIPs = new iptrie.IPTrie();
    let items = api.getConfig(section, 'excludeIPs', '');
    items.split(';').map(item => item.trim()).forEach((item) => {
      if (item === '') {
        return;
      }
      const parts = item.split('/');
      try {
        this.excludeIPs.add(parts[0], +parts[1] || (parts[0].includes(':') ? 128 : 32), true);
      } catch (e) {
        console.log(`${section} - ERROR - excludeIPs for '${item}'`, e);
        process.exit();
      }
    });

    items = api.getConfig(section, 'onlyIPs', undefined);
    if (items) {
      this.onlyIPs = new iptrie.IPTrie();
      items.split(';').map(item => item.trim()).forEach((item) => {
        if (item === '') {
          return;
        }
        let parts = item.split('/');
        try {
          this.onlyIPs.add(parts[0], +parts[1] || (parts[0].includes(':') ? 128 : 32), true);
        } catch (e) {
          console.log(`${section} - ERROR - onlyIPs for '${item}'`, e);
          process.exit();
        }
      });
    }

    // fields defined for source
    let fields = api.getConfig(section, 'fields');
    if (fields !== undefined) {
      fields = fields.split('\\n');
      for (let i = 0; i < fields.length; i++) {
        this.parseFieldDef(fields[i]);
      }
    }

    // views defined for source
    const view = api.getConfig(section, 'view');
    if (view !== undefined) {
      this.view = view.replace(/\\n/g, '\n');
    }

    if (this.view !== '') {
      api.addView(section, this.view);
    }
  }

  // ----------------------------------------------------------------------------
  /**
   * Parse a field definition line and call the addField or addView as needed
   * @param {string} line - the line to parse
   */
  parseFieldDef (line) {
    if (line[0] === '#') {
      line = line.substring(1);
    }

    if (line.lastIndexOf('field:', 0) === 0) {
      const pos = this.api.addField(line);
      const match = line.match(/shortcut:([^;]+)/);
      if (match) {
        this.shortcuts[match[1]] = pos;
      }
    } else if (line.lastIndexOf('view:', 0) === 0) {
        this.view += line.substring(5) + '\n';
    }
  };

  // ----------------------------------------------------------------------------
  /**
   * Util function to parse CSV formatted data
   * @param {string} body - the raw CSV data
   * @param {function} setCb - the function to call for each row found
   * @param {function} endCB - all done parsing
   */
  parseCSV (body, setCb, endCb) {
    csv.parse(body, { skip_empty_lines: true, comment: '#', relax_column_count: true }, (err, data) => {
      if (err) {
        return endCb(err);
      }

      for (let i = 0; i < data.length; i++) {
        const args = [];
        for (const k in this.shortcuts) {
          if (data[i][k] !== undefined) {
            args.push(this.shortcuts[k]);
            args.push(data[i][k]);
          }
        }

        if (args.length === 0) {
          setCb(data[i][this.column], WISESource.emptyResult);
        } else {
          setCb(data[i][this.column], { num: args.length / 2, buffer: WISESource.encode.apply(null, args) });
        }
      }
      endCb(err);
    });
  };

  // ----------------------------------------------------------------------------
  /**
   * Util function to parse tagger formatted data
   * @param {string} body - the raw CSV data
   * @param {function} setCb - the function to call for each row found
   * @param {function} endCB - all done parsing
   */
  parseTagger (body, setCb, endCb) {
    const lines = body.toString().split(/\r?\n/);
    this.view = '';
    for (let l = 0, llen = lines.length; l < llen; l++) {
      if (lines[l][0] === '#') {
        this.parseFieldDef(lines[l]);
        continue;
      }

      if (lines[l].match(/^\s*$/)) {
        continue;
      }

      const args = [];
      const parts = lines[l].split(';');
      for (let p = 1; p < parts.length; p++) {
        const kv = splitRemain(parts[p], '=', 1);
        if (kv.length !== 2) {
          console.log('WARNING -', this.section, "- ignored extra piece '" + parts[p] + "' from line '" + lines[l] + "'");
          continue;
        }
        if (this.shortcuts[kv[0]] !== undefined) {
          args.push(this.shortcuts[kv[0]]);
        } else if (WISESource.field2Pos[kv[0]]) {
          args.push(WISESource.field2Pos[kv[0]]);
        } else {
          args.push(this.api.addField('field:' + kv[0]));
        }
        args.push(kv[1]);
      }
      setCb(parts[0], { num: args.length / 2, buffer: WISESource.encode.apply(null, args) });
    }
    if (this.view !== '') {
      this.api.addView(this.section, this.view);
    }
    endCb(null);
  }

  // ----------------------------------------------------------------------------
  /**
   * Util function to parse JSON formatted data
   * @param {string} body - the raw CSV data
   * @param {function} setCb - the function to call for each row found
   * @param {function} endCB - all done parsing
   */
  parseJSON (body, setCb, endCb) {
    const json = JSON.parse(body);

    if (this.keyColumn === undefined) {
      return endCb('No keyColumn set');
    }

    let keyColumn = this.keyColumn.split('.');

    // Convert shortcuts into array of key path
    let shortcuts = [];
    let shortcutsValue = [];
    for (const k in this.shortcuts) {
      shortcuts.push(k.split('.'));
      shortcutsValue.push(this.shortcuts[k]);
    }

    for (let i = 0; i < json.length; i++) {
      // Walk the key path
      let key = json[i];
      for (let j = 0; key && j < keyColumn.length; j++) {
        key = key[keyColumn[j]];
      }

      if (key === undefined || key === null) {
        continue;
      }

      const args = [];
      // Check each shortcut
      for (let k = 0; k < shortcuts.length; k++) {
        let obj = json[i];
        // Walk the shortcut path
        for (let j = 0; obj && j < shortcuts[k].length; j++) {
          obj = obj[shortcuts[k][j]];
        }
        if (obj !== undefined && obj !== null) {
          args.push(shortcutsValue[k]);
          args.push(obj);
        }
      }

      if (Array.isArray(key)) {
        key.forEach((part) => {
          setCb(part, { num: args.length / 2, buffer: WISESource.encode.apply(null, args) });
        });
      } else {
        setCb(key, { num: args.length / 2, buffer: WISESource.encode.apply(null, args) });
      }
    }
    endCb(null);
  }

  // ----------------------------------------------------------------------------
  tagsSetting () {
    const tagsField = this.api.addField('field:tags');
    const tags = this.api.getConfig(this.section, 'tags');
    if (tags) {
      const args = [];
      tags.split(',').map(item => item.trim()).forEach((part) => {
        args.push(tagsField, part);
      });
      this.tagsResult = { num: args.length / 2, buffer: WISESource.encode.apply(null, args) };
    } else {
      this.tagsResult = WISESource.emptyResult;
    }
  };

  // ----------------------------------------------------------------------------
  formatSetting () {
    this.format = this.api.getConfig(this.section, 'format', 'csv');
    if (this.format === 'csv') {
      this.parse = this.parseCSV;
    } else if (this.format === 'tagger') {
      this.parse = this.parseTagger;
    } else if (this.format === 'json') {
      this.parse = this.parseJSON;
    } else {
      throw new Error(`${this.section} - ERROR not loading unknown data format - ${this.format}`);
    }
  };

  // ----------------------------------------------------------------------------
  typeSetting () {
    this.type = this.api.getConfig(this.section, 'type');
    if (this.type === undefined) {
      throw new Error(`${this.section} - ERROR not loading since missing required type setting`);
    }
    this.typeFunc = this.api.funcName(this.type);
    if (this.getTypes === undefined) {
      this.getTypes = function () {
        return [this.type];
      };
    }
  };

  // ----------------------------------------------------------------------------
  /** A simple constant that should be used when needed to represent an empty result */
  static emptyResult = { num: 0, buffer: Buffer.alloc(0) };

  // ----------------------------------------------------------------------------
  static field2Pos = {};
  static field2Info = {};
  static pos2Field = {};
}
module.exports = WISESource;

// ----------------------------------------------------------------------------
WISESource.combineResults = function (results) {
  let num = 0;
  let len = 1;
  for (let a = 0; a < results.length; a++) {
    if (!results[a]) {
      continue;
    }
    num += results[a].num;
    len += results[a].buffer.length;
  }

  const buf = Buffer.allocUnsafe(len);
  let offset = 1;
  for (let a = 0; a < results.length; a++) {
    if (!results[a]) {
      continue;
    }

    results[a].buffer.copy(buf, offset);
    offset += results[a].buffer.length;
  }
  buf[0] = num;
  return buf;
};
// ----------------------------------------------------------------------------
WISESource.result2Str = function (result) {
  let collection = [];
  let offset = 1;
  for (let i = 0; i < result[0]; i++) {
    const pos = result[offset];
    const len = result[offset + 1];
    const value = result.toString('utf8', offset + 2, offset + 2 + len - 1);
    offset += 2 + len;
    collection.push({ field: WISESource.pos2Field[pos], len: len - 1, value: value });
  }

  return JSON.stringify(collection).replace(/},{/g, '},\n{');
};
// ----------------------------------------------------------------------------
WISESource.encode = function () {
  let l;
  let len = 0;
  for (let a = 1; a < arguments.length; a += 2) {
    l = Buffer.byteLength(arguments[a]);
    if (l > 250) {
      arguments[a] = arguments[a].substring(0, 240);
    }
    len += 3 + Buffer.byteLength(arguments[a]);
  }

  const buf = Buffer.allocUnsafe(len);
  let offset = 0;
  for (let a = 1; a < arguments.length; a += 2) {
      buf.writeUInt8(arguments[a - 1], offset);
      len = Buffer.byteLength(arguments[a]);
      buf.writeUInt8(len + 1, offset + 1);
      l = buf.write(arguments[a], offset + 2);
      buf.writeUInt8(0, offset + l + 2);
      offset += 3 + l;
  }
  return buf;
};
// ----------------------------------------------------------------------------
WISESource.request = function (url, file, cb) {
  const headers = {};
  if (file) {
    if (fs.existsSync(file)) {
      const stat = fs.statSync(file);

      // Don't download again if file is less then 1 minutes old
      if (Date.now() - stat.mtime.getTime() < 60000) {
        return setImmediate(cb, 304);
      }
      headers['If-Modified-Since'] = stat.mtime.toUTCString();
    }
  }
  let statusCode;
  console.log(url);
  request({ url: url, headers: headers })
  .on('response', function (response) {
    statusCode = response.statusCode;
    if (response.statusCode === 200) {
      this.pipe(fs.createWriteStream(file));
    }
  })
  .on('error', (error) => {
    console.log(error);
  })
  .on('end', () => {
    setTimeout(cb, 100, statusCode);
  })
  ;
};
WISESource.emptyCombinedResult = WISESource.combineResults([]);
// ----------------------------------------------------------------------------
// https://coderwall.com/p/pq0usg/javascript-string-split-that-ll-return-the-remainder
function splitRemain (str, separator, limit) {
    str = str.split(separator);
    if (str.length <= limit) { return str; }

    const ret = str.splice(0, limit);
    ret.push(str.join(separator));

    return ret;
}
