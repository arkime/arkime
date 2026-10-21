/******************************************************************************/
/* db.cluster.js -- hunts, queries and history in the cluster db
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';

function twoDigitString (value) {
  return (value < 10) ? ('0' + value) : value.toString();
}

class DbClusterImpl {
  #hunts;
  #queries;
  #history;
  #isES;
  #sharedES;

  // capture still writes these to the sessions ES, so they are read from there
  files;
  stats;
  dstats;
  fields;
  sequence;

  /**
   * @param conn the cluster db connection
   * @param esConn the sessions ES connection
   * @param isES conn is an Elasticsearch/OpenSearch connection
   * @param prefix, fixIndex build the sessions ES index names
   */
  constructor ({ conn, esConn, isES, prefix, fixIndex }) {
    this.#isES = isES;
    // Only the sessions ES client knows the multi cluster routing
    this.#sharedES = conn === esConn;
    for (const collName of ['files', 'stats', 'dstats', 'fields', 'sequence']) {
      this[collName] = esConn.collection(collName, { es: { index: fixIndex(collName) } });
    }

    if (this.#isES) {
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

  #cluster (cluster) {
    return this.#sharedES ? cluster : undefined;
  }

  // --------------------------------------------------------------------------
  // HUNTS
  // --------------------------------------------------------------------------
  async createHunt (doc, cluster) {
    return this.#hunts.put(null, doc, { cluster: this.#cluster(cluster) });
  }

  async getHunt (id, cluster) {
    return this.#hunts.get(id, { cluster: this.#cluster(cluster) });
  }

  async updateHunt (id, fields, cluster) {
    return this.#hunts.update(id, fields, { cluster: this.#cluster(cluster) });
  }

  async deleteHunt (id, cluster) {
    return this.#hunts.delete(id, { cluster: this.#cluster(cluster) });
  }

  async searchHunts (params, cluster) {
    return this.#hunts.find({ ...params, cluster: this.#cluster(cluster) });
  }

  async countHunts (where, cluster) {
    return this.#hunts.count(where, { cluster: this.#cluster(cluster) });
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
    return this.#history.put(null, doc, { cluster: this.#cluster(cluster) });
  }

  async searchHistory (params, cluster) {
    return this.#history.find({ ...params, cluster: this.#cluster(cluster) });
  }

  async countHistory (cluster) {
    return this.#history.count(null, { cluster: this.#cluster(cluster) });
  }

  async deleteHistory (id, cluster) {
    return this.#history.delete(id, { cluster: this.#cluster(cluster) });
  }

  async deleteAllHistory () {
    return this.#history.deleteAll();
  }
}

module.exports = DbClusterImpl;
