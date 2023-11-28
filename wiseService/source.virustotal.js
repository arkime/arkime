/******************************************************************************/
/*
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const axios = require('axios');
const WISESource = require('./wiseSource.js');

let source;

class VirusTotalSource extends WISESource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { fullQuery: true });
    this.waiting = [];
    this.processing = new Map();

    this.key = this.api.getConfig('virustotal', 'key');
    if (this.key === undefined) {
      console.log(this.section, '- No key defined');
      return;
    }

    this.contentTypes = {};
    const contentTypes = this.api.getConfig('virustotal', 'contentTypes',
      'application/x-dosexec,application/vnd.ms-cab-compressed,application/pdf,application/x-shockwave-flash,application/x-java-applet,application/jar').split(',').map(item => item.trim());
    contentTypes.forEach((type) => { this.contentTypes[type] = 1; });

    this.queriesPerMinute = +this.api.getConfig('virustotal', 'queriesPerMinute', 3); // Keeps us under default limit, however most wise queries will time out :(
    this.maxOutstanding = +this.api.getConfig('virustotal', 'maxOutstanding', 25);
    this.dataSources = this.api.getConfig('virustotal', 'dataSources', 'McAfee,Symantec,Microsoft,Kaspersky').split(',').map(item => item.trim());
    this.dataSourcesLC = this.dataSources.map((x) => { return x.toLowerCase(); });
    this.dataFields = [];

    this.api.addSource('virustotal', this, ['md5']);
    setInterval(this.performQuery.bind(this), 60000 / this.queriesPerMinute);

    let str =
      'if (session.virustotal)\n' +
      '  div.sessionDetailMeta.bold VirusTotal\n' +
      '  dl.sessionDetailMeta\n' +
      "    +arrayList(session.virustotal, 'hits', 'Hits', 'virustotal.hits')\n" +
      "    +arrayList(session.virustotal, 'links', 'Links', 'virustotal.links')\n";

    for (let i = 0; i < this.dataSources.length; i++) {
      const uc = this.dataSources[i];
      const lc = this.dataSourcesLC[i];
      this.dataFields[i] = this.api.addField(`field:virustotal.${lc};db:virustotal.${lc};kind:lotermfield;friendly:${uc};help:VirusTotal ${uc} Status;count:true`);
      str += "    +arrayList(session.virustotal, '" + lc + "', '" + uc + "', 'virustotal." + lc + "')\n";
    }

    this.hitsField = this.api.addField('field:virustotal.hits;db:virustotal.hits;kind:integer;friendly:Hits;help:VirusTotal Hits;count:true');
    this.linksField = this.api.addField('field:virustotal.links;db:virustotal.links;kind:termfield;friendly:Link;help:VirusTotal Link;count:true');

    this.api.addValueAction('virustotallinks', { name: 'Open', url: '%TEXT%', fields: 'virustotal.links' });
    this.api.addValueAction('virustotalmd5', { name: 'Virus Total MD5 Search', url: 'https://www.virustotal.com/gui/search/%TEXT%', category: 'md5' });

    this.api.addView('virustotal', str);
  }

  // ----------------------------------------------------------------------------
  performQuery = function () {
    if (this.waiting.length === 0) {
      return;
    }

    if (this.api.debug > 0) {
      console.log(this.section, '- Fetching %d', this.waiting.length);
    }

    const options = {
      url: 'https://www.virustotal.com/vtapi/v2/file/report?',
      params: { apikey: this.key, resource: this.waiting.join(',') },
      method: 'GET'
    };
    const sent = this.waiting;

    this.waiting = [];

    axios(options)
      .then((response) => {
        let results = response.data;
        if (response.status !== 200) {
          console.log(this.section, 'Error for request:\n', options, '\n', '\nresults:\n', results);
          sent.forEach((md5) => {
            const cb = this.processing.get(md5);
            if (!cb) {
              return;
            }
            this.processing.delete(md5);
            cb(undefined, undefined);
          });
          return;
        }

        if (!Array.isArray(results)) {
          results = [results];
        }

        results.forEach((result) => {
          const cb = this.processing.get(result.md5);
          if (!cb) {
            return;
          }
          this.processing.delete(result.md5);

          let wiseResult;
          if (result.response_code === 0) {
            wiseResult = WISESource.emptyResult;
          } else {
            const args = [this.hitsField, '' + result.positives, this.linksField, result.permalink];

            for (let i = 0; i < this.dataSources.length; i++) {
              const uc = this.dataSources[i];

              if (result.scans[uc] && result.scans[uc].detected) {
                args.push(this.dataFields[i], result.scans[uc].result);
              }
            }

            wiseResult = WISESource.encodeResult.apply(null, args);
          }

          cb(null, wiseResult);
        });
      }).catch((err) => {
        console.log(this.section, err);
      });
  };

  // ----------------------------------------------------------------------------
  getMd5 (query, cb) {
    if (query.contentType !== undefined && this.contentTypes[query.contentType] !== 1) {
      return cb(null, undefined);
    }

    this.processing.set(query.value, cb);
    if (this.waiting.length < this.maxOutstanding) {
      this.waiting.push(query.value);
    } else {
      return cb('dropped');
    }
  };
}

// ----------------------------------------------------------------------------
const reportApi = function (req, res) {
  source.getMd5(req.query.resource, (err, result) => {
    // console.log(err, result);
    if (result[0] === 0) {
      res.send({ response_code: 0, resource: req.query.resource, verbose_msg: 'The requested resource is not among the finished, queued or pending scans' });
    } else {
      const obj = { scans: {} };
      let offset = 1;
      for (let i = 0; i < result[0]; i++) {
        const pos = result[offset];
        const len = result[offset + 1];
        const value = result.toString('utf8', offset + 2, offset + 2 + len - 1);
        offset += 2 + len;
        switch (pos) {
        case source.hitsField:
          obj.positives = +value;
          break;
        case source.linksField:
          obj.permalink = value;
          break;
        default:
          for (let j = 0; j < source.dataFields.length; j++) {
            if (source.dataFields[j] === pos) {
              obj.scans[source.dataSources[j]] = { detected: true, result: value };
              break;
            }
          }
        }
      }
      res.send(obj);
    }
  });
};

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('virustotal', {
    singleton: true,
    name: 'virustotal',
    description: 'Link to the virustotal data',
    types: ['md5'],
    fields: [
      { name: 'key', password: true, required: true, help: 'The API key' },
      { name: 'contentTypes', required: false, help: 'Which content types to look up' },
      { name: 'queriesPerMinute', required: false, help: 'The number of queries per minute (defaults to 3)' },
      { name: 'maxOutstanding', required: false, help: 'Max number of outstanding queries at one time (defaults to 25)' },
      { name: 'dataSources', required: false, help: 'The data sources (defaults to McAfee,Symantec,Microsoft,Kaspersky)' }
    ]
  });

  api.app.get('/vtapi/v2/file/report', reportApi);
  source = new VirusTotalSource(api, 'virustotal');
};
// ----------------------------------------------------------------------------
