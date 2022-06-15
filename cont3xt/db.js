/******************************************************************************/
/* db.js  -- Cont3xt DB Interface
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
      console.log('Db.initialize', options);
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
  static async getMatchingLinkGroups (creator, roles) {
    return Db.implementation.getMatchingLinkGroups(creator, roles);
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

  /**
   * Get all the views that match the creator and set of roles
   */
  static async getMatchingViews (creator, roles) {
    return Db.implementation.getMatchingViews(creator, roles);
  }

  /**
   * Update a single view
   */
  static async putView (id, view) {
    if (view._id) { delete view._id; }
    return Db.implementation.putView(id, view);
  }

  /**
   * Get a single linkGroup
   */
  static async getView (id) {
    return Db.implementation.getView(id);
  }

  /**
   * Delete a single view
   */
  static async deleteView (id) {
    return Db.implementation.deleteView(id);
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
    this.createLinksIndex();
    // Create the cont3xt_views index
    this.createViewsIndex();
  };

  async createLinksIndex () {
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

  async createViewsIndex () {
    try {
      await this.client.indices.create({
        index: 'cont3xt_views',
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
      index: 'cont3xt_views',
      body: {
        properties: {
          name: { type: 'keyword' },
          creator: { type: 'keyword' },
          viewRoles: { type: 'keyword' },
          editRoles: { type: 'keyword' },
          integrations: { type: 'keyword', index: false }
        }
      }
    });
  }

  async getMatchingLinkGroups (creator, roles) {
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
          creator
        }
      });
    }
    if (roles) {
      query.query.bool.should.push({
        terms: {
          editRoles: roles
        }
      });

      query.query.bool.should.push({
        terms: {
          viewRoles: roles
        }
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
      id,
      index: 'cont3xt_links',
      body: linkGroup,
      refresh: true
    });

    return results.body._id;
  }

  async getLinkGroup (id) {
    const results = await this.client.get({
      index: 'cont3xt_links',
      id
    });

    if (results?.body?._source) {
      return results.body._source;
    }
    return null;
  }

  async deleteLinkGroup (id) {
    const results = await this.client.delete({
      index: 'cont3xt_links',
      id,
      refresh: true
    });

    if (results.body) {
      return results.body;
    }
    return null;
  }

  async getMatchingViews (creator, roles) {
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
          creator
        }
      });
    }

    if (roles) {
      query.query.bool.should.push({
        terms: {
          editRoles: roles
        }
      });
      query.query.bool.should.push({
        terms: {
          viewRoles: roles
        }
      });
    }

    try {
      const results = await this.client.search({
        body: query,
        index: 'cont3xt_views',
        rest_total_hits_as_int: true
      });

      const hits = results.body.hits.hits;
      const views = [];
      for (let i = 0; i < hits.length; i++) {
        const view = new View(hits[i]._source);
        view._id = hits[i]._id;
        views.push(view);
      }

      return views;
    } catch (err) {
      console.log('ERROR FETCHING VIEWS', err);
      return [];
    }
  }

  async putView (id, view) {
    const results = await this.client.index({
      id,
      body: view,
      refresh: true,
      index: 'cont3xt_views'
    });

    return results.body._id;
  }

  async getView (id) {
    const results = await this.client.get({
      id,
      index: 'cont3xt_views'
    });

    if (results?.body?._source) {
      return results.body._source;
    }

    return null;
  }

  async deleteView (id) {
    const results = await this.client.delete({
      id,
      refresh: true,
      index: 'cont3xt_views'
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
  viewStore;
  linkGroupStore;

  constructor (options) {
    this.store = ArkimeUtil.createLMDBStore(options.url, 'Db');
    this.linkGroupStore = this.store.openDB('linkGroups');
    this.viewStore = this.store.openDB('views');
  }

  /**
   * Get all the links that match the creator and set of roles
   */
  async getMatchingLinkGroups (creator, roles) {
    const hits = [];
    this.linkGroupStore.getRange({})
      .filter(({ key, value }) => {
        if (creator !== undefined && creator === value.creator) { return true; }
        if (roles !== undefined) {
          if (value.editRoles && roles.some(x => value.editRoles.includes(x))) { return true; }
          if (value.viewRoles && roles.some(x => value.viewRoles.includes(x))) { return true; }
        }
        return false;
      }).forEach(({ key, value }) => {
        value._id = key;
        hits.push(value);
      });

    const linkGroups = [];
    for (let i = 0; i < hits.length; i++) {
      const linkGroup = new LinkGroup(hits[i]);
      linkGroups.push(linkGroup);
    }

    return linkGroups;
  }

  async putLinkGroup (id, linkGroup) {
    if (id === null) {
      // Maybe should be a UUID?
      id = cryptoLib.randomBytes(16).toString('hex');
    }
    await this.linkGroupStore.put(id, linkGroup);
    return id;
  }

  async getLinkGroup (id) {
    return await this.linkGroupStore.get(id);
  }

  async deleteLinkGroup (id) {
    return this.linkGroupStore.remove(id);
  }

  /**
   * Get all the links that match the creator and set of roles
   */
  async getMatchingViews (creator, roles) {
    const hits = [];
    this.viewStore.getRange({})
      .filter(({ key, value }) => {
        if (creator !== undefined && creator === value.creator) { return true; }
        if (roles !== undefined) {
          if (value.editRoles && roles.some(x => value.editRoles.includes(x))) { return true; }
          if (value.viewRoles && roles.some(x => value.viewRoles.includes(x))) { return true; }
        }
        return false;
      }).forEach(({ key, value }) => {
        value._id = key;
        hits.push(value);
      });

    const views = [];
    for (let i = 0; i < hits.length; i++) {
      const view = new View(hits[i]);
      views.push(view);
    }

    return views;
  }

  async putView (id, view) {
    if (id === null) {
      // Maybe should be a UUID?
      id = cryptoLib.randomBytes(16).toString('hex');
    }
    await this.viewStore.put(id, view);
    return id;
  }

  async getView (id) {
    return await this.viewStore.get(id);
  }

  async deleteView (id) {
    return this.viewStore.remove(id);
  }
}

module.exports = Db;

const View = require('./view');
