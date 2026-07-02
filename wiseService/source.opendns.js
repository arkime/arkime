/******************************************************************************/
/*
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const https = require('https');
const WISESource = require('./wiseSource.js');

class OpenDNSSource extends WISESource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { });
    this.key = this.api.getConfig('opendns', 'key');
    if (this.key === undefined) {
      console.log(this.section, '- No key defined');
      return;
    }

    this.waiting = [];
    this.processing = new Map();
    this.categories = {}; // filled async by getCategories
    this.statuses = { '-1': 'malicious', 0: 'unknown', 1: 'benign' };

    this.api.addSource('opendns', this, ['domain']);
    this.getCategories();
    setInterval(this.getCategories.bind(this), 10 * 60 * 1000);
    setInterval(this.performQuery.bind(this), 500);

    this.statusField = this.api.addField('field:opendns.domain.status;db:opendns.status;kind:lotermfield;friendly:Status;help:OpenDNS domain security status;count:true');
    this.scField = this.api.addField('field:opendns.domain.security;db:opendns.securityCategory;kind:termfield;friendly:Security;help:OpenDNS domain security category;count:true');
    this.ccField = this.api.addField('field:opendns.domain.content;db:opendns.contentCategory;kind:termfield;friendly:Content;help:OpenDNS domain content category;count:true');

    this.api.addView('opendns',
      'if (session.opendns)\n' +
      '  div.sessionDetailMeta.bold OpenDNS\n' +
      '  dl.sessionDetailMeta\n' +
      "    +arrayList(session.opendns, 'status', 'Status', 'opendns.domain.status')\n" +
      "    +arrayList(session.opendns, 'securityCategory', 'Security Cat', 'opendns.domain.security')\n" +
      "    +arrayList(session.opendns, 'contentCategory', 'Content Cat', 'opendns.domain.content')\n"
    );

    this.api.addValueAction('opendnsip', { name: 'OpenDNS', url: 'https://sgraph.opendns.com/ip-view/%TEXT%', category: 'ip' });
    this.api.addValueAction('opendnsasn', { name: 'OpenDNS', url: 'https://sgraph.opendns.com/as-view/%REGEX%', category: 'asn', regex: '^[Aa][Ss](\\d+)' });
    this.api.addValueAction('opendnshost', { name: 'OpenDNS', url: 'https://sgraph.opendns.com/domain-view/name/%HOST%/view', category: 'host' });
  }

  // ----------------------------------------------------------------------------
  getCategories () {
    const options = {
      host: 'sgraph.api.opendns.com',
      port: '443',
      path: '/domains/categories/',
      method: 'GET',
      headers: {
        Authorization: 'Bearer ' + this.key
      }
    };

    let response = '';
    const request = https.request(options, (res) => {
      res.on('data', (chunk) => {
        response += chunk;
      });
      res.on('end', () => {
        try {
          this.categories = JSON.parse(response);
        } catch (e) {
          console.log(this.section, '- ERROR parsing categories', e);
        }
      });
    });
    request.on('error', (err) => {
      console.log(this.section, err);
    });

    request.end();
  }

  // ----------------------------------------------------------------------------
  performQuery () {
    if (this.waiting.length === 0) {
      return;
    }

    if (this.api.debug > 0) {
      console.log(this.section, '- Fetching', this.waiting.length);
    }

    // http://stackoverflow.com/questions/6158933/how-to-make-an-http-post-request-in-node-js/6158966
    // console.log("doing query:", waiting.length, "current cache", Object.keys(cache).length);
    const postData = JSON.stringify(this.waiting);
    const sent = this.waiting.slice(); // for error cleanup
    this.waiting.length = 0;

    // On failure invoke and remove all callbacks for this batch so queries
    // don't hang in processing forever
    const failBatch = (err) => {
      for (const domain of sent) {
        const cbs = this.processing.get(domain);
        if (!cbs) { continue; }
        this.processing.delete(domain);
        let cb;
        while ((cb = cbs.shift())) {
          cb(err);
        }
      }
    };

    const postOptions = {
      host: 'sgraph.api.opendns.com',
      port: '443',
      path: '/domains/categorization/',
      method: 'POST',
      headers: {
        Authorization: 'Bearer ' + this.key,
        'Content-Type': 'application/x-www-form-urlencoded',
        'Content-Length': Buffer.byteLength(postData)
      }
    };

    let response = '';
    const request = https.request(postOptions, (res) => {
      res.on('data', (chunk) => {
        response += chunk;
      });
      res.on('end', (err) => {
        let results;
        try {
          results = JSON.parse(response);
        } catch (e) {
          console.log(this.section, 'Error parsing for request:\n', postData, '\nresponse:\n', response);
          failBatch('opendns parse error');
          return;
        }

        for (let result in results) {
          const cbs = this.processing.get(result);
          if (!cbs) {
            continue;
          }
          this.processing.delete(result);

          const args = [this.statusField, this.statuses[results[result].status] || ('' + results[result].status)];

          if (results[result].security_categories) {
            results[result].security_categories.forEach((value) => {
              if (this.categories[value]) {
                args.push(this.scField, this.categories[value]);
              } else {
                console.log(this.section, 'Bad OpenDNS SC', value);
              }
            });
          }

          if (results[result].content_categories) {
            results[result].content_categories.forEach((value) => {
              if (this.categories[value]) {
                args.push(this.ccField, this.categories[value]);
              } else {
                console.log(this.section, 'Bad OpenDNS CC', value, results);
              }
            });
          }

          result = WISESource.encodeResult.apply(null, args);

          let cb;
          while ((cb = cbs.shift())) {
            cb(null, result);
          }
        }
      });
    });
    request.on('error', (err) => {
      console.log(this.section, err);
      failBatch(err);
    });

    // post the data
    request.write(postData);
    request.end();
  }

  // ----------------------------------------------------------------------------
  getDomain (domain, cb) {
    if (this.processing.has(domain)) {
      this.processing.get(domain).push(cb);
      return;
    }

    this.processing.set(domain, [cb]);
    this.waiting.push(domain);
    if (this.waiting.length > 1000) {
      this.performQuery();
    }
  }
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('opendns', {
    singleton: true,
    name: 'opendns',
    description: 'OpenDNS source for domain names',
    link: 'https://arkime.com/wise#opendns-umbrella',
    types: ['domain'],
    fields: [
      { name: 'key', password: true, required: true, help: 'The API key' }]
  });

  return new OpenDNSSource(api, 'opendns');
};
// ----------------------------------------------------------------------------
