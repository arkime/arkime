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

const WISESource = require('./wiseSource.js');
const ini = require('iniparser');

class ValueActionsRedisSource extends WISESource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { });

    if (api.getConfig(section, 'redisURL') === undefined) {
      console.log(this.section, '- ERROR not loading', this.section, 'since no redisURL specified in config file');
      return;
    }

    this.key = api.getConfig(section, 'key');
    if (this.key === undefined) {
      console.log(this.section, '- ERROR not loading', this.section, 'since no key specified in config file');
      return;
    }

    this.api.addSource(section, this, []);

    this.client = api.createRedisClient(api.getConfig(section, 'redisURL'), section);

    setImmediate(this.load.bind(this));
    setInterval(this.load.bind(this), 5 * 1000 * 60); // Eventually could just monitor
  }

  // ----------------------------------------------------------------------------
  load () {
    if (this.client && this.key) {
      this.client.get(this.key, (err, data) => {
        if (err) {
          console.log(this.section, '- ERROR', err);
          return;
        }
        if (data === null) { data = ''; }
        this.process(ini.parseString(data));
      });
    }
  };

  // ----------------------------------------------------------------------------
  process (data) {
    if (!data) { return; }

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
    this.client.get(this.key, cb);
  }

  // ----------------------------------------------------------------------------
  putSourceRaw (file, cb) {
    this.client.set(this.key, file, (err) => {
      this.load();
      cb(err);
    });
  }
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('valueactionsredis', {
    singleton: false,
    name: 'valueactionsredis',
    description: "This source monitors configured files for value actions to send to all the viewer instances that connect to this WISE Server. It isn't really a source in the true WISE sense, but makes it easy to edit.",
    link: 'https://arkime.com/wise#valueactionsredis',
    cacheable: false,
    editable: true,
    types: [], // This is a fake source, no types
    fields: [
      { name: 'redisURL', password: true, required: true, help: 'Format is redis://[:password@]host:port/db-number, redis-sentinel://[[sentinelPassword]:[password]@]host[:port]/redis-name/db-number, or redis-cluster://[:password@]host:port/db-number' },
      { name: 'key', required: true, help: 'When using redis the document key to fetch the value actions from' }
    ]
  });

  const sections = api.getConfigSections().filter((e) => { return e.match(/^(valueactionsredis:)/); });
  sections.forEach((section) => {
    return new ValueActionsRedisSource(api, section);
  });
};
// ----------------------------------------------------------------------------
