/******************************************************************************/
/* integration.js  -- Integrations wrapper
 *
 * Copyright Yahoo Inc.
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

const glob = require('glob');
const path = require('path');

class Integration {
  static debug = 0;
  static cache;
  static integrations = {
    all: [],
    ip: [],
    domain: [],
    phone: [],
    email: [],
    hash: [],
    url: [],
    text: []
  };

  static initialize (options) {
    Integration.debug = options.debug ?? 0;
    Integration.cache = options.cache;
    options.integrationsPath = options.integrationsPath ?? path.join(__dirname, '/integrations/');

    glob(options.integrationsPath + 'integration.*.js', (err, files) => {
      files.forEach((file) => {
        require(file);
      });
    });
  }

  static register (integration) {
    if (Integration.debug > 0) {
      console.log('REGISTER', integration.name);
    }
    integration.cacheable = integration.cacheable ?? true;
    // Min 1 minute, default 60 minutes
    integration.cacheTimeout = Math.max(60 * 1000, integration.cacheTimeout ?? 60 * 60 * 1000);

    integration.cacheTimeout = 5000; // ALW

    if (typeof (integration.itypes) !== 'object') {
      console.log('Missing .itypes object', integration);
      return;
    }

    if (typeof (integration.name) !== 'string') {
      console.log('Missing .name', integration);
      return;
    }

    Integration.integrations.all.push(integration);
    for (const itype in integration.itypes) {
      Integration.integrations[itype].push(integration);
    }
  }

  static classify (str) {
    if (str.match(/^(\d{3})[-. ]?(\d{3})[-. ]?(\d{4})$/)) {
      return 'phone';
    }

    // https://www.oreilly.com/library/view/regular-expressions-cookbook/9780596802837/ch07s16.html
    if (str.match(/^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$/)) {
      return 'ip'; // v4
    }

    // modified https://www.oreilly.com/library/view/regular-expressions-cookbook/9781449327453/ch08s17.html
    if (str.match(/^(?:[a-fA-F0-9]{1,4}:){7}[a-fA-F0-9]{1,4}$/)) {
      return 'ip'; // v6
    }

    // https://emailregex.com/
    // eslint-disable-next-line no-useless-escape
    if (str.match(/^(([^<>()\[\]\\.,;:\s@"]+(\.[^<>()\[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/)) {
      return 'email';
    }

    // https://urlregex.com/
    // eslint-disable-next-line no-useless-escape
    if (str.match(/((([A-Za-z]{3,9}:(?:\/\/)?)(?:[\-;:&=\+\$,\w]+@)?[A-Za-z0-9\.\-]+|(?:www\.|[\-;:&=\+\$,\w]+@)[A-Za-z0-9\.\-]+)((?:\/[\+~%\/\.\w\-_]*)?\??(?:[\-\+=&;%@\.\w_]*)#?(?:[\.\!\/\\\w]*))?)/)) {
      return 'domain';
    }

    if (str.match(/^[A-Fa-f0-9]{32}$/) || str.match(/^[A-Fa-f0-9]{40}$/) || str.match(/^[A-Fa-f0-9]{64}$/)) {
      return 'hash';
    }

    // Not sure
    return 'text';
  }

  static async apiSearch (req, res, next) {
    if (!req.params.query) {
      return res.send({ success: false, text: 'Missing query' });
    }
    const query = req.params.query.trim();

    const itype = Integration.classify(query);

    const integrations = Integration.integrations[itype];
    let sent = 0;
    const total = integrations.length;
    res.write(JSON.stringify({ success: true, itype: itype, sent: sent, total: total, text: 'more to follow' }));
    res.write('\n');

    for (const integration of integrations) {
      const cacheKey = `${integration.name}-${itype}-${query}`;

      if (Integration.cache && integration.cacheable) {
        const response = await Integration.cache.get(cacheKey);
        // TODO - Fix dup sending code for cache and not cache
        if (response && Date.now() - response._createTime < integration.cacheTimeout) {
          sent++;
          res.write(JSON.stringify({ sent: sent, total: total, response: response }));
          res.write('\n');
          if (sent === total) {
            res.end(JSON.stringify({ finished: true }));
          }
          continue;
        }
      }

      integration[integration.itypes[itype]](query)
        .then(response => {
          sent++;
          if (response) {
            response._createTime = Date.now();
            res.write(JSON.stringify({ sent: sent, total: total, response: response }));
            res.write('\n');
            if (Integration.cache && integration.cacheable) {
              Integration.cache.set(cacheKey, response);
            }
          }
          if (sent === total) {
            res.end(JSON.stringify({ finished: true }));
          }
        });
    }
  }
}

module.exports = Integration;

// Must be at bottom to avoid circular dependency
// const Db = require('./db.js');
