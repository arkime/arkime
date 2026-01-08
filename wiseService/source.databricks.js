/******************************************************************************/
/*
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const WISESource = require('./wiseSource.js');
const { DBSQLClient } = require('@databricks/sql');
const iptrie = require('arkime-iptrie');

class DatabricksSource extends WISESource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { typeSetting: true, tagsSetting: true });

    this.host = api.getConfig(section, 'host');
    this.path = api.getConfig(section, 'path');
    this.token = api.getConfig(section, 'token');
    this.query = api.getConfig(section, 'query');
    this.periodic = api.getConfig(section, 'periodic');
    this.mergeQuery = api.getConfig(section, 'mergeQuery');
    this.keyPath = api.getConfig(section, 'keyPath', api.getConfig(section, 'keyColumn', 0));

    ['host', 'path', 'token', 'query', 'keyPath'].forEach((item) => {
      if (this[item] === undefined) {
        console.log(this.section, `- ERROR not loading since no ${item} specified in config file`);
      }
    });

    if (this.periodic) {
      this.cacheTimeout = -1; // Don't cache
      this[this.api.funcName(this.type)] = this.sendResultPeriodic;
      setInterval(this.periodicRefresh.bind(this), 1000 * this.periodic);
    } else {
      this[this.api.funcName(this.type)] = this.sendResult;
    }

    const client = new DBSQLClient();

    client.connect({
      host: this.host,
      path: this.path,
      token: this.token
    }).then(async (client2) => {
      this.session = await client2.openSession();
    }).catch((err) => {
      console.log(err);
    });

    api.addSource(section, this, [this.type]);
  }

  // ----------------------------------------------------------------------------
  async periodicRefresh (firstTime) {
    let query = this.query;
    let merging = false;
    if (this.mergeQuery && !firstTime) {
      query = this.mergeQuery;
      merging = true;
    }

    const queryOperation = await this.session.executeStatement(query);
    const results = await queryOperation.fetchAll();
    await queryOperation.close();

    if (results === undefined || results.length === 0) {
      console.log(this.section, '- No results - ', results);
      return;
    }

    let cache;
    if (merging) {
      cache = this.cache;
    } else {
      if (this.type === 'ip') {
        cache = { items: new Map(), trie: new iptrie.IPTrie() };
      } else {
        cache = new Map();
      }
    }

    for (const item of results) {
      const key = item[this.keyPath];
      if (!key) { continue; }

      const args = [];
      for (const k in this.shortcuts) {
        if (item[k] !== undefined) {
          args.push(this.shortcuts[k].pos);
          if (Array.isArray(item[k])) {
            args.push(item[k][0]);
          } else {
            args.push(item[k]);
          }
        }
      }

      const newitem = WISESource.encodeResult.apply(null, args);

      if (this.type === 'ip') {
        const parts = key.split('/');
        cache.trie.add(parts[0], +parts[1] || (parts[0].includes(':') ? 128 : 32), newitem);
        cache.items.set(key, newitem);
      } else {
        cache.set(key, newitem);
      }
    }
    this.cache = cache;
  }

  // ----------------------------------------------------------------------------
  itemCount () {
    if (this.cache) {
      return this.type === 'ip' ? this.cache.items.size : this.cache.size;
    } else {
      return 0;
    }
  }

  // ----------------------------------------------------------------------------
  dump (res) {
    if (this.cache === undefined) {
      return res.end();
    }

    const cache = this.type === 'ip' ? this.cache.items : this.cache;
    cache.forEach((value, key) => {
      const str = `{"key": "${key}", "ops":\n` +
        WISESource.result2JSON(WISESource.combineResults([this.tagsResult, value])) + '},\n';
      res.write(str);
    });
    res.end();
  }

  // ----------------------------------------------------------------------------
  sendResultPeriodic (key, cb) {
    if (!this.cache) {
      return cb(null, undefined);
    }

    const result = this.type === 'ip' ? this.cache.trie.find(key) : this.cache.get(key);

    // Not found, or found but no extra values to add
    if (!result) {
      return cb(null, undefined);
    }
    if (result[0] === 0) {
      return cb(null, this.tagsResult);
    }

    // Found, so combine the two results (per item, and per source)
    const newresult = WISESource.combineResults([result, this.tagsResult]);
    return cb(null, newresult);
  }

  // ----------------------------------------------------------------------------
  async sendResult (key, cb) {
    const queryOperation = await this.session.executeStatement(this.query, { namedParameters: { SEARCHTERM: key } });
    const results = await queryOperation.fetchAll();
    await queryOperation.close();

    if (!results || results.length === 0) {
      return cb(null, undefined);
    }

    const item = results[0];

    const args = [];
    for (const k in this.shortcuts) {
      if (item[k] !== undefined) {
        args.push(this.shortcuts[k]);
        if (Array.isArray(item[k])) {
          args.push(item[k][0]);
        } else {
          args.push(item[k]);
        }
      }
    }
    const newresult = WISESource.combineResults([WISESource.encodeResult.apply(null, args), this.tagsResult]);
    return cb(null, newresult);
  }
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('databricks', {
    singleton: false,
    name: 'databricks',
    description: 'Fetch data from databricks',
    link: 'https://arkime.com/wise#databricks',
    displayable: true,
    fields: [
      { name: 'type', required: true, help: 'The wise query type this source supports' },
      { name: 'tags', required: false, help: 'Comma separated list of tags to set for matches', regex: '^[-a-z0-9,]+' },
      { name: 'host', required: true, help: 'The Databricks hostname' },
      { name: 'path', required: true, help: 'The Databricks path' },
      { name: 'token', required: true, password: true, help: 'The Databricks token' },
      { name: 'keyPath', required: true, help: 'The path to use from the returned data to use as the key' },
      { name: 'periodic', required: false, help: 'Should we do periodic queries or individual queries' },
      { name: 'query', required: true, help: 'The query to run against Databricks. For non periodic queries the parameter SEARCHTERM will be replaced with the key' },
      { name: 'mergeQuery', help: 'When in periodic mode, use this query after startup and merge the keyPath value into previous table' }
    ]
  });

  const sections = api.getConfigSections().filter((e) => { return e.match(/^databricks:/); });
  sections.forEach((section) => {
    return new DatabricksSource(api, section);
  });
};
