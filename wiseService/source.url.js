/******************************************************************************/
/*
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

const SimpleSource = require('./simpleSource.js');
const request = require('request');

class URLSource extends SimpleSource {
// ----------------------------------------------------------------------------
  constructor (api, section) {
    super({ api: api, section: section, dontCache: true });
    this.url = api.getConfig(section, 'url');
    this.reload = +api.getConfig(section, 'reload', -1);
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

    if (!this.initSimple()) {
      return;
    }

    setImmediate(this.load.bind(this));

    // Reload url every so often
    if (this.reload > 0) {
      setInterval(this.load.bind(this), this.reload * 1000 * 60);
    }
  }
// ----------------------------------------------------------------------------
  simpleSourceLoad (setFunc, cb) {
    request(this.url, { headers: this.headers }, (error, response, body) => {
      if (!error && response.statusCode === 200) {
        this.parse(body, setFunc, cb);
      } else {
        cb(error);
      }
    });
  }
}
// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('url', {
    singleton: false,
    name: 'url',
    description: 'Use a web url to load data into wise. The url can be periodically reloaded.',
    cacheable: false,
    fields: [
      { name: 'type', required: true, help: 'The wise query type this source supports' },
      { name: 'tags', required: false, help: 'Comma separated list of tags to set for matches', regex: '^[-a-z0-9,]+' },
      { name: 'format', required: false, help: 'The format data is in: csv (default), tagger, or json', regex: '^(csv|tagger|json)$' },
      { name: 'column', required: false, help: 'The numerical column number to use as the key', regex: '^[0-9]*$', ifField: 'format', ifValue: 'csv' },
      { name: 'keyColumn', required: false, help: 'The path of what field to use as the key', ifField: 'format', ifValue: 'json' },
      { name: 'url', required: true, help: 'The URL to load' },
      { name: 'reload', required: false, help: 'How often in minutes to refresh the file, or -1 (default) to never refresh it' },
      { name: 'headers', required: true, help: 'Semicolon separated list of headers to send in the URL request' }
    ]
  });

  const sections = api.getConfigSections().filter((e) => { return e.match(/^url:/); });
  sections.forEach((section) => {
    return new URLSource(api, section);
  });
};
// ----------------------------------------------------------------------------
