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
const WISESource = require('./wiseSource.js');
const ini = require('iniparser');

class ValueActionsSource extends WISESource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { });

    if (section === 'right-click') {
      this.process(api.getConfigSection(section));
      return;
    }

    this.file = api.getConfig(section, 'file');

    if (this.file === undefined) {
      console.log(this.section, '- ERROR not loading', this.section, 'since no file specified in config file');
      return;
    }

    if (!fs.existsSync(this.file)) {
      console.log(this.section, '- ERROR not loading', this.section, 'since', this.file, "doesn't exist");
      return;
    }

    this.api.addSource(section, this, []);

    setImmediate(this.load.bind(this));

    // Watch file for changes, combine multiple changes into one, on move restart watch after a pause
    this.watchTimeout = null;
    const watchCb = (event, filename) => {
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
  load () {
    if (!fs.existsSync(this.file)) {
      console.log(this.section, '- ERROR not loading', this.section, 'since', this.file, "doesn't exist");
      return;
    }

    const config = ini.parseSync(this.file);
    const data = config.valueactions || config;

    this.process(data);
  };

  // ----------------------------------------------------------------------------
  process (data) {
    const keys = Object.keys(data);
    if (!keys) { return; }

    keys.forEach((key) => {
      const obj = {};
      data[key].split(';').forEach((element) => {
        const i = element.indexOf(':');
        if (i === -1) {
          return;
        }

        const parts = [element.slice(0, i), element.slice(i + 1)];
        if (parts[1] === 'true') {
          parts[1] = true;
        } else if (parts[1] === 'false') {
          parts[1] = false;
        }
        obj[parts[0]] = parts[1];
      });
      if (obj.fields) {
        obj.fields = obj.fields.split(',').map(item => item.trim());
      }
      if (obj.users) {
        const users = {};
        obj.users.split(',').map(item => item.trim()).forEach((item) => {
          users[item] = 1;
        });
        obj.users = users;
      }
      this.api.addValueAction(key, obj);
    });
  };

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
  api.addSourceConfigDef('valueactions', {
    singleton: false,
    name: 'valueactions',
    description: "This source monitors configured files for value actions to send to all the viewer instances that connect to this WISE Server. It isn't really a source in the true WISE sense, but makes it easy to edit.",
    cacheable: false,
    editable: true,
    types: [], // This is a fake source, no types
    fields: [
      { name: 'file', required: true, help: 'The file to load' }
    ]
  });

  const sections = api.getConfigSections().filter((e) => { return e.match(/^(right-click$|right-click:|valueactions:)/); });
  sections.forEach((section) => {
    return new ValueActionsSource(api, section);
  });
};
// ----------------------------------------------------------------------------
