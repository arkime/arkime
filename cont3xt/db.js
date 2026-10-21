/******************************************************************************/
/* db.js  -- Cont3xt DB Interface
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const LinkGroup = require('./linkGroup');
const ArkimeUtil = require('../common/arkimeUtil');
const ArkimeConfig = require('../common/arkimeConfig');

const esSettings = { number_of_shards: 1, number_of_replicas: 0, auto_expand_replicas: '0-2' };
const keywordStrings = [{ string_template: { match_mapping_type: 'string', mapping: { type: 'keyword' } } }];

// Names match the layouts each backend used before collections
const collections = {
  linkGroups: {
    idType: 'hex',
    table: 'link_groups',
    es: { index: 'cont3xt_links', create: { settings: esSettings, mappings: { dynamic_templates: keywordStrings } } },
    lmdb: { db: 'linkGroups' },
    redis: { prefix: 'lg:' }
  },
  views: {
    idType: 'hex',
    es: {
      index: 'cont3xt_views',
      create: {
        settings: esSettings,
        mappings: {
          properties: {
            name: { type: 'keyword' },
            creator: { type: 'keyword' },
            viewRoles: { type: 'keyword' },
            editRoles: { type: 'keyword' },
            integrations: { type: 'keyword', index: false }
          }
        }
      }
    },
    lmdb: { db: 'views' },
    redis: { prefix: 'view:' }
  },
  audits: {
    idType: 'hex',
    es: {
      index: 'cont3xt_history',
      create: {
        settings: esSettings,
        mappings: {
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
          dynamic_templates: keywordStrings
        }
      }
    },
    sqlite: { columns: { issuedAt: 'INTEGER' } },
    lmdb: { db: 'audits' },
    redis: { prefix: 'audit:' }
  },
  overviews: {
    idType: 'hex',
    es: {
      index: 'cont3xt_overviews',
      create: {
        settings: esSettings,
        mappings: {
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
          dynamic_templates: keywordStrings
        }
      }
    },
    lmdb: { db: 'overviews' },
    redis: { prefix: 'overview:' }
  }
};

// Docs the creator owns or one of the roles can edit or view
function accessWhere (creator, roles, all) {
  if (all) { return null; }
  const or = [];
  if (creator !== undefined) { or.push({ creator }); }
  if (roles !== undefined) { or.push({ editRoles: { $in: roles } }, { viewRoles: { $in: roles } }); }
  return { $or: or };
}

class Db {
  static #linkGroups;
  static #views;
  static #audits;
  static #overviews;

  static async initialize (options) {
    if (ArkimeConfig.debug > 1) {
      console.log('Db.initialize', options);
    }

    const url = options.url ?? [options.node].flat()[0];
    const conn = ArkimeUtil.dbConnect(url, { ...options, section: 'cont3xt' });
    Db.#linkGroups = conn.collection('linkGroups', collections.linkGroups);
    Db.#views = conn.collection('views', collections.views);
    Db.#audits = conn.collection('audits', collections.audits);
    Db.#overviews = conn.collection('overviews', collections.overviews);

    try {
      await Promise.all([Db.#linkGroups.init(), Db.#views.init(), Db.#audits.init(), Db.#overviews.init()]);
    } catch (err) {
      console.log('ERROR - creating cont3xt tables', err);
      process.exit(1);
    }
  }

  static async #findMatching (coll, Cls, creator, roles, all) {
    try {
      const { data } = await coll.find({ where: accessWhere(creator, roles, all) });
      return data.map(({ id, doc }) => new Cls(Object.assign(doc, { _id: id })));
    } catch (err) {
      console.log(`ERROR - fetching ${coll.name}`, err);
      return [];
    }
  }

  static async #put (coll, id, doc) {
    if (doc._id) { delete doc._id; }
    return coll.put(id, doc);
  }

  static async #delete (coll, id) {
    return (await coll.delete(id)) ? {} : null;
  }

  /**
   * Get all the links that match the creator and set of roles
   */
  static async getMatchingLinkGroups (creator, roles, all) {
    return Db.#findMatching(Db.#linkGroups, LinkGroup, creator, roles, all);
  }

  /**
   * Put a single linkGroup
   */
  static async putLinkGroup (id, linkGroup) {
    return Db.#put(Db.#linkGroups, id, linkGroup);
  }

  /**
   * Get a single linkGroup
   */
  static async getLinkGroup (id) {
    return Db.#linkGroups.get(id);
  }

  /**
   * Delete a single linkGroup
   */
  static async deleteLinkGroup (id) {
    return Db.#delete(Db.#linkGroups, id);
  }

  /**
   * Get all the views that match the creator and set of roles
   */
  static async getMatchingViews (creator, roles, all) {
    return Db.#findMatching(Db.#views, View, creator, roles, all);
  }

  /**
   * Update a single view
   */
  static async putView (id, view) {
    return Db.#put(Db.#views, id, view);
  }

  /**
   * Get a single view
   */
  static async getView (id) {
    return Db.#views.get(id);
  }

  /**
   * Delete a single view
   */
  static async deleteView (id) {
    return Db.#delete(Db.#views, id);
  }

  /**
   * Update a single audit entry
   */
  static async putAudit (id, audit) {
    return Db.#audits.put(id, audit);
  }

  /**
   * Get all audit logs for a user within a date range (or everyone's, if roles contains cont3xtAdmin)
   */
  static async getMatchingAudits (userId, roles, reqQuery) {
    const { startMs, stopMs, searchTerm, page, itemsPerPage, sortBy, sortOrder } = reqQuery;

    const and = [];
    if (startMs != null && stopMs != null) {
      and.push({ issuedAt: { $gte: startMs, $lte: stopMs } });
    }
    if (ArkimeUtil.isString(searchTerm)) {
      and.push({ $or: ['indicator', 'iType', 'tags'].map(field => ({ [field]: { $contains: searchTerm } })) });
    }
    // normal users can only see their own history, but cont3xtAdmins can see everyone's!
    if (!(roles?.includes('cont3xtAdmin') && reqQuery.seeAll === 'true')) {
      and.push({ userId });
    }

    const safeSortBy = ['issuedAt', 'indicator', 'iType'].includes(sortBy) ? sortBy : 'issuedAt';
    const size = itemsPerPage || 1000;

    try {
      const { total, data } = await Db.#audits.find({
        where: { $and: and },
        sort: { [safeSortBy]: sortOrder === 'asc' ? 'asc' : 'desc' },
        from: ((page || 1) - 1) * size,
        size
      });
      return { total, audits: data.map(({ id, doc }) => new Audit(Object.assign(doc, { _id: id }))) };
    } catch (err) {
      console.log('ERROR - fetching audit log history', err);
      return { audits: [], total: 0 };
    }
  }

  /**
   * Get a single history audit log
   */
  static async getAudit (id) {
    return Db.#audits.get(id);
  }

  /**
   * Delete a single history audit log
   */
  static async deleteAudit (id) {
    return Db.#delete(Db.#audits, id);
  }

  /**
   * Delete all history audit logs created before expireMs
   * @returns number of deleted logs
   */
  static async deleteExpiredAudits (expireMs) {
    try {
      return await Db.#audits.deleteWhere({ issuedAt: { $lt: expireMs } });
    } catch (err) {
      return null;
    }
  }

  /**
   * Get all the overviews that match the creator and set of roles
   */
  static async getMatchingOverviews (creator, roles, all) {
    return Db.#findMatching(Db.#overviews, Overview, creator, roles, all);
  }

  /**
   * Put a single overview
   */
  static async putOverview (id, overview) {
    return Db.#put(Db.#overviews, id, overview);
  }

  /**
   * Get a single overview
   */
  static async getOverview (id) {
    return Db.#overviews.get(id);
  }

  /**
   * Delete a single overview
   */
  static async deleteOverview (id) {
    return Db.#delete(Db.#overviews, id);
  }

  /**
   * Clear all data from all tables (regression tests only)
   */
  static async clearAll () {
    await Promise.all([Db.#linkGroups.deleteAll(), Db.#views.deleteAll(), Db.#audits.deleteAll(), Db.#overviews.deleteAll()]);
  }
}

module.exports = Db;

const View = require('./view');
const Audit = require('./audit');
const Overview = require('./overview');
