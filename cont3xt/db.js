/******************************************************************************/
/* db.js  -- ES DB Interface
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
const LinkGroup = require('./linkGroup');
const ArkimeUtil = require('../common/arkimeUtil');
const cryptoLib = require('crypto');

class Db {
  static debug;

  static async initialize (options) {
    if (options.debug > 1) {
      console.log('Auth.initialize', options);
    }

    Db.debug = options.debug;

    if (!options.url) {
      Db.implementation = new DbESImplementation(options);
    } else if (options.url.startsWith('lmdb')) {
      Db.implementation = new DbLMDBImplementation(options);
    // } else if (options.url.startsWith('redis')) {
    //  Db.implementation = new DbRedisImplementation(options);
    } else {
      Db.implementation = new DbESImplementation(options);
    }
  };

  /**
   * Get all the links that match the creator and set of roles
   */
  static async getMatchingLinkGroups (creator, rolesField, roles) {
    return Db.implementation.getMatchingLinkGroups(creator, rolesField, roles);
  }

  /**
   * Put a single linkGroup
   */
  static async putLinkGroup (id, linkGroup) {
    if (linkGroup._id) { delete linkGroup._id; }
    return Db.implementation.putLinkGroup(id, linkGroup);
  }

  /**
   * Get a single linkGroup
   */
  static async getLinkGroup (id) {
    return Db.implementation.getLinkGroup(id);
  }

  /**
   * Delete a single linkGroup
   */
  static async deleteLinkGroup (id) {
    return Db.implementation.deleteLinkGroup(id);
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

    // Create the cont3xt_links index
    this.createIndex();
  };

  async createIndex () {
    try {
      await this.client.indices.create({
        index: 'cont3xt_links',
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

    // Update the cont3xt_links mapping
    await this.client.indices.putMapping({
      index: 'cont3xt_links',
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

  async getMatchingLinkGroups (creator, rolesField, roles) {
    const query = {
      size: 1000,
      query: {
        bool: {
          should: []
        }
      }
    };

    if (creator) {
      query.query.bool.should.push({
        term: {
          creator: creator
        }
      });
    }
    if (roles) {
      const obj = {};
      obj[rolesField] = roles;
      query.query.bool.should.push({
        terms: obj
      });
    }

    const results = await this.client.search({
      index: 'cont3xt_links',
      body: query,
      rest_total_hits_as_int: true
    });

    const hits = results.body.hits.hits;
    const linkGroups = [];
    for (let i = 0; i < hits.length; i++) {
      const linkGroup = new LinkGroup(hits[i]._source);
      linkGroup._id = hits[i]._id;
      linkGroups.push(linkGroup);
    }

    return linkGroups;
  }

  async putLinkGroup (id, linkGroup) {
    const results = await this.client.index({
      id: id,
      index: 'cont3xt_links',
      body: linkGroup,
      refresh: true
    });

    return results.body._id;
  }

  async getLinkGroup (id) {
    const results = await this.client.get({
      index: 'cont3xt_links',
      id: id
    });

    if (results?.body?._source) {
      return results.body._source;
    }
    return null;
  }

  async deleteLinkGroup (id) {
    const results = await this.client.delete({
      index: 'cont3xt_links',
      id: id,
      refresh: true
    });

    if (results.body) {
      return results.body;
    }
    return null;
  }
}
/******************************************************************************/
// LMDB Implementation of Users DB
/******************************************************************************/
class DbLMDBImplementation {
  store;

  constructor (options) {
    this.store = ArkimeUtil.createLMDBStore(options.url, 'Db');
  }

  /**
   * Get all the links that match the creator and set of roles
   */
  async getMatchingLinkGroups (creator, rolesField, roles) {
    const hits = [];
    this.store.getRange({})
      .filter(({ key, value }) => {
        if (creator !== undefined && creator === value.creator) { return true; }
        if (roles !== undefined) {
          if (value[rolesField] === undefined) { return false; }
          const match = roles.some(x => value[rolesField].includes(x));
          if (!match) { return false; }
        }
        return true;
      }).forEach(({ key, value }) => {
        hits.push(value);
      });

    const linkGroups = [];
    for (let i = 0; i < hits.length; i++) {
      const linkGroup = new LinkGroup(hits[i]._source);
      linkGroup._id = hits[i]._id;
      linkGroups.push(linkGroup);
    }

    return linkGroups;
  }

  async putLinkGroup (id, linkGroup) {
    if (id === null) {
      // Maybe should be a UUID?
      id = cryptoLib.randomBytes(16).toString('hex');
    }
    await this.store.put(id, linkGroup);
    return id;
  }

  async getLinkGroup (id) {
    return await this.store.get(id);
  }

  async deleteLinkGroup (id) {
    return this.store.remove(id);
  }
}

module.exports = Db;
