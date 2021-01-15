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

class ElasticsearchFileSource extends SimpleSource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { reload: true, formatSetting: 'json' });
    this.url = api.getConfig(section, 'url');
    if (this.url === undefined) {
      console.log(this.section, '- ERROR not loading since no url specified in config file');
      return;
    }
  }

  // ----------------------------------------------------------------------------
  simpleSourceLoad (cb) {
    request(this.url, {}, (err, response, body) => {
      if (err) {
        return cb(err);
      }
      return cb(null, JSON.stringify(JSON.parse(body)._source, null, 2));
    });
  }

  // ----------------------------------------------------------------------------
  getSourceRaw (cb) {
    request(this.url, {}, (err, response, body) => {
      if (err) {
        return cb(err);
      }
      return cb(null, JSON.stringify(JSON.parse(body)._source, null, 2));
    });
  }

  // ----------------------------------------------------------------------------
  putSourceRaw (file, cb) {
    request({ method: 'POST', url: this.url, headers: { 'Content-Type': 'application/json' }, body: file }, (err, response, body) => {
      this.load();
      return cb(err);
    });
  }
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('elasticsearchfile', {
    singleton: false,
    name: 'elasticsearchfile',
    description: 'Like the url source, use a single elasticsearch document as the file. The document can be periodically reloaded.',
    cacheable: false,
    editable: true,
    displayable: true,
    fields: [
      { name: 'url', password: true, required: false, help: 'Format is http[s]://[user:password@]host:port/<index>/_doc/<document>' },
      { name: 'type', required: true, help: 'The wise query type this source supports' },
      { name: 'tags', required: false, help: 'Comma separated list of tags to set for matches', regex: '^[-a-z0-9,]+' },
      { name: 'column', required: false, help: 'The numerical column number to use as the key', regex: '^[0-9]*$', ifField: 'format', ifValue: 'csv' },
      { name: 'arrayPath', required: true, help: 'The path of where to find the array' },
      { name: 'keyPath', required: true, help: 'The path of what field to use as the key inside each item' },
      { name: 'reload', required: false, help: 'How often in minutes to refresh the file, or -1 (default) to never refresh it' }
    ]
  });

  const sections = api.getConfigSections().filter((e) => { return e.match(/^elasticsearchfile:/); });
  sections.forEach((section) => {
    return new ElasticsearchFileSource(api, section);
  });
};
// ----------------------------------------------------------------------------
