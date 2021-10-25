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
  static integrations = {
    all: [],
    ip: [],
    domain: [],
    phone: [],
    email: [],
    hash: [],
    url: []
  };

  static initialize (options) {
    options.integrationsPath = options.integrationsPath ?? path.join(__dirname, '/integrations/');

    console.log('path', options.integrationsPath);
    glob(options.integrationsPath + 'integration.*.js', (err, files) => {
      files.forEach((file) => {
        console.log('loading', file);
        require(file);
      });
    });
  }

  static register (iname, integration, options) {
    console.log('REGISTER', iname, options);
    options.name = iname;
    options.integration = integration;
    Integration.integrations.all.push(options);
    for (const i in options.itypes) {
      Integration.integrations[i].push(options);
    }
  }

  // TODO - Add caching
  // TODO - Redo with req/res and async
  static search (itype, value) {
    console.log('SEARCH', itype, value);
    const results = [];
    for (const i of Integration.integrations[itype]) {
      const result = i.integration[i.itypes[itype]](value);
      results.push(result);
    }
    return results;
  }

  static async apiSearch (req, res, next) {

  }
}

module.exports = Integration;

// Must be at bottom to avoid circular dependency
// const Db = require('./db.js');
