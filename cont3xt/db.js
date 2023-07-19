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

    if (options.url?.startsWith('lmdb')) {
      Db.implementation = new DbLMDBImplementation(options);
    } else {
      Db.implementation = new DbESImplementation(options);
      await Db.implementation.initialize();
    }
  };

  /**
   * Get all the links that match the creator and set of roles
   */
  static async getMatchingLinkGroups (creator, roles, all) {
    return Db.implementation.getMatchingLinkGroups(creator, roles, all);
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
  static async getMatchingViews (creator, roles, all) {
    return Db.implementation.getMatchingViews(creator, roles, all);
  }

  /**
   * Update a single view
   */
  static async putView (id, view) {
    if (view._id) { delete view._id; }
    return Db.implementation.putView(id, view);
  }

  /**
   * Get a single view
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

  /**
   * Update a single audit entry
   */
  static async putAudit (id, audit) {
    return Db.implementation.putAudit(id, audit);
  }

  /**
   * Get all audit logs for a user within a date range (or everyone's, if roles contains cont3xtAdmin)
   */
  static async getMatchingAudits (userID, roles, reqQuery) {
    return Db.implementation.getMatchingAudits(userID, roles, reqQuery);
  }

  /**
   * Get a single history audit log
   */
  static async getAudit (id) {
    return Db.implementation.getAudit(id);
  }

  /**
   * Delete all history audit logs created before expireMs
   * @returns number of deleted logs
   */
  static async deleteExpiredAudits (expireMs) {
    return await Db.implementation.deleteExpiredAudits(expireMs);
  }

  /**
   * Get all the overviews that match the creator and set of roles
   */
  static async getMatchingOverviews (creator, roles, all) {
    return Db.implementation.getMatchingOverviews(creator, roles, all);
  }

  /**
   * Put a single overview
   */
  static async putOverview (id, overview) {
    if (overview._id) { delete overview._id; }
    return Db.implementation.putOverview(id, overview);
  }

  /**
   * Get a single overview
   */
  static async getOverview (id) {
    return Db.implementation.getOverview(id);
  }

  /**
   * Delete a single overview
   */
  static async deleteOverview (id) {
    return Db.implementation.deleteOverview(id);
  }
}

/******************************************************************************/
// ES Implementation of Cont3xt DB
/******************************************************************************/
class DbESImplementation {
  client;

  constructor (options) {
    const esSSLOptions = { rejectUnauthorized: !options.insecure };
    if (options.caTrustFile) { esSSLOptions.ca = ArkimeUtil.certificateFileToArray(options.caTrustFile); };
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
      basicAuth = ArkimeUtil.splitRemain(basicAuth, ':', 1);
      esOptions.auth = {
        username: basicAuth[0],
        password: basicAuth[1]
      };
    }

    this.client = new Client(esOptions);
  };

  async initialize () {
    // create all ES indices simultaneously
    await Promise.all([
      // Create the cont3xt_links index
      this.createLinksIndex(),
      // Create the cont3xt_views index
      this.createViewsIndex(),
      // Create the cont3xt_history index
      this.createHistoryIndex(),
      // Create the cont3xt_overviews index
      this.createOverviewIndex()
    ]);
  }

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

  async createHistoryIndex () {
    try {
      await this.client.indices.create({
        index: 'cont3xt_history',
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
      index: 'cont3xt_history',
      body: {
        properties: {
          issuedAt: { type: 'date' },
          took: { type: 'long' },
          resultCount: { type: 'long' },
          userId: { type: 'keyword' },
          iType: { type: 'keyword' },
          indicator: { type: 'keyword' },
          tags: { type: 'keyword' },
          viewId: { type: 'keyword' }
        },
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

  async createOverviewIndex () {
    try {
      await this.client.indices.create({
        index: 'cont3xt_overviews',
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
      index: 'cont3xt_overviews',
      body: {
        properties: {
          creator: { type: 'keyword' },
          name: { type: 'keyword' },
          title: { type: 'keyword' },
          iType: { type: 'keyword' },
          viewRoles: { type: 'keyword' },
          editRoles: { type: 'keyword' },
          fields: {
            properties: {
              from: { type: 'keyword' },
              type: { type: 'keyword' },
              field: { type: 'keyword' },
              custom: { type: 'keyword' }
            }
          }
        },
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

  async getMatchingLinkGroups (creator, roles, all) {
    const query = {
      size: 1000,
      query: {
        bool: {
          should: []
        }
      }
    };

    if (!all) {
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
    try {
      const results = await this.client.get({
        index: 'cont3xt_links',
        id
      });

      if (results?.body?._source) {
        return results.body._source;
      }
    } catch (err) {}
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

  async getMatchingViews (creator, roles, all) {
    const query = {
      size: 1000,
      query: {
        bool: {
          should: []
        }
      }
    };

    if (!all) {
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
    try {
      const results = await this.client.get({
        id,
        index: 'cont3xt_views'
      });

      if (results?.body?._source) {
        return results.body._source;
      }
    } catch (err) {}

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

  /* Audit Log ---------------------------------------- */
  async putAudit (id, audit) {
    const results = await this.client.index({
      id,
      body: audit,
      refresh: true,
      index: 'cont3xt_history'
    });

    return results.body._id;
  }

  async deleteAudit (id) {
    const results = await this.client.delete({
      index: 'cont3xt_history',
      id,
      refresh: true
    });

    if (results.body) {
      return results.body;
    }
  }

  async getAudit (id) {
    try {
      const results = await this.client.get({
        id,
        index: 'cont3xt_history'
      });

      if (results?.body?._source) {
        return results.body._source;
      }
    } catch (err) {}

    return null;
  }

  async deleteExpiredAudits (expireMs) {
    const query = {
      size: 1000,
      query: {
        range: {
          issuedAt: {
            lt: expireMs
          }
        }
      }
    };

    try {
      const results = await this.client.delete_by_query({
        body: query,
        index: 'cont3xt_history'
      });
      return results.body.deleted;
    } catch (err) {
      return null;
    }
  }

  async getMatchingAudits (userId, roles, reqQuery) {
    const { startMs, stopMs, searchTerm } = reqQuery;
    const filter = [];
    const query = {
      size: 1000,
      query: {
        bool: { filter }
      }
    };

    if (startMs != null && stopMs != null) {
      filter.push({ // restricts logs to those between certain dates
        range: {
          issuedAt: {
            gte: startMs,
            lte: stopMs
          }
        }
      });
    }

    if (searchTerm != null && typeof searchTerm === 'string') {
      filter.push({ // apply search term
        query_string: {
          query: `*${searchTerm}*`,
          fields: ['indicator', 'iType', 'tags']
        }
      });
    }

    // normal users can only see their own history, but cont3xtAdmins can see everyone's!
    if (!roles.includes('cont3xtAdmin') || reqQuery.seeAll !== 'true') {
      filter.push({ term: { userId } });
    }

    try {
      const results = await this.client.search({
        body: query,
        index: 'cont3xt_history',
        rest_total_hits_as_int: true
      });

      const hits = results.body.hits.hits;

      return hits.map(({ _id, _source }) => {
        return new Audit(Object.assign(_source, { _id }));
      });
    } catch (err) {
      console.log('ERROR - fetching audit log history', err);
      return [];
    }
  }

  /* Overviews ---------------------------------------- */
  async getMatchingOverviews (creator, roles, all) {
    const query = {
      size: 1000,
      query: {
        bool: {
          should: []
        }
      }
    };

    if (!all) {
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
    }

    const results = await this.client.search({
      index: 'cont3xt_overviews',
      body: query,
      rest_total_hits_as_int: true
    });

    const hits = results.body.hits.hits;
    const overviews = [];
    for (let i = 0; i < hits.length; i++) {
      const overview = new Overview(hits[i]._source);
      overview._id = hits[i]._id;
      overviews.push(overview);
    }

    return overviews;
  }

  async putOverview (id, overview) {
    const results = await this.client.index({
      id,
      index: 'cont3xt_overviews',
      body: overview,
      refresh: true
    });

    return results.body._id;
  }

  async getOverview (id) {
    try {
      const results = await this.client.get({
        index: 'cont3xt_overviews',
        id
      });

      if (results?.body?._source) {
        return results.body._source;
      }
    } catch (err) {}
    return null;
  }

  async deleteOverview (id) {
    const results = await this.client.delete({
      index: 'cont3xt_overviews',
      id,
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
  viewStore;
  linkGroupStore;
  auditStore;
  overviewStore;

  constructor (options) {
    this.store = ArkimeUtil.createLMDBStore(options.url, 'Db');
    this.linkGroupStore = this.store.openDB('linkGroups');
    this.viewStore = this.store.openDB('views');
    this.auditStore = this.store.openDB('audits');
    this.overviewStore = this.store.openDB('overviews');
  }

  /**
   * Get all the links that match the creator and set of roles
   */
  async getMatchingLinkGroups (creator, roles, all) {
    const hits = [];
    this.linkGroupStore.getRange({})
      .filter(({ key, value }) => {
        if (all) { return true; }
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
   * Get all the views that match the creator and set of roles
   */
  async getMatchingViews (creator, roles, all) {
    const hits = [];
    this.viewStore.getRange({})
      .filter(({ key, value }) => {
        if (all) { return true; }
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

  /* Audit Log ---------------------------------------- */
  async putAudit (id, audit) {
    if (id === null) {
      // Maybe should be a UUID?
      id = cryptoLib.randomBytes(16).toString('hex');
    }
    await this.auditStore.put(id, audit);
    return id;
  }

  async deleteAudit (id) {
    return await this.auditStore.remove(id);
  }

  async getMatchingAudits (userId, roles, reqQuery) {
    const { startMs, stopMs, searchTerm } = reqQuery;
    return [...this.auditStore.getRange({})
      .filter(({ _, value }) => {
        // remove entries outside the dateRange, if there is one
        if ((startMs != null && stopMs != null) && (value.issuedAt < startMs || value.issuedAt > stopMs)) {
          return false;
        }

        // apply search term
        if (searchTerm != null) {
          const containsTerm = (
            value.indicator.includes(searchTerm) ||
            value.iType.includes(searchTerm) ||
            value.tags.some(tag => tag.includes(searchTerm))
          );
          if (!containsTerm) { return false; }
        }

        // cont3xtAdmins can see anyone's logs
        // non-admin accounts can only see their own logs
        return ((roles?.includes('cont3xtAdmin') && reqQuery.seeAll === 'true') || userId === value.userId);
      }).map(({ key, value }) => new Audit( // create Audit objs with _id
        Object.assign(value, { _id: key }))
      )];
  }

  async getAudit (id) {
    return await this.auditStore.get(id);
  }

  async deleteExpiredAudits (expireMs) {
    const expiredLogIds = [...this.auditStore.getRange({}).filter(({ value }) => value.issuedAt < expireMs).map(({ key }) => key)];

    for (const expiredLogId of expiredLogIds) {
      await this.deleteAudit(expiredLogId);
    }
    return expiredLogIds.length;
  }

  /* Overviews ---------------------------------------- */
  /**
   * Get all the links that match the creator and set of roles
   */
  async getMatchingOverviews (creator, roles, all) {
    const hits = [];
    this.overviewStore.getRange({})
      .filter(({ value }) => {
        if (all) { return true; }
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

    const overviews = [];
    for (let i = 0; i < hits.length; i++) {
      const overview = new Overview(hits[i]);
      overviews.push(overview);
    }

    return overviews;
  }

  async putOverview (id, overview) {
    if (id === null) {
      // Maybe should be a UUID?
      id = cryptoLib.randomBytes(16).toString('hex');
    }
    await this.overviewStore.put(id, overview);
    return id;
  }

  async getOverview (id) {
    return await this.overviewStore.get(id);
  }

  async deleteOverview (id) {
    return this.overviewStore.remove(id);
  }
}

module.exports = Db;

const View = require('./view');
const Audit = require('./audit');
const Overview = require('./overview');
