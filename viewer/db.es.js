/******************************************************************************/
/* db.es.js -- Elasticsearch implementation of the viewer DB backend
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';

// Session update scripts, esProxy only lets sensors send addHuntToSession
const SESSION_SCRIPTS = {
  addTagsToSession: `
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
  removeTagsFromSession: `
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
  addHuntToSession: `
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
  removeHuntFromSession: `
        if (ctx._source.huntId != null) {
          int idx = ctx._source.huntId.indexOf(params.huntId);
          if (idx > -1) { ctx._source.huntId.remove(idx); }
        }
        if (ctx._source.huntName != null) {
          int idx = ctx._source.huntName.indexOf(params.huntName);
          if (idx > -1) { ctx._source.huntName.remove(idx); }
        }
      `
};

class DbESImpl {
  #Db;
  #internals;
  #fixIndex;

  constructor ({ Db, internals, fixIndex }) {
    this.#Db = Db;
    this.#internals = internals;
    this.#fixIndex = fixIndex;
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
      if (cb) { return cb(err); }
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
        source: SESSION_SCRIPTS.addTagsToSession,
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
        source: SESSION_SCRIPTS.removeTagsFromSession,
        lang: 'painless',
        params: { tags }
      }
    };

    if (cluster) { body.cluster = cluster; }

    return this.updateSession(index, id, body);
  }

  // --------------------------------------------------------------------------
  async addHuntToSession (index, id, huntId, huntName) {
    // esProxy's validateUpdate only passes this exact body from sensor viewers
    const body = {
      script: {
        source: SESSION_SCRIPTS.addHuntToSession,
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
        source: SESSION_SCRIPTS.removeHuntFromSession,
        lang: 'painless',
        params: { huntId, huntName }
      }
    };

    return this.updateSession(index, id, body);
  }

  // --------------------------------------------------------------------------
  async numberOfDocuments (index, options) {
    const params = { index: this.#fixIndex(index), ignore_unavailable: true };
    this.#Db.merge(params, options);
    const { body: total } = await this.#internals.client7.count(params);
    return { count: total.count };
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
        if (this.#internals.queryExtraIndicesRegex.some(re => re.test(index))) {
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
      return ['sessions3-*'];
    }
    return this.#internals.sessionIndices;
  }

  // --------------------------------------------------------------------------
  async getMinValue (index, field) {
    const params = {
      index: this.#fixIndex(index),
      body: { size: 0, aggs: { min: { min: { field } } } },
      rest_total_hits_as_int: true
    };
    return this.#internals.client7.search(params);
  }

  // --------------------------------------------------------------------------
  close () {
    // client7 is closed by Db.close itself
  }
}

DbESImpl.SESSION_SCRIPTS = SESSION_SCRIPTS;

module.exports = DbESImpl;
