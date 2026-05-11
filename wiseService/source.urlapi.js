/******************************************************************************/
/*
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const WISESource = require('./wiseSource.js');
const axios = require('axios');

class URLApiSource extends WISESource {
// ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { typeSetting: true, tagsSetting: true });

    this.url = api.getConfig(section, 'url');
    this.headers = {};
    const headers = api.getConfig(section, 'headers');

    if (this.url === undefined) {
      console.log(this.section, '- ERROR not loading since no url specified in config file');
      return;
    }

    if (headers) {
      headers.split(';').forEach((header) => {
        const parts = header.split(':').map(item => item.trim());
        if (parts.length === 2) {
          this.headers[parts[0]] = parts[1];
        }
      });
    }

    this.resultField = api.getConfig(section, 'resultField', 0);

    this[this.api.funcName(this.type)] = this.sendResult;
    api.addSource(section, this, [this.type]);

    this.sourceFields = [this.resultField];
    for (const k in this.shortcuts) {
      if (this.sourceFields.indexOf(k) === -1) {
        this.sourceFields.push(k);
      }
    }
  }

  // ----------------------------------------------------------------------------
  async sendResult (key, cb) {

    let url = `${this.url}`;

    url = url.replace(/{value}/g, encodeURIComponent(key));

    axios.get(url, { headers: this.headers })
      .then((response) => {
        if (response.status === 404) {
          return cb(null, undefined);
        }

        const json = JSON.parse(response.data);
        const eskey = json[this.resultField];
        if (eskey === undefined) {
          return cb(null, undefined);
        }
        const args = this.parseJSONElement(json);
        const newresult = WISESource.combineResults([WISESource.encodeResult.apply(null, args), this.tagsResult]);
        return cb(null, newresult);
      }).catch((err) => {
        return cb(err);
      });
  }
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('urlapi', {
    singleton: false,
    name: 'urlapi',
    description: 'Use a web url to load data into wise a single call at a time, not recommended.',
    link: 'https://arkime.com/wise#url',
    cacheable: false,
    displayable: true,
    fields: [
      { name: 'type', required: true, help: 'The wise query type this source supports' },
      { name: 'tags', required: false, help: 'Comma separated list of tags to set for matches', regex: '^[-a-z0-9,]+' },
      { name: 'url', required: true, help: 'The URL to load, {value} will be replaced' },
      { name: 'resultField', required: true, help: 'Field that is required to be in the result' },
      { name: 'headers', required: false, multiline: ';', help: 'List of headers to send in the URL request' }
    ]
  });

  const sections = api.getConfigSections().filter((e) => { return e.match(/^urlapi:/); });
  sections.forEach((section) => {
    return new URLApiSource(api, section);
  });
};
// ----------------------------------------------------------------------------
