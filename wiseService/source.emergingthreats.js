/******************************************************************************/
/*
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const fs = require('fs');
const csv = require('csv');
const WISESource = require('./wiseSource.js');

class EmergingThreatsSource extends WISESource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { dontCache: true });
    this.key = api.getConfig('emergingthreats', 'key');

    if (this.key === undefined) {
      console.log(this.section, '- No key defined');
      return;
    }

    this.ips = new Map();
    this.domains = new Map();
    this.categories = {};

    this.api.addSource('emergingthreats', this, ['ip', 'domain']);

    this.scoreField = this.api.addField('field:emergingthreats.score;db:et.score;kind:integer;friendly:Score;help:Emerging Threats Score;count:true');
    this.categoryField = this.api.addField('field:emergingthreats.category;db:et.category;kind:termfield;friendly:Category;help:Emerging Threats Category;count:true');

    this.api.addView('emergingthreats',
      'if (session.et)\n' +
      '  div.sessionDetailMeta.bold Emerging Threats\n' +
      '  dl.sessionDetailMeta\n' +
      "    +arrayList(session.et, 'category', 'Category', 'emergingthreats.category')\n" +
      "    +arrayList(session.et, 'score', 'Score', 'emergingthreats.score')\n"
    );

    setImmediate(this.loadFiles.bind(this));
    setInterval(this.loadFiles.bind(this), 60 * 60 * 1000); // Reload files every hour
  }

  // ----------------------------------------------------------------------------
  parseCategories (fn) {
    const parser = csv.parse({ skip_empty_lines: true }, (err, data) => {
      if (err) {
        console.log(this.section, "- Couldn't parse", fn, 'csv', err);
        return;
      }

      this.categories = {};
      for (let i = 0; i < data.length; i++) {
        this.categories[data[i][0]] = data[i][1];
      }
    });
    fs.createReadStream(fn).on('error', (err) => {
      console.log(this.section, "- Couldn't read", fn, err);
    }).pipe(parser);
  }

  // ----------------------------------------------------------------------------
  parseRepData (fn, hashName) {
    const parser = csv.parse({ skip_empty_lines: true }, (err, data) => {
      if (err) {
        console.log(this.section, "- Couldn't parse", fn, 'csv', err);
        return;
      }

      // Build a fresh map and swap on success so a failed load keeps prior data
      const hash = new Map();
      for (let i = 1; i < data.length; i++) {
        if (data[i].length !== 3) {
          continue;
        }

        let encoded = WISESource.encodeResult(
          this.categoryField, this.categories[data[i][1]] || ('Unknown - ' + data[i][1]),
          this.scoreField, '' + data[i][2]
        );

        // Already have something for this item, add the new one
        const value = hash.get(data[i][0]);
        if (value) {
          encoded = WISESource.combineResults([value, encoded]);
        }

        hash.set(data[i][0], encoded);
      }
      this[hashName] = hash;
      console.log(this.section, '- Done Loading', fn);
    });
    fs.createReadStream(fn).on('error', (err) => {
      console.log(this.section, "- Couldn't read", fn, err);
    }).pipe(parser);
  }

  // ----------------------------------------------------------------------------
  loadFiles () {
    console.log(this.section, '- Downloading Files');
    WISESource.request('https://rules.emergingthreatspro.com/' + this.key + '/reputation/categories.txt', '/tmp/categories.txt', (statusCode) => {
      if (statusCode === 200 || !this.categoriesLoaded) {
        this.categoriesLoaded = true;
        this.parseCategories('/tmp/categories.txt');
      }
    });

    WISESource.request('https://rules.emergingthreatspro.com/' + this.key + '/reputation/iprepdata.csv', '/tmp/iprepdata.csv', (statusCode) => {
      if (statusCode === 200 || !this.ipsLoaded) {
        this.ipsLoaded = true;
        this.parseRepData('/tmp/iprepdata.csv', 'ips');
      }
    });

    WISESource.request('https://rules.emergingthreatspro.com/' + this.key + '/reputation/domainrepdata.csv', '/tmp/domainrepdata.csv', (statusCode) => {
      if (statusCode === 200 || !this.domainsLoaded) {
        this.domainsLoaded = true;
        this.parseRepData('/tmp/domainrepdata.csv', 'domains');
      }
    });
  }

  // ----------------------------------------------------------------------------
  getDomain (domain, cb) {
    cb(null, this.domains.get(domain) || this.domains.get(domain.substring(domain.indexOf('.') + 1)));
  }

  // ----------------------------------------------------------------------------
  getIp (ip, cb) {
    cb(null, this.ips.get(ip));
  }

  // ----------------------------------------------------------------------------
  itemCount () {
    return this.ips.size + this.domains.size;
  }

  // ----------------------------------------------------------------------------
  dump (res) {
    ['ips', 'domains'].forEach((ckey) => {
      res.write(`${ckey}:\n`);
      this[ckey].forEach((value, key) => {
        const str = `{"key": ${JSON.stringify(key)}, "ops":\n` +
          WISESource.result2JSON(value) + '},\n';
        res.write(str);
      });
    });
    res.end();
  }
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('emergingthreats', {
    singleton: true,
    name: 'emergingthreats',
    description: 'Link to the emergingthreats data',
    link: 'https://arkime.com/wise#emerging-threats-pro',
    types: ['ip', 'domain'],
    cacheable: false,
    displayable: true,
    fields: [
      { name: 'key', password: true, required: true, help: 'The API key' }
    ]
  });

  return new EmergingThreatsSource(api, 'emergingthreats');
};
