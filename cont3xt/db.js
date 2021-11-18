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

class Db {
  static debug;
  static client;

  static async initialize (options) {
    Db.debug = options.debug;

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

    Db.client = new Client(esOptions);

    // Create the cont3xt_links index
    try {
      await Db.client.indices.create({
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
    await Db.client.indices.putMapping({
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
  };

  /**
   * Get all the links that match the creator and set of roles
   */
  static async getMatchingLinkGroups (creator, rolesField, roles) {
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

    const results = await Db.client.search({
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

  /**
   * Put a single linkGroup
   */
  static async putLinkGroup (id, linkGroup) {
    if (linkGroup._id) { delete linkGroup._id; }

    const results = await Db.client.update({
      id: id,
      index: 'cont3xt_links',
      body: { doc: linkGroup },
      refresh: true
    });

    return results.body._id;
  }

  /**
   * Get a single linkGroup
   */
  static async getLinkGroup (id) {
    const results = await Db.client.get({
      index: 'cont3xt_links',
      id: id
    });

    if (results?.body?._source) {
      return results.body._source;
    }
    return null;
  }

  /**
   * Delete a single linkGroup
   */
  static async deleteLinkGroup (id) {
    const results = await Db.client.delete({
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

module.exports = Db;
