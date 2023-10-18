/******************************************************************************/
/*
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const SimpleSource = require('./simpleSource.js');
const axios = require('axios');

class URLSource extends SimpleSource {
// ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { reload: true });
    this.url = api.getConfig(section, 'url');
    this.urlScrapeRedirect = api.getConfig(section, 'urlScrapeRedirect');
    if (this.urlScrapeRedirect) {
      this.urlScrapeRedirect = new RegExp(this.urlScrapeRedirect);
    }
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
  }

  // ----------------------------------------------------------------------------
  simpleSourceLoad (cb) {
    if (!this.url) {
      return;
    }

    axios.get(this.url, { headers: this.headers, transformResponse: x => x })
      .then((response) => {
        console.log(this.url, response.status);
        if (response.status !== 200) {
          return cb(response.status);
        }
        if (this.urlScrapeRedirect) {
          const match = response.data.match(this.urlScrapeRedirect);
          if (!match) {
            return cb('URL Scrape not found');
          }

          axios.get(match[0], { headers: this.headers, transformResponse: x => x })
            .then((subResponse) => {
              return cb(null, subResponse.data);
            }).catch((subError) => {
              return cb(subError);
            });
        } else {
          return cb(null, response.data);
        }
      }).catch((error) => {
        return cb(error);
      });
  }
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('url', {
    singleton: false,
    name: 'url',
    description: 'Use a web url to load data into wise. The url can be periodically reloaded.',
    link: 'https://arkime.com/wise#url',
    cacheable: false,
    displayable: true,
    fields: [
      { name: 'type', required: true, help: 'The wise query type this source supports' },
      { name: 'tags', required: false, help: 'Comma separated list of tags to set for matches', regex: '^[-a-z0-9,]+' },
      { name: 'format', required: false, help: 'The format data is in: csv (default), tagger, or json', regex: '^(csv|tagger|json)$' },
      { name: 'column', required: false, help: 'The numerical column number to use as the key', regex: '^[0-9]*$', ifField: 'format', ifValue: 'csv' },
      { name: 'arrayPath', required: false, help: "The path of where to find the array, if the json result isn't an array", ifField: 'format', ifValue: 'json' },
      { name: 'keyPath', required: true, help: 'The path of what field to use as the key', ifField: 'format', ifValue: 'json' },
      { name: 'url', required: true, help: 'The URL to load' },
      { name: 'urlScrapeRedirect', required: false, help: 'If set this is a redirect to match against the results of URL to find the url with the real data' },
      { name: 'reload', required: false, help: 'How often in minutes to refresh the file, or -1 (default) to never refresh it' },
      { name: 'headers', required: false, multiline: ';', help: 'List of headers to send in the URL request' }
    ]
  });

  const sections = api.getConfigSections().filter((e) => { return e.match(/^url:/); });
  sections.forEach((section) => {
    return new URLSource(api, section);
  });
};
// ----------------------------------------------------------------------------
