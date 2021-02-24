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
   * @param {boolean} [options.formatSetting=false] - load the format setting with default the provided value if not false
   * @param {boolean} [options.fullQuery=false] - for MD5/SHA, query will be query.value and query.contentType
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
    this.requestStat = 0;
    this.cacheHitStat = 0;
    this.cacheMissStat = 0;
    this.cacheRefreshStat = 0;
    this.requestDroppedStat = 0;
    this.directHitStat = 0;
    this.recentAverageMS = 0;
    this.srcInProgress = {};
    this.fullQuery = !!options.fullQuery;

    if (options.typeSetting) {
      this.typeSetting();
    }

    if (options.tagsSetting) {
      this.tagsSetting();
    }

    if (options.formatSetting) {
      this.formatSetting(options.formatSetting);
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
        const parts = item.split('/');
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
        this.shortcuts[match[1]] = { pos: pos, mod: 0 };
        const kind = line.match(/kind:([^;]+)/);
        if (kind) {
          if (kind[1] === 'lotermfield') {
            this.shortcuts[match[1]].mod = 1;
          } else if (kind[1] === 'uptermfield') {
            this.shortcuts[match[1]].mod = 2;
          }
        }
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
          if (data[i][k] !== undefined && data[i][k] !== '') {
            args.push(this.shortcuts[k].pos);

            if (this.shortcuts[k].mod === 1) {
              args.push(data[i][k].toLowerCase());
            } else if (this.shortcuts[k].mod === 2) {
              args.push(data[i][k].toUpperCase());
            } else {
              args.push(data[i][k]);
            }
          }
        }

        if (args.length === 0) {
          setCb(data[i][this.column], WISESource.emptyResult);
        } else {
          setCb(data[i][this.column], WISESource.encodeResult.apply(null, args));
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
          args.push(this.shortcuts[kv[0]].pos);
        } else if (WISESource.field2Pos[kv[0]]) {
          args.push(WISESource.field2Pos[kv[0]]);
        } else {
          args.push(this.api.addField('field:' + kv[0]));
        }
        args.push(kv[1]);
      }
      setCb(parts[0], WISESource.encodeResult.apply(null, args));
    }
    if (this.view !== '') {
      this.api.addView(this.section, this.view);
    }
    endCb(null);
  }

  // ----------------------------------------------------------------------------
  /**
   * Util function to parse JSON formatted data
   * @param {string} body - the raw JSON data
   * @param {function} setCb - the function to call for each row found
   * @param {function} endCB - all done parsing
   */
  parseJSON (body, setCb, endCb) {
    try {
      let json = JSON.parse(body);

      if (this.keyPath === undefined) {
        return endCb('No keyPath set');
      }

      if (this.arrayPath !== undefined) {
        const arrayPath = this.arrayPath.split('.');
        for (let i = 0; i < arrayPath.length; i++) {
          json = json[arrayPath[i]];
          if (!json) {
            return endCb(`Couldn't find ${arrayPath[i]} in results`);
          }
        }
      }

      const keyPath = this.keyPath.split('.');

      // Convert shortcuts into array of key path
      const shortcuts = [];
      const shortcutsValue = [];
      for (const k in this.shortcuts) {
        shortcuts.push(k.split('.'));
        shortcutsValue.push(this.shortcuts[k]);
      }

      for (let i = 0; i < json.length; i++) {
        // Walk the key path
        let key = json[i];
        for (let j = 0; key && j < keyPath.length; j++) {
          key = key[keyPath[j]];
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
          if (obj !== undefined && obj !== null && obj !== '') {
            args.push(shortcutsValue[k].pos);
            if (shortcutsValue[k].mod === 1) {
              args.push(obj.toLowerCase());
            } else if (shortcutsValue[k].mod === 2) {
              args.push(obj.toUpperCase());
            } else {
              args.push(obj);
            }
          }
        }

        if (Array.isArray(key)) {
          key.forEach((part) => {
            setCb(part, WISESource.encodeResult.apply(null, args));
          });
        } else {
          setCb(key, WISESource.encodeResult.apply(null, args));
        }
      }
      endCb(null);
    } catch (e) {
      endCb(e);
    }
  }

  /** The encoded tags result if options.tagsSetting was set to true */
  tagsResult;

  // ----------------------------------------------------------------------------
  tagsSetting () {
    const tagsField = this.api.addField('field:tags');
    const tags = this.api.getConfig(this.section, 'tags');
    if (tags) {
      const args = [];
      tags.split(',').map(item => item.trim()).forEach((part) => {
        args.push(tagsField, part);
      });
      this.tagsResult = WISESource.encodeResult.apply(null, args);
    } else {
      this.tagsResult = WISESource.emptyResult;
    }
  };

  /** The format of the source if options.formatSetting was set to true. */
  format;

  /** {function} The parser function of the source if options.formatSetting was set to true. */
  parse;

  // ----------------------------------------------------------------------------
  formatSetting (d) {
    this.format = this.api.getConfig(this.section, 'format', d);
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

  /** The wise item type of the source if options.typeSetting was set to true. */
  type;

  // ----------------------------------------------------------------------------
  typeSetting () {
    this.type = this.api.getConfig(this.section, 'type');
    if (this.type === undefined) {
      throw new Error(`${this.section} - ERROR not loading since missing required type setting`);
    }
    this.typeFunc = this.api.funcName(this.type);
  };

  // ----------------------------------------------------------------------------
  /** A simple constant that should be used when needing to represent an empty result */
  static emptyResult = Buffer.alloc(1);

  // ----------------------------------------------------------------------------
  static field2Pos = {};
  static field2Info = {};
  static pos2Field = {};

  // ----------------------------------------------------------------------------
  /**
   * Convert field ids and string values into the encoded form used in WISE.
   *
   * This method tags a variable number of arguments, each in a pair of field id and string value.
   * @returns {buffer} - the endcoded results
   */
  static encodeResult () {
    let l;
    let len = 0;
    for (let a = 1; a < arguments.length; a += 2) {
      l = Buffer.byteLength(arguments[a]);
      if (l > 250) {
        arguments[a] = arguments[a].substring(0, 240);
      }
      len += 3 + Buffer.byteLength(arguments[a]);
    }

    const buf = Buffer.allocUnsafe(len + 1);
    let offset = 1;
    for (let a = 1; a < arguments.length; a += 2) {
      buf.writeUInt8(arguments[a - 1], offset);
      len = Buffer.byteLength(arguments[a]);
      buf.writeUInt8(len + 1, offset + 1);
      l = buf.write(arguments[a], offset + 2);
      buf.writeUInt8(0, offset + l + 2);
      offset += 3 + l;
    }
    buf[0] = arguments.length / 2;
    return buf;
  };

  // ----------------------------------------------------------------------------
  /**
   * Combine a array of encoded results into one encoded result
   *
   * @param {object|array} results - Array of results
   * @returns {Buffer} - A single combined result
   */
  static combineResults (results) {
    // Don't really need to combine 1 result
    if (results.length === 1) {
      return results[0] ? results[0] : WISESource.emptyResult;
    }

    let num = 0;
    let len = 0;
    for (let a = 0; a < results.length; a++) {
      if (!results[a]) {
        continue;
      }
      num += results[a][0];
      len += results[a].length - 1;
    }

    if (len === 0) { return WISESource.emptyResult; }

    const buf = Buffer.allocUnsafe(len + 1);
    let offset = 1;
    for (let a = 0; a < results.length; a++) {
      if (!results[a]) {
        continue;
      }

      results[a].copy(buf, offset, 1);
      offset += results[a].length - 1;
    }
    buf[0] = num;
    return buf;
  };

  // ----------------------------------------------------------------------------
  /**
   * Convert an encoded combined results binary buffer into JSON string
   *
   * @param {Buffer} results - The combined results from encode
   * @returns {string} - The JSON string
   */
  static result2JSON (results) {
    const collection = [];
    let offset = 1;
    for (let i = 0; i < results[0]; i++) {
      const pos = results[offset];
      const len = results[offset + 1];
      const value = results.toString('utf8', offset + 2, offset + 2 + len - 1);
      offset += 2 + len;
      collection.push({ field: WISESource.pos2Field[pos], len: len - 1, value: value });
    }

    return JSON.stringify(collection).replace(/},{/g, '},\n{');
  };

  // ----------------------------------------------------------------------------
  /**
   * Download a url and save to a file, if we already have the file and less than a minute old use that file.
   *
   * @param {string} url - The URL to download
   * @param {string} file - The file to save the results to
   * @param {function} cb - (statusCode) The stats code result from the download
   */
  static request (url, file, cb) {
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
      });
  };

  // ----------------------------------------------------------------------------
  /**
   * For sources that support it, get the number of items loaded into memory.
   *
   * @returns {integer} the number of items loaded into memory
   */
  itemCount () {
    return 0;
  };
}
/**
 * Get the raw source data for editing.
 * Source should implement this method if they want to support editing the data for a source.
 *
 * @method
 * @name WISESource#getSourceRaw
 * @param {function} cb - (err, data)
 * @abstract
 */
/**
 * Put the raw source data after editing.
 * Source should implement this method if they want to support editing the data for a source.
 *
 * @method
 * @name WISESource#putSourceRaw
 * @param {string} data - The full data for the source from UI
 * @param {function} cb - (err)
 * @abstract
 */
/**
 * Dump the sources data to caller.
 * Source should implement this method if they want to support displaying the current state.
 *
 * @method
 * @name WISESource#dump
 * @param {object} res - The express res object
 * @abstract
 */

/**
 * Every source needs to implement this method. If a singleton it will just create the source object direction.
 * If not it should loop thru all keys that start with sourcekind:
 * @method
 * @name WISESource.initSource
 * @param {WISESourceAPI} api - The api back into the WISE Service
 * @abstract
 * @example
 * exports.initSource = function (api) {
 *   api.addSourceConfigDef('sourcename', {
 *     singleton: false,
 *     name: 'sourcename',
 *     description: 'This is the best source ever',
 *     fields: [
 *       { name: 'type', required: true, help: 'The wise query type this source supports' },
 *       { name: 'tags', required: false, help: 'Comma separated list of tags to set for matches', regex: '^[-a-z0-9,]+' }
 *     ]
 *   });
 *   new TheSource(api);
 * }
 */

module.exports = WISESource;

// ----------------------------------------------------------------------------
// https://coderwall.com/p/pq0usg/javascript-string-split-that-ll-return-the-remainder
function splitRemain (str, separator, limit) {
  str = str.split(separator);
  if (str.length <= limit) { return str; }

  const ret = str.splice(0, limit);
  ret.push(str.join(separator));

  return ret;
}
