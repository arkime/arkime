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

const fs = require('fs');
const SimpleSource = require('./simpleSource.js');

class FileSource extends SimpleSource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { dontCache: true });

    this.file = api.getConfig(section, 'file');

    if (this.file === undefined) {
      console.log(`${this.section} - ERROR not loading since no file specified in config file`);
      return;
    }

    if (!fs.existsSync(this.file)) {
      console.log(`${this.section} - ERROR not loading since ${this.file} doesn't exist`);
      return;
    }

    if (!this.initSimple()) {
      return;
    }

    // Watch file for changes, combine multiple changes into one, on move restart watch after a pause
    this.watchTimeout = null;
    let watchCb = (event, filename) => {
      clearTimeout(this.watchTimeout);
      if (event === 'rename') {
        this.watch.close();
        setTimeout(() => {
          this.load();
          this.watch = fs.watch(this.file, watchCb);
        }, 500);
      } else {
        this.watchTimeout = setTimeout(() => {
          this.watchTimeout = null;
          this.load();
        }, 2000);
      }
    };

    this.watch = fs.watch(this.file, watchCb);
  }

  // ----------------------------------------------------------------------------
  simpleSourceLoad (cb) {
    fs.readFile(this.file, cb);
  }
  // ----------------------------------------------------------------------------
  getSourceRaw (cb) {
    fs.readFile(this.file, (err, body) => {
      if (err) {
        return cb(err);
      }
      return cb(null, body);
    });
  }
  // ----------------------------------------------------------------------------
  putSourceRaw (body, cb) {
    fs.writeFile(this.file, body, (err) => {
      return cb(err);
    });
  }
}
  // ----------------------------------------------------------------------------
exports.initSource = function (api) {
  const sections = api.getConfigSections().filter((e) => { return e.match(/^file:/); });
  api.addSourceConfigDef('file', {
    singleton: false,
    name: 'file',
    description: 'Use a local file to load data into wise. The file is automatically reloaded if it changes.',
    cacheable: false,
    editable: true,
    fields: [
      { name: 'file', required: true, help: 'The path of the file to load' },
      { name: 'type', required: false, help: 'The wise query type this source supports' },
      { name: 'tags', required: false, help: 'Comma separated list of tags to set for matches', regex: '^[-a-z0-9,]+' },
      { name: 'format', required: false, help: 'The format data is in: csv (default), tagger, or json', regex: '^(csv|tagger|json)$' },
      { name: 'column', required: false, help: 'The numerical column number to use as the key', regex: '^[0-9]*$', ifField: 'format', ifValue: 'csv' },
      { name: 'keyColumn', required: false, help: 'The path of what field to use as the key', ifField: 'format', ifValue: 'json' }
    ]
  });

  sections.forEach((section) => {
    return new FileSource(api, section);
  });
};
