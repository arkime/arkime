/******************************************************************************/
/* db.cluster.js -- hunts, queries and history in the cluster db
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';

// Non ES history has no index, but the client sends one back to delete
const HISTORY_INDEX = 'arkime_history_v1-db';

function twoDigitString (value) {
  return (value < 10) ? ('0' + value) : value.toString();
}

class DbClusterImpl {
  #hunts;
  #queries;
  #history;
  #isES;

  /**
   * @param conn the cluster db connection
   * @param esNames when conn is the sessions ES: { prefix, fixIndex } to build the index names
   */
  constructor (conn, esNames) {
    this.#isES = !!esNames;
    if (esNames) {
      const { prefix, fixIndex } = esNames;
      this.#hunts = conn.collection('hunts', { es: { index: fixIndex('hunts') } });
      this.#queries = conn.collection('queries', { es: { index: fixIndex('queries') } });
      this.#history = conn.collection('history', {
        es: {
          // Didn't used to have a prefix
          index: prefix === 'arkime_' ? 'history_v1-*,arkime_history_v1-*' : fixIndex('history_v1-*'),
          writeIndex: () => {
            const d = new Date(Date.now());
            const jan = new Date(d.getUTCFullYear(), 0, 0);
            return prefix + 'history_v1-' + twoDigitString(d.getUTCFullYear() % 100) + 'w' +
              twoDigitString(Math.floor((d - jan) / 604800000));
          }
        }
      });
    } else {
      this.#hunts = conn.collection('hunts');
      this.#queries = conn.collection('queries');
      this.#history = conn.collection('history');
    }
  }

  async init () {
    await Promise.all([this.#hunts.init(), this.#queries.init(), this.#history.init()]);
  }

  // --------------------------------------------------------------------------
  // HUNTS
  // --------------------------------------------------------------------------
  async createHunt (doc, cluster) {
    return this.#hunts.put(null, doc, { cluster });
  }

  async getHunt (id, cluster) {
    return this.#hunts.get(id, { cluster });
  }

  async updateHunt (id, fields, cluster) {
    return this.#hunts.update(id, fields, { cluster });
  }

  async deleteHunt (id, cluster) {
    return this.#hunts.delete(id, { cluster });
  }

  async searchHunts (params, cluster) {
    return this.#hunts.find({ ...params, cluster });
  }

  async countHunts (where, cluster) {
    return this.#hunts.count(where, { cluster });
  }

  // --------------------------------------------------------------------------
  // QUERIES (crons), the primary-viewer doc is the cron/hunt processing lease
  // --------------------------------------------------------------------------
  async createQuery (doc) {
    return this.#queries.put(null, doc);
  }

  async getQuery (id) {
    return this.#queries.get(id);
  }

  async updateQuery (id, fields) {
    return this.#queries.update(id, fields);
  }

  async deleteQuery (id) {
    return this.#queries.delete(id);
  }

  async searchQueries (params) {
    const { total, data } = await this.#queries.find({ ...params, where: { $and: [{ id: { $ne: 'primary-viewer' } }, params.where ?? {}] } });
    return { total, data };
  }

  async setQueriesNode (node, force) {
    const namePid = `${node}-${process.pid}`;
    // force is true we just rewrite the primary-viewer entry every time
    if (force) {
      await this.#queries.put('primary-viewer', { name: namePid, lastRun: Date.now(), enabled: false }, { refresh: false });
      return true;
    }
    // take it if it is ours or a minute old
    return this.#queries.lease('primary-viewer', namePid, 60000);
  }

  async getQueriesNode () {
    const doc = await this.#queries.get('primary-viewer');
    if (doc) {
      return { node: doc.name, updateTime: doc.lastRun };
    }
    // Older versions kept it in the queries index _meta
    if (!this.#isES) { return { node: undefined, updateTime: undefined }; }
    return { node: await this.#queries.getMeta('node'), updateTime: await this.#queries.getMeta('updateTime') };
  }

  // --------------------------------------------------------------------------
  // HISTORY
  // --------------------------------------------------------------------------
  async addHistory (doc, cluster) {
    return this.#history.put(null, doc, { cluster });
  }

  async searchHistory (params, cluster) {
    const { total, data } = await this.#history.find({ ...params, cluster });
    for (const hit of data) {
      hit.index ??= HISTORY_INDEX;
    }
    return { total, data };
  }

  async countHistory (cluster) {
    return this.#history.count(null, { cluster });
  }

  async deleteHistory (id, cluster) {
    return this.#history.delete(id, { cluster });
  }

  async deleteAllHistory () {
    return this.#history.deleteAll();
  }
}

module.exports = DbClusterImpl;
