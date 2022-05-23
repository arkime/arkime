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
const axios = require('axios');
const ArkimeUtil = require('../common/arkimeUtil');

class FieldActionsSource extends WISESource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { });

    if (section === 'right-click') {
      this.process(api.getConfigSection(section));
      return;
    }

    this.url = api.getConfig(section, 'url', api.getConfig(section, 'file'));

    if (this.url === undefined) {
      console.log(this.section, '- ERROR not loading', this.section, 'since no url specified in config file');
      return;
    }

    if (this.url.startsWith('file:///')) {
      this.url = this.url.substring(7);
    }

    if (this.url[0] === '/' || this.url.startsWith('./') || this.url.startsWith('../')) {
      if (!fs.existsSync(this.url)) {
        console.log(this.section, '- ERROR not loading', this.section, 'since', this.url, "doesn't exist");
        return;
      }
      this.load = this.loadFile;
      this.getSourceRaw = this.getSourceRawFile;
      this.putSourceRaw = this.putSourceRawFile;
    } else if (this.url.startsWith('elasticsearch://') || this.url.startsWith('elasticsearchs://')) {
      this.url = this.url.replace('elasticsearch', 'http');
      if (!this.url.includes('/_doc/')) {
        throw new Error('Missing _doc in url, should be format elasticsearch://user:pass@host:port/INDEX/_doc/DOC');
      }
      this.load = this.loadES;
      this.getSourceRaw = this.getSourceRawES;
      this.putSourceRaw = this.putSourceRawES;
    } else if (this.url.startsWith('redis')) {
      this.load = this.loadRedis;
      this.getSourceRaw = this.getSourceRawRedis;
      this.putSourceRaw = this.putSourceRawRedis;
      const redisParts = this.url.split('/');
      if (redisParts.length !== 5) {
        throw new Error(`Invalid redis url - ${redisParts[0]}//[:pass@]redishost[:redisport]/redisDbNum/key`);
      }
      this.key = redisParts.pop();
      this.client = ArkimeUtil.createRedisClient(redisParts.join('/'), section);
    } else {
      console.log(this.section, '- ERROR not loading', this.section, 'don\'t know how to open', this.url);
      return;
    }

    this.api.addSource(section, this, []);

    setImmediate(this.load.bind(this));

    if (this.url[0] === '/' || this.url.startsWith('../')) {
      this.watchFile();
    } else {
      setInterval(this.load.bind(this), 5 * 1000 * 60);
    }
  }

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
      if (obj.notUsers) {
        const users = {};
        obj.notUsers.split(',').map(item => item.trim()).forEach((item) => {
          users[item] = 1;
        });
        obj.notUsers = users;
      }
      this.api.addFieldAction(key, obj);
    });
  };

  // ----------------------------------------------------------------------------
  watchFile () {
    // Watch file for changes, combine multiple changes into one, on move restart watch after a pause
    this.watchTimeout = null;
    const watchCb = (e, filename) => {
      clearTimeout(this.watchTimeout);
      if (e === 'rename') {
        this.watch.close();
        setTimeout(() => {
          this.load();
          this.watch = fs.watch(this.url, watchCb);
        }, 500);
      } else {
        this.watchTimeout = setTimeout(() => {
          this.watchTimeout = null;
          this.load();
        }, 2000);
      }
    };
    this.watch = fs.watch(this.url, watchCb);
  }

  // ----------------------------------------------------------------------------
  loadFile () {
    if (!fs.existsSync(this.url)) {
      console.log(this.section, '- ERROR not loading', this.section, 'since', this.url, "doesn't exist");
      return;
    }

    const config = ini.parseSync(this.url);
    const data = config.fieldactions || config;

    this.process(data);
  };

  // ----------------------------------------------------------------------------
  getSourceRawFile (cb) {
    fs.readFile(this.url, (err, body) => {
      if (err) {
        return cb(err);
      }
      return cb(null, body);
    });
  }

  // ----------------------------------------------------------------------------
  putSourceRawFile (body, cb) {
    fs.writeFile(this.url, body, (err) => {
      this.process(ini.parseString(body));
      return cb(err);
    });
  }

  // ----------------------------------------------------------------------------
  loadRedis () {
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
  getSourceRawRedis (cb) {
    this.client.get(this.key, cb);
  }

  // ----------------------------------------------------------------------------
  putSourceRawRedis (file, cb) {
    this.client.set(this.key, file, (err) => {
      this.load();
      cb(err);
    });
  }

  // ----------------------------------------------------------------------------
  loadES () {
    axios.get(this.url)
      .then((response) => {
        return this.process(ini.parseString(response.data._source.ini || ''));
      })
      .catch((error) => {
        if (error.response && error.response.status === 404) {
          return this.process({});
        }
        console.log(this.section, '- ERROR', error);
      });
  };

  // ----------------------------------------------------------------------------
  getSourceRawES (cb) {
    axios.get(this.url)
      .then((response) => {
        cb(null, response.data._source.ini || '');
      })
      .catch((error) => {
        if (error.response && error.response.status === 404) {
          return cb(null, '');
        }
        return cb(error);
      });
  }

  // ----------------------------------------------------------------------------
  putSourceRawES (file, cb) {
    axios.post(this.url, JSON.stringify({ ini: file }), { headers: { 'Content-Type': 'application/json' } })
      .then((response) => {
        this.process(ini.parseString(file));
        cb(null);
      })
      .catch((error) => {
        cb(error);
      });
  }
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('fieldactions', {
    singleton: false,
    name: 'fieldactions',
    description: "This source monitors configured files, redis or elasticsearch for field actions to send to all the viewer instances that connect to this WISE Server. It isn't really a source in the true WISE sense, but makes it easy to edit.",
    link: 'https://arkime.com/wise#fieldactions',
    cacheable: false,
    editable: true,
    types: [], // This is a fake source, no types
    format: 'valueactions', // Which vueapp editor to use
    fields: [
      { name: 'url', required: true, help: 'The file to load, can be a file path, redis url (Format is redis://[:password@]host:port/db-number/key, redis-sentinel://[[sentinelPassword]:[password]@]host[:port]/redis-name/db-number/key, or redis-cluster://[:password@]host:port/db-number/key), or elasticsearch url (elasticsearch://host:9200/INDEX/_doc/DOCNAME)' }
    ]
  });

  const sections = api.getConfigSections().filter((e) => { return e.match(/^(right-click$|right-click:|fieldactions:)/); });
  sections.forEach((section) => {
    return new FieldActionsSource(api, section);
  });
};
// ----------------------------------------------------------------------------
