/******************************************************************************/
/* db.js  -- Agent DB Interface
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

const { Client } = require('@elastic/elasticsearch');

const fs = require('fs');
const ArkimeUtil = require('../common/arkimeUtil');

class Db {
  static debug;

  static async initialize (options) {
    if (options.debug > 1) {
      console.log('Agent.initialize', options);
    }

    Db.debug = options.debug;

    if (!options.url) {
      Db.implementation = new DbESImplementation(options);
    }
    /* } else if (options.url.startsWith('lmdb')) {
      Db.implementation = new DbLMDBImplementation(options);
    // } else if (options.url.startsWith('redis')) {
    //  Db.implementation = new DbRedisImplementation(options);
    } else {
      Db.implementation = new DbESImplementation(options);
    */
  };

  static async getMatchingConfig (config) {
    return Db.implementation.getMatchingConfig(config);
  }
  static async putConfig (config) {
    return Db.implementation.putConfig(config);
  }
  static async getConfig (id) {
    return Db.implementation.getConfig(id);
  }
  static async deleteConfig (id) {
    return Db.implementation.deleteConfig(id);
  }
  static async countConfig () {
    return Db.implementation.countConfig();
  }
}

/******************************************************************************/
// ES Implementation of Cont3xt DB
/******************************************************************************/
class DbESImplementation {
  client;

  constructor (options) {
    const esSSLOptions = { rejectUnauthorized: !options.insecure, ca: options.ca };
    if (options.clientKey) {
      esSSLOptions.key = fs.readFileSync(options.clientKey);
      esSSLOptions.cert = fs.readFileSync(options.clientCert);
      if (options.clientKeyPass) {
        esSSLOptions.passphrase = options.clientKeyPass;
      }
    }

    const esOptions = {
      node: options.node,
      maxRetries: 2,
      requestTimeout: (parseInt(options.requestTimeout) + 30) * 1000 || 330000,
      ssl: esSSLOptions
    };

    if (options.apiKey) {
      esOptions.auth = {
        apiKey: options.apiKey
      };
    } else if (options.basicAuth) {
      let basicAuth = options.basicAuth;
      if (!basicAuth.includes(':')) {
        basicAuth = Buffer.from(basicAuth, 'base64').toString();
      }
      basicAuth = basicAuth.split(':');
      esOptions.auth = {
        username: basicAuth[0],
        password: basicAuth[1]
      };
    }

    this.client = new Client(esOptions);

    // Create the agent_config index
    this.createConfigIndex();
  };

  async createConfigIndex () {
    try {
      await this.client.indices.create({
        index: 'agent_config',
        body: {
          settings: {
            number_of_shards: 1,
            number_of_replicas: 0,
            auto_expand_replicas: '0-2'
          }
        }
      });
    } catch (err) {
      // If already exists ignore error
      if (err.meta.body?.error?.type !== 'resource_already_exists_exception') {
        console.log(err);
        process.exit(0);
      }
    }

    await this.client.indices.putMapping({
      index: 'agent_config',
      body: {
        dynamic_templates: [
          {
            string_template: {
              match_mapping_type: 'string',
              mapping: {
                type: 'keyword'
              }
            }
          }
        ]
      }
    });
  }

  async getMatchingConfig (config) {
    const query = {
      size: 1000,
      query: {
        bool: {
          should: []
        }
      }
    };

    for (const element in config) {
      console.log('element', element);
      const obj = {
        term: {
        }
      };
      obj.term[element] = config[element];
      query.query.bool.should.push(obj);
    }
    console.log(JSON.stringify(query));

    const results = await this.client.search({
      index: 'agent_config',
      body: query,
      rest_total_hits_as_int: true
    });

    return results.body.hits.hits.sort((a,b) => (a._source.priority - b._source.priority)).map( a => a._source.rule );
  }

  async putConfig (config) {
    const results = await this.client.index({
      index: 'agent_config',
      body: config,
      refresh: true
    });

    return results.body._id;
  }

  async getConfig (id) {
    const results = await this.client.get({
      index: 'agent_config',
      id: id
    });
    if (results?.body?._source) {
      return results.body._source;
    }
    return null;
  }

  async deleteConfig (id) {
    const results = await this.client.delete({
      index: 'agent_config',
      id: id,
      refresh: true
    });

    if (results.body) {
      return results.body;
    }
    return null;
  }

  async countConfig () {
    const results = await this.client.count({
      index: 'agent_config'
    });

    return results.body.count;
  }
}

module.exports = Db;
