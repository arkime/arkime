/******************************************************************************/
/* Middle class for simple sources
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

const wiseSource = require('./wiseSource.js');
const iptrie = require('iptrie');

/**
 * The SimpleSource base class implements some common functions for
 * sources that only have one type.
 *
 * Sources need to
 * * call initSimple
 * * implement initSource, simpleSourceLoad
 * @extends WISESource
 */
class SimpleSource extends wiseSource {
  /**
   * SimpleSource
   * @param {WISESourceAPI} api - the api when source created
   * @param {string} section - the section name
   * @param {object} options - see WISESource constructor
   */
  constructor (api, section, options) {
    options.typeSetting = true;
    options.tagsSetting = true;
    options.formatSetting = true;
    super(api, section, options);
    this.column = +api.getConfig(section, 'column', 0);
    this.keyColumn = api.getConfig(section, 'keyColumn', 0);

    if (this.type === 'ip') {
      this.cache = { items: new Map(), trie: new iptrie.IPTrie() };
    } else {
      this.cache = new Map();
    }
  }

  // ----------------------------------------------------------------------------
  dump (res) {
    const cache = this.type === 'ip' ? this.cache.items : this.cache;
    cache.forEach((value, key) => {
      const str = `{key: "${key}", ops:\n` +
        wiseSource.result2Str(wiseSource.combineResults([this.tagsResult, value])) + '},\n';
      res.write(str);
    });
    res.end();
  };

  // ----------------------------------------------------------------------------
  sendResult (key, cb) {
    const result = this.type === 'ip' ? this.cache.trie.find(key) : this.cache.get(key);

    // Not found, or found but no extra values to add
    if (!result) {
      return cb(null, undefined);
    }
    if (result.num === 0) {
      return cb(null, this.tagsResult);
    }

    // Found, so combine the two results (per item, and per source)
    const newresult = { num: result.num + this.tagsResult.num, buffer: Buffer.concat([result.buffer, this.tagsResult.buffer]) };
    return cb(null, newresult);
  };
  // ----------------------------------------------------------------------------
  /**
   * This function should be called by the constructor of the source when all
   * config is verified and the source is ready to go online.
   * @returns {boolean} - On true the source was initialized with no issue
   */
  initSimple () {
    if (this.type === 'domain') {
      this.getDomain = function (domain, cb) {
        if (this.cache.get(domain)) {
          return this.sendResult(domain, cb);
        }
        domain = domain.substring(domain.indexOf('.') + 1);
        return this.sendResult(domain, cb);
      };
    } else {
      this[this.api.funcName(this.type)] = this.sendResult;
    }

    this.api.addSource(this.section, this);
    return true;
  };
  // ----------------------------------------------------------------------------
  getTypes () {
    return [this.type];
  };
  // ----------------------------------------------------------------------------
  load () {
    let setFunc;
    let newCache;
    let count = 0;
    if (this.type === 'ip') {
      newCache = { items: new Map(), trie: new iptrie.IPTrie() };
      setFunc = (key, value) => {
        key.split(',').forEach((cidr) => {
          const parts = cidr.split('/');
          try {
            newCache.trie.add(parts[0], +parts[1] || (parts[0].includes(':') ? 128 : 32), value);
          } catch (e) {
            console.log('ERROR adding', this.section, cidr, e);
          }
          newCache.items.set(cidr, value);
          count++;
        });
      };
    } else {
      newCache = new Map();
      if (this.type === 'url') {
        setFunc = (key, value) => {
          if (key.lastIndexOf('http://', 0) === 0) {
            key = key.substring(7);
          }
          newCache.set(key, value);
          count++;
        };
      } else {
        setFunc = function (key, value) {
          newCache.set(key, value);
          count++;
        };
      }
    }
    this.simpleSourceLoad(setFunc, (err) => {
      if (err) {
        console.log('ERROR loading', this.section, err);
        return;
      }
      this.cache = newCache;
      console.log(this.section, '- Done Loading', count, 'elements');
    });
  };
};

module.exports = SimpleSource;
