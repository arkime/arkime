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
  #Db;
  #internals;
  #fixIndex;

  // The sessions instance passes { Db, internals, fixIndex }, which the
  // SESSIONS methods below need; the users instance omits them.
  constructor (client, prefix, { Db, internals, fixIndex } = {}) {
    this.#client = client;
    this.#prefix = prefix;
    this.#Db = Db;
    this.#internals = internals;
    this.#fixIndex = fixIndex;
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
  /// /////////////////////////////////////////////////////////////////////////
  /// / SESSIONS - Elasticsearch implementation of the sessions DB backend.
  /// / internals.sessionsImpl in db.js always points at an implementation
  /// / with this surface; non-sessions indices are always served by this
  /// / implementation via internals.esImpl.
  /// /////////////////////////////////////////////////////////////////////////

  // --------------------------------------------------------------------------
  async searchSessions (index, query, options) {
    return this.#Db.search(index, query, options);
  }

  // --------------------------------------------------------------------------
  // Called by Db.getSession when a session isn't found; refresh the index and
  // return true so the caller retries once.
  async refreshForNotFoundRetry (index) {
    await new Promise((resolve) => {
      this.#internals.client7.indices.refresh({ index: this.#fixIndex(index) }, () => {
        resolve();
      });
    });
    return true;
  }

  // --------------------------------------------------------------------------
  async searchScroll (index, query, options, cb) {
    const Db = this.#Db;
    const internals = this.#internals;

    // external scrolling, or multiES or (not regressionTests AND lesseq 10000), do a normal search which does its own Promise conversion
    if (options?.scroll !== undefined || internals.multiES || (!internals.regressionTests && (query.size ?? 0) + (parseInt(query.from ?? 0, 10)) <= 10000)) {
      if (!cb) {
        return Db.search(index, query, options);
      }
      try {
        return cb(null, await Db.search(index, query, options));
      } catch (err) {
        return cb(err);
      }
    }

    try {
      // Now actually do the search scroll
      const from = +query.from || 0;
      const size = +query.size || 0;

      const querySize = from + size;
      delete query.from;

      let totalResults;
      const params = { scroll: '2m' };
      Db.merge(params, options);
      query.size = 1000; // Get 1000 items per scroll call
      query.profile = internals.esProfile;

      let response = await Db.search(index, query, params);

      while (true) {
        if (totalResults === undefined) {
          totalResults = response;
        } else {
          Array.prototype.push.apply(totalResults.hits.hits, response.hits.hits);
        }

        if (totalResults.hits.total > 0 && totalResults.hits.hits.length < Math.min(response.hits.total, querySize)) {
          try {
            const { body: results } = await Db.scroll({
              scroll: '2m', body: { scroll_id: response._scroll_id }
            });
            response = results;
          } catch (err) {
            console.log('ERROR - issuing scroll', err);
            if (totalResults) {
              totalResults.hits.hits = totalResults.hits.hits.slice(from, querySize);
            }
            if (response._scroll_id) {
              await Db.clearScroll({ body: { scroll_id: response._scroll_id } });
            }
            throw err;
          }
        } else {
          break;
        }
      }

      if (totalResults) {
        totalResults.hits.hits = totalResults.hits.hits.slice(from, querySize);
      }
      if (response._scroll_id) {
        await Db.clearScroll({ body: { scroll_id: response._scroll_id } });
      }

      if (cb) { cb(null, totalResults); }
      return totalResults;
    } catch (err) {
      if (cb) { cb(err); }
      throw err;
    }
  }

  // --------------------------------------------------------------------------
  async * searchScrollIterator (index, query, options) {
    const Db = this.#Db;
    const internals = this.#internals;

    // external scrolling, or multiES or (not regressionTests AND lesseq 10000), do a normal search which does its own Promise conversion
    if (options?.scroll !== undefined || internals.multiES || (!internals.regressionTests && (query.size ?? 0) + (parseInt(query.from ?? 0, 10)) <= 10000)) {
      const result = await Db.search(index, query, options);
      yield result;
      return;
    }

    let from = +query.from || 0;
    const size = +query.size || 0;
    delete query.from;

    let yielded = 0;
    const params = { scroll: '2m' };
    Db.merge(params, options);
    query.size = internals.regressionTests ? 20 : 2000;
    query.profile = internals.esProfile;

    let response = await Db.search(index, query, params);

    while (true) {
      let hits = response.hits.hits;

      // Stop if no more results
      if (hits.length === 0) {
        break;
      }

      if (from === 0) {
        // Don't do anything
      } else if (from < hits.length) {
        hits = hits.slice(from);
        from = 0;
      } else {
        from -= hits.length;
        hits = [];
      }

      if (hits.length > 0) {
        response.hits.hits = hits.slice(0, size - yielded);
        yielded += response.hits.hits.length;
        yield response;

        if (yielded >= size) {
          break;
        }
      }

      // Fetch next chunk
      try {
        const { body: results } = await Db.scroll({
          scroll: '2m', body: { scroll_id: response._scroll_id }
        });
        response = results;
      } catch (err) {
        console.log('ERROR - issuing scroll', err);
        if (response._scroll_id) {
          await Db.clearScroll({ body: { scroll_id: response._scroll_id } });
        }
        throw err;
      }
    }

    if (response._scroll_id) {
      await Db.clearScroll({ body: { scroll_id: response._scroll_id } });
    }
  }

  // --------------------------------------------------------------------------
  async msearchSessions (index, queries, options) {
    const body = [];

    for (let i = 0, ilen = queries.length; i < ilen; i++) {
      body.push({ index: this.#fixIndex(index) });
      body.push(queries[i]);
    }

    const params = { body, rest_total_hits_as_int: true };

    let cancelId = null;
    if (options && options.cancelId) {
      // use opaqueId option so the task can be cancelled
      cancelId = { opaqueId: options.cancelId };
    }

    return this.#internals.client7.msearch(params, cancelId);
  }

  // --------------------------------------------------------------------------
  async deleteDocument (index, id, options) {
    const params = { index: this.#fixIndex(index), id };
    this.#Db.merge(params, options);
    return this.#internals.client7.delete(params);
  }

  // --------------------------------------------------------------------------
  async updateSession (index, id, doc) {
    const params = {
      retry_on_conflict: 3,
      index: this.#fixIndex(index),
      body: doc,
      id,
      timeout: '10m'
    };

    try {
      const { body: data } = await this.#internals.client7.update(params);
      return data;
    } catch (err) {
      if (err.statusCode !== 403) {
        throw err;
      }

      await this.#Db.setIndexSettings(this.#fixIndex(index), { body: { 'index.blocks.write': null } });
      const { body: retryData } = await this.#internals.client7.update(params);
      return retryData;
    }
  }

  // --------------------------------------------------------------------------
  async addTagsToSession (index, id, tags, cluster) {
    const body = {
      script: {
        source: `
        if (ctx._source.tags != null) {
          for (int i = 0; i < params.tags.length; i++) {
            if (ctx._source.tags.indexOf(params.tags[i]) == -1) {
              ctx._source.tags.add(params.tags[i]);
            }
          }
          ctx._source.tagsCnt = ctx._source.tags.length;
        } else {
          ctx._source.tags = params.tags;
          ctx._source.tagsCnt = params.tags.length;
        }
      `,
        lang: 'painless',
        params: { tags }
      }
    };

    if (cluster) { body.cluster = cluster; }

    return this.updateSession(index, id, body);
  }

  // --------------------------------------------------------------------------
  async removeTagsFromSession (index, id, tags, cluster) {
    const body = {
      script: {
        source: `
        if (ctx._source.tags != null) {
          for (int i = 0; i < params.tags.length; i++) {
            int idx = ctx._source.tags.indexOf(params.tags[i]);
            if (idx > -1) { ctx._source.tags.remove(idx); }
          }
          ctx._source.tagsCnt = ctx._source.tags.length;
          if (ctx._source.tagsCnt == 0) {
            ctx._source.remove("tags");
            ctx._source.remove("tagsCnt");
          }
        }
      `,
        lang: 'painless',
        params: { tags }
      }
    };

    if (cluster) { body.cluster = cluster; }

    return this.updateSession(index, id, body);
  }

  // --------------------------------------------------------------------------
  async addHuntToSession (index, id, huntId, huntName) {
    const body = {
      script: {
        source: `
        if (ctx._source.huntId != null) {
          ctx._source.huntId.add(params.huntId);
        } else {
          ctx._source.huntId = [ params.huntId ];
        }
        if (ctx._source.huntName != null) {
          ctx._source.huntName.add(params.huntName);
        } else {
          ctx._source.huntName = [ params.huntName ];
        }
      `,
        lang: 'painless',
        params: { huntId, huntName }
      }
    };

    return this.updateSession(index, id, body);
  }

  // --------------------------------------------------------------------------
  async removeHuntFromSession (index, id, huntId, huntName) {
    const body = {
      script: {
        source: `
        if (ctx._source.huntId != null) {
          int idx = ctx._source.huntId.indexOf(params.huntId);
          if (idx > -1) { ctx._source.huntId.remove(idx); }
        }
        if (ctx._source.huntName != null) {
          int idx = ctx._source.huntName.indexOf(params.huntName);
          if (idx > -1) { ctx._source.huntName.remove(idx); }
        }
      `,
        lang: 'painless',
        params: { huntId, huntName }
      }
    };

    return this.updateSession(index, id, body);
  }

  // --------------------------------------------------------------------------
  async numberOfDocuments (index, options) {
    // count interface is slow for large data sets, don't use for sessions unless multiES
    if (index !== 'sessions2-*' || this.#internals.multiES) {
      const params = { index: this.#fixIndex(index), ignore_unavailable: true };
      this.#Db.merge(params, options);
      const { body: total } = await this.#internals.client7.count(params);
      return { count: total.count };
    }

    let count = 0;
    const str = `${this.#internals.prefix}sessions2-`;

    const indices = await this.#Db.indicesCache(options.cluster);

    for (let i = 0; i < indices.length; i++) {
      if (indices[i].index.includes(str)) {
        count += parseInt(indices[i]['docs.count']);
      }
    }

    return { count };
  }

  // --------------------------------------------------------------------------
  // Map a time range to the concrete list of session indices to query, by
  // walking the aliases cache and parsing rotate-style date suffixes
  async getIndices (startTime, stopTime, bounding, rotateIndex, extraIndices) {
    try {
      const aliases = await this.#Db.getAliasesCache();
      const indices = [];

      // Guess how long hour indices we find are
      let hlength = 0;
      if (rotateIndex === 'hourly') {
        hlength = 60 * 60;
      } else if (rotateIndex.startsWith('hourly')) {
        hlength = +rotateIndex.substring(6) * 60 * 60;
      } else {
        hlength = 12 * 60 * 60; // Max hourly can be is 12 hours
      }

      // Go thru each index, convert to start/stop range and see if our time range overlaps
      // For hourly and month indices (and user-specified queryExtraIndices) we may search extra
      for (const iname in aliases) {
        let index = iname;
        let isQueryExtraIndex = false;
        if (index.startsWith('partial-')) {
          index = index.substring(8);
        }
        if (index.endsWith('-shrink')) {
          index = index.substring(0, index.length - 7);
        }
        if (index.endsWith('-reindex')) {
          index = index.substring(0, index.length - 8);
        }
        if (index.startsWith('sessions2-')) { // sessions2 might not have prefix
          index = index.substring(10);
        } else if (this.#internals.queryExtraIndicesRegex.some(re => re.test(index))) {
          // extra user-specified indexes from the queryExtraIndices don't have the prefix
          isQueryExtraIndex = true;
        } else {
          index = index.substring(this.#internals.prefix.length + 10);
        }

        let year; let month; let day = 0; let hour = 0; let len;
        let queryExtraIndexTimeMatched = false; let queryExtraIndexTimeMatch;

        if (isQueryExtraIndex) {
          // the user-specified queryExtraIndices are less under our control, so we
          //   are going to take some regex-based best guesses to figure out if it's hourly, daily, etc.

          // daily 240311                         v year      v month        v day
          queryExtraIndexTimeMatch = iname.match(/([0-9][0-9])(0[1-9]|1[0-2])(0[1-9]|[12][0-9]|3[01])$/);
          if (queryExtraIndexTimeMatch) {
            queryExtraIndexTimeMatched = true;
            index = queryExtraIndexTimeMatch[0];
          }

          if (!queryExtraIndexTimeMatched) {
            // hourly 240311h19                     v year      v month        v day                    h  v hour
            queryExtraIndexTimeMatch = iname.match(/([0-9][0-9])(0[1-9]|1[0-2])(0[1-9]|[12][0-9]|3[01])[Hh]([01][0-9]|2[0-3])$/);
            if (queryExtraIndexTimeMatch) {
              queryExtraIndexTimeMatched = true;
              index = queryExtraIndexTimeMatch[0];
            }
          }

          if (!queryExtraIndexTimeMatched) {
            // weekly 24w10                         v year     w  v week
            queryExtraIndexTimeMatch = iname.match(/([0-9][0-9])[Ww]([0-4][0-9]|5[0-3])$/);
            if (queryExtraIndexTimeMatch) {
              queryExtraIndexTimeMatched = true;
              index = queryExtraIndexTimeMatch[0];
            }
          }

          if (!queryExtraIndexTimeMatched) {
            // monthly 24m10                        v year     m  v month
            queryExtraIndexTimeMatch = iname.match(/([0-9][0-9])[Mm](0[1-9]|1[0-2])$/);
            if (queryExtraIndexTimeMatch) {
              queryExtraIndexTimeMatched = true;
              index = queryExtraIndexTimeMatch[0];
            }
          }
        } // if (isQueryExtraIndex)

        if (!isQueryExtraIndex || queryExtraIndexTimeMatched) {
          if (+index[0] >= 6) {
            year = 1900 + (+index[0]) * 10 + (+index[1]);
          } else {
            year = 2000 + (+index[0]) * 10 + (+index[1]);
          }

          if (index[2] === 'w') {
            len = 7 * 24 * 60 * 60;
            month = 1;
            day = (+index[3] * 10 + (+index[4])) * 7;
          } else if (index[2] === 'm') {
            month = (+index[3]) * 10 + (+index[4]);
            day = 1;
            len = 31 * 24 * 60 * 60;
          } else if (index.length === 6) {
            month = (+index[2]) * 10 + (+index[3]);
            day = (+index[4]) * 10 + (+index[5]);
            len = 24 * 60 * 60;
          } else {
            month = (+index[2]) * 10 + (+index[3]);
            day = (+index[4]) * 10 + (+index[5]);
            hour = (+index[7]) * 10 + (+index[8]);
            // queryExtraIndices don't really have any way to specify (hourly[23468]|hourly12),
            //   so for those hourly just means "hourly" with regards to length calculation
            len = isQueryExtraIndex ? (60 * 60) : hlength;
          }

          const start = Date.UTC(year, month - 1, day, hour) / 1000;
          const stop = Date.UTC(year, month - 1, day, hour) / 1000 + len;

          switch (bounding) {
          default:
          case 'last':
            if (stop >= startTime && start <= stopTime) {
              indices.push(iname);
            }
            break;
          case 'first':
          case 'both':
          case 'either':
          case 'database':
            if (stop >= (startTime - len) && start <= (stopTime + len)) {
              indices.push(iname);
            }
            break;
          }
        } else if (isQueryExtraIndex) {
          // this is an extra user-specified index pattern from queryExtraIndices, and
          //   we couldn't grok it, so just query the whole thing
          indices.push(iname);
        }
      } // for (const iname in aliases)

      if (indices.length === 0) {
        return this.#fixIndex(this.#internals.sessionIndices);
      }

      if (this.#internals.debug > 2) {
        console.log(`getIndices: ${indices}`);
      }
      return indices.join(',');
    } catch {
      return '';
    }
  }

  // --------------------------------------------------------------------------
  getSessionIndices (excludeExtra) {
    if (excludeExtra) {
      return ['sessions2-*', 'sessions3-*'];
    }
    return this.#internals.sessionIndices;
  }

  // --------------------------------------------------------------------------
  async getMinValue (index, field) {
    const params = {
      index: this.#fixIndex(index),
      body: { size: 0, aggs: { min: { min: { field } } } }
    };
    return this.#internals.client7.search(params);
  }

  // --------------------------------------------------------------------------
  close () {
    // client7 is closed by Db.close itself
  }
}

module.exports = DbESImpl;
