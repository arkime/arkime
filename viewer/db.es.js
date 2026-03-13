/******************************************************************************/
/* db.es.js -- Elasticsearch implementation of the viewer DB backend
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';

class DbESImpl {
  #client;
  #prefix;

  constructor (client, prefix) {
    this.#client = client;
    this.#prefix = prefix;
  }

  // --------------------------------------------------------------------------
  // VIEWS
  // --------------------------------------------------------------------------
  async searchViews (params) {
    const query = this.#buildViewsQuery(params);
    query.sort = { [params.sortField || 'name']: { order: params.sortOrder || 'asc' } };
    query.from = params.from || 0;
    query.size = params.size || 50;

    const { body: { hits } } = await this.#client.search({
      index: `${this.#prefix}views`,
      body: query,
      rest_total_hits_as_int: true
    });

    return {
      data: hits.hits.map(h => ({ id: h._id, source: h._source })),
      total: hits.total
    };
  }

  async numberOfViews (params) {
    const query = this.#buildViewsQuery(params);
    const { body: { count } } = await this.#client.count({
      index: `${this.#prefix}views`,
      body: query
    });
    return count;
  }

  async getView (id) {
    const { body } = await this.#client.get({
      index: `${this.#prefix}views`,
      id
    });
    return body._source;
  }

  async getViewByIdOrName (idOrName, user, roles) {
    const query = {
      size: 1,
      query: {
        bool: {
          filter: [{
            bool: {
              must: [{
                bool: {
                  should: [
                    { term: { _id: idOrName } },
                    { term: { name: idOrName } }
                  ]
                }
              }, {
                bool: {
                  should: [
                    { terms: { roles } },
                    { term: { users: user } },
                    { term: { user } }
                  ]
                }
              }]
            }
          }]
        }
      }
    };

    const { body: { hits: { hits } } } = await this.#client.search({
      index: `${this.#prefix}views`,
      body: query,
      rest_total_hits_as_int: true
    });

    if (hits.length === 0) { return null; }
    return hits[0]._source;
  }

  async createView (doc) {
    const { body } = await this.#client.index({
      index: `${this.#prefix}views`,
      body: doc,
      refresh: 'wait_for',
      timeout: '10m'
    });
    return body._id;
  }

  async deleteView (id) {
    await this.#client.delete({
      index: `${this.#prefix}views`,
      id,
      refresh: true
    });
  }

  async deleteAllViews () {
    await this.#client.deleteByQuery({
      index: `${this.#prefix}views`,
      body: { query: { match_all: {} } },
      conflicts: 'proceed',
      refresh: true
    });
  }

  async setView (id, doc) {
    await this.#client.index({
      index: `${this.#prefix}views`,
      body: doc,
      id,
      refresh: true,
      timeout: '10m'
    });
  }

  // --------------------------------------------------------------------------
  // SHAREABLES
  // --------------------------------------------------------------------------
  async searchShareables (params) {
    const query = this.#buildShareablesQuery(params);
    query.sort = { name: { order: 'asc' } };
    query.from = params.from || 0;
    query.size = params.size || 50;

    const { body: { hits } } = await this.#client.search({
      index: `${this.#prefix}shareables`,
      body: query,
      rest_total_hits_as_int: true
    });

    return {
      data: hits.hits.map(h => ({ id: h._id, source: h._source })),
      total: hits.total
    };
  }

  async numberOfShareables (params) {
    const query = this.#buildShareablesQuery(params);
    const { body: { count } } = await this.#client.count({
      index: `${this.#prefix}shareables`,
      body: query
    });
    return count;
  }

  async getShareable (id) {
    const { body } = await this.#client.get({
      index: `${this.#prefix}shareables`,
      id
    });
    return body._source;
  }

  async createShareable (doc) {
    const { body } = await this.#client.index({
      index: `${this.#prefix}shareables`,
      body: doc,
      refresh: 'wait_for',
      timeout: '10m'
    });
    return body._id;
  }

  async deleteShareable (id) {
    await this.#client.delete({
      index: `${this.#prefix}shareables`,
      id,
      refresh: true
    });
  }

  async deleteAllShareables () {
    await this.#client.deleteByQuery({
      index: `${this.#prefix}shareables`,
      body: { query: { match_all: {} } },
      conflicts: 'proceed',
      refresh: true
    });
  }

  async setShareable (id, doc) {
    await this.#client.index({
      index: `${this.#prefix}shareables`,
      body: doc,
      id,
      refresh: true,
      timeout: '10m'
    });
  }

  // --------------------------------------------------------------------------
  // SHORTCUTS
  // --------------------------------------------------------------------------
  async searchShortcuts (params) {
    const query = this.#buildShortcutsQuery(params);
    query.sort = { [params.sortField || 'name']: { order: params.sortOrder || 'asc' } };
    query.from = params.from || 0;
    query.size = params.size || 50;

    const { body: { hits } } = await this.#client.search({
      index: `${this.#prefix}lookups`,
      body: query,
      rest_total_hits_as_int: true
    });

    return {
      data: hits.hits.map(h => ({ id: h._id, source: h._source })),
      total: hits.total
    };
  }

  async numberOfShortcuts (params) {
    const query = this.#buildShortcutsQuery(params);
    const { body: { count } } = await this.#client.count({
      index: `${this.#prefix}lookups`,
      body: query
    });
    return count;
  }

  async getShortcut (id) {
    const { body } = await this.#client.get({
      index: `${this.#prefix}lookups`,
      id
    });
    return body._source;
  }

  async createShortcut (doc) {
    const { body } = await this.#client.index({
      index: `${this.#prefix}lookups`,
      body: doc,
      refresh: 'wait_for',
      timeout: '10m'
    });
    return body._id;
  }

  async deleteShortcut (id) {
    await this.#client.delete({
      index: `${this.#prefix}lookups`,
      id,
      refresh: true
    });
  }

  async setShortcut (id, doc) {
    await this.#client.index({
      index: `${this.#prefix}lookups`,
      body: doc,
      id,
      refresh: true,
      timeout: '10m'
    });
  }

  async deleteAllShortcuts () {
    await this.#client.deleteByQuery({
      index: `${this.#prefix}lookups`,
      body: { query: { match_all: {} } },
      conflicts: 'proceed',
      refresh: true
    });
  }

  // Returns all shortcuts with id, source, and version for sync purposes
  async getAllShortcuts () {
    const { body: { hits } } = await this.#client.search({
      index: `${this.#prefix}lookups`,
      body: { size: 10000 },
      rest_total_hits_as_int: true,
      version: true
    });
    return hits.hits.map(h => ({ id: h._id, source: h._source, version: h._version }));
  }

  async getShortcutsVersion () {
    const { body: doc } = await this.#client.indices.getMapping({
      index: `${this.#prefix}lookups`
    });
    return doc[Object.keys(doc)[0]]?.mappings?._meta?.version || 0;
  }

  async setShortcutsVersion () {
    const version = await this.getShortcutsVersion();
    return this.#client.indices.putMapping({
      index: `${this.#prefix}lookups`,
      body: { _meta: { version: version + 1, initSync: true } }
    });
  }

  // --------------------------------------------------------------------------
  // Flush/Refresh for backend indices
  // --------------------------------------------------------------------------
  async flush (index, cluster) {
    return this.#client.indices.flush({ index: `${this.#prefix}${index}`, cluster });
  }

  async refresh (index, cluster) {
    return this.#client.indices.refresh({ index: `${this.#prefix}${index}`, cluster });
  }

  // --------------------------------------------------------------------------
  // PRIVATE HELPERS
  // --------------------------------------------------------------------------
  #buildViewsQuery (params) {
    const filter = [];

    if (!params.all) {
      filter.push({
        bool: {
          should: [
            { terms: { roles: params.roles || [] } },
            { terms: { editRoles: params.roles || [] } },
            { term: { users: params.user } },
            { term: { user: params.user } }
          ]
        }
      });
    }

    if (params.searchTerm) {
      filter.push({ wildcard: { name: '*' + params.searchTerm + '*' } });
    }

    return { query: { bool: { filter } } };
  }

  #buildShareablesQuery (params) {
    const must = [{ term: { type: params.type } }];

    const permissionFilters = [];
    permissionFilters.push({ term: { creator: params.user } });

    if (params.viewOnly) {
      permissionFilters.push({ term: { viewUsers: params.user } });
      if (params.roles?.length > 0) {
        permissionFilters.push({ terms: { viewRoles: params.roles } });
      }
    } else {
      permissionFilters.push({ term: { editUsers: params.user } });
      if (params.roles?.length > 0) {
        permissionFilters.push({ terms: { editRoles: params.roles } });
      }
      permissionFilters.push({ term: { viewUsers: params.user } });
      if (params.roles?.length > 0) {
        permissionFilters.push({ terms: { viewRoles: params.roles } });
      }
    }

    const filter = [{
      bool: { should: permissionFilters, minimum_should_match: 1 }
    }];

    return { query: { bool: { must, filter } } };
  }

  #buildShortcutsQuery (params) {
    const filter = [];

    if (!params.all) {
      filter.push({
        bool: {
          should: [
            { terms: { roles: params.roles || [] } },
            { terms: { editRoles: params.roles || [] } },
            { term: { users: params.user } },
            { term: { userId: params.user } }
          ]
        }
      });
    }

    if (params.searchTerm) {
      filter.push({ wildcard: { name: '*' + params.searchTerm + '*' } });
    }

    if (params.fieldType) {
      filter.push({ exists: { field: params.fieldType } });
    }

    // Name collision check: find shortcuts with this name but NOT this id
    if (params.nameCheck) {
      filter.push({ term: { name: params.nameCheck } });
      if (params.excludeId) {
        return { query: { bool: { filter, must_not: [{ ids: { values: [params.excludeId] } }] } } };
      }
    }

    return { query: { bool: { filter } } };
  }
}

module.exports = DbESImpl;
