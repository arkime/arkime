/******************************************************************************/
/*
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const fs = require('fs');
const SimpleSource = require('./simpleSource.js');

class FileSource extends SimpleSource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { });

    this.file = api.getConfig(section, 'file');

    if (this.file === undefined) {
      console.log(`${this.section} - ERROR not loading since no file specified in config file`);
      return;
    }

    if (!fs.existsSync(this.file)) {
      console.log(`${this.section} - ERROR not loading since ${this.file} doesn't exist`);
      delete this.file;
      return;
    }

    // Watch file for changes, combine multiple changes into one, on move restart watch after a pause
    this.watchTimeout = null;
    const watchCb = (e, filename) => {
      clearTimeout(this.watchTimeout);
      if (e === 'rename') {
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
    if (this.file) {
      fs.readFile(this.file, cb);
    }
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
      this.load();
      return cb(err);
    });
  }
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('file', {
    singleton: false,
    name: 'file',
    description: 'Use a local file to load data into wise. The file is automatically reloaded if it changes.',
    link: 'https://arkime.com/wise#file',
    cacheable: false,
    editable: true,
    displayable: true,
    fields: [
      { name: 'file', required: true, help: 'The path of the file to load' },
      { name: 'type', required: false, help: 'The wise query type this source supports' },
      { name: 'tags', required: false, help: 'Comma separated list of tags to set for matches', regex: '^[-a-z0-9,]+' },
      { name: 'format', required: false, help: 'The format data is in: csv (default), tagger, jsonl, or json', regex: '^(csv|tagger|jsonl|json)$' },
      { name: 'column', required: false, help: 'The numerical column number to use as the key', regex: '^[0-9]*$', ifField: 'format', ifValue: 'csv' },
      { name: 'arrayPath', required: false, help: "The path of where to find the array, if the json result isn't an array", ifField: 'format', ifValue: ['jsonl', 'json'] },
      { name: 'keyPath', required: false, help: 'The path of what field to use as the key', ifField: 'format', ifValue: ['jsonl', 'json'] }
    ]
  });

  const sections = api.getConfigSections().filter((e) => { return e.match(/^file:/); });
  sections.forEach((section) => {
    return new FileSource(api, section);
  });
};
