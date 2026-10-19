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
const https = require('https');

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

    const insecure = api.getConfig(section, 'insecure', false);
    if (api.insecure || insecure === true || insecure === 'true') { // ini values are strings
      this.httpsAgent = new https.Agent({ rejectUnauthorized: false });
    }

    this[this.api.funcName(this.type)] = this.sendResult;
    api.addSource(section, this, [this.type]);

  }

  // ----------------------------------------------------------------------------
  async sendResult (key, cb) {

    // Replace with functions so a $& or $' in the looked up value isn't
    // treated as a replacement pattern. Standard base64 has / + = so it is
    // still url encoded; base64url is url safe as is.
    const b64 = Buffer.from(key).toString('base64');
    const url = this.url
      .replace(/{value}/g, () => encodeURIComponent(key))
      .replace(/{valueBase64}/g, () => encodeURIComponent(b64))
      .replace(/{valueBase64Url}/g, () => b64.replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, ''));

    axios.get(url, { headers: this.headers, httpsAgent: this.httpsAgent })
      .then((response) => {
        const json = (typeof response.data === 'string') ? JSON.parse(response.data) : response.data;
        const eskey = json[this.resultField];
        if (eskey === undefined) {
          return cb(null, undefined);
        }
        const args = this.parseJSONElement(json);
        const newresult = WISESource.combineResults([WISESource.encodeResult.apply(null, args), this.tagsResult]);
        return cb(null, newresult);
      }).catch((err) => {
        // axios rejects non-2xx, so a 404 (no result for this key) lands here
        if (err.response && err.response.status === 404) {
          return cb(null, undefined);
        }
        return cb(err);
      });
  }
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('urlapi', {
    singleton: false,
    name: 'urlapi',
    description: 'Use a web url to load data into WISE, a single call at a time, not recommended.',
    link: 'https://arkime.com/wise#url',
    cacheable: false,
    displayable: true,
    fields: [
      { name: 'type', required: true, help: 'The wise query type this source supports' },
      { name: 'tags', required: false, help: 'Comma separated list of tags to set for matches', regex: '^[-a-z0-9,]+' },
      { name: 'url', required: true, help: 'The URL to load, {value} is replaced with the url encoded value, {valueBase64} with the base64 (url encoded) value, {valueBase64Url} with the base64url value' },
      { name: 'resultField', required: true, help: 'Field that is required to be in the result' },
      { name: 'headers', required: false, multiline: ';', help: 'List of headers to send in the URL request' },
      { name: 'insecure', required: false, regex: '^(true|false)$', help: 'Set to true to disable TLS certificate verification' }
    ]
  });

  const sections = api.getConfigSections().filter((e) => { return e.match(/^urlapi:/); });
  sections.forEach((section) => {
    return new URLApiSource(api, section);
  });
};
// ----------------------------------------------------------------------------
