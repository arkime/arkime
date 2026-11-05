/******************************************************************************/
/* db.cluster.js -- hunts, queries and history in the cluster db
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';

const fs = require('fs');
const path = require('path');

// The stats doc capture writes, a Postgres cluster db gives each its own column. capture/db.pg.c
// has the same list, a new field goes in both and its column is added the next time either starts.
const STATS_FIELDS = Object.fromEntries([
  ...['ver', 'nodeName', 'hostname'].map(f => [f, 'keyword']),
  ['interval', 'short'], ['currentTime', 'date'], ['freeSpaceP', 'float'], ['memoryP', 'float'],
  ...['usedSpaceM', 'freeSpaceM', 'monitoring', 'memory', 'cpu', 'diskQueue', 'esQueue', 'packetQueue', 'fragsQueue',
    'frags', 'needSave', 'closeQueue', 'tcpSessions',
    'udpSessions', 'icmpSessions', 'sctpSessions', 'espSessions', 'otherSessions', 'deltaPackets', 'deltaBytes',
    'deltaWrittenBytes', 'deltaUnwrittenBytes', 'deltaSessions', 'deltaSessionBytes', 'deltaDropped',
    'deltaFragsDropped', 'deltaOverloadDropped', 'deltaESDropped', 'deltaDupDropped', 'esHealthMS', 'deltaMS',
    'startTime'].map(f => [f, 'long'])
]);

// What expiry, the files and stats pages and capture look up, capture creates the same columns in db.pg.c
const COLLECTION_OPTIONS = {
  stats: { typed: true, fields: STATS_FIELDS },
  files: {
    fields: {
      node: { type: 'keyword', indexed: true },
      name: { type: 'keyword', indexed: 'wildcard' },
      first: { type: 'long', indexed: 'sort' },
      num: { type: 'long', indexed: 'sort' },
      locked: { type: 'long', indexed: true }
    }
  },
  // Devices (with the mac and state docs), their history and alerts, 100k+ devices on big networks
  featherprint: { fields: { ip: { type: 'ip', indexed: true }, lastSeen: { type: 'long', indexed: 'sort' } } },
  featherprint_history: { fields: { ip: { type: 'ip', indexed: true }, ts: { type: 'long', indexed: 'sort' } } },
  featherprint_alerts: { fields: { acked: { type: 'boolean', indexed: true }, ts: { type: 'long', indexed: 'sort' } } },
  dstats: {
    typed: true,
    fields: {
      ...STATS_FIELDS,
      nodeName: { type: 'keyword', indexed: true },
      interval: { type: 'short', indexed: true },
      currentTime: { type: 'date', indexed: true }
    }
  }
};

function twoDigitString (value) {
  return (value < 10) ? ('0' + value) : value.toString();
}

class DbClusterImpl {
  #hunts;
  #queries;
  #history;
  #isES;
  featherprint = {};
  #isPG;
  #sharedES;

  // capture writes these, to the cluster db like the rest
  files;
  stats;
  dstats;
  fields;
  sequence;

  /**
   * @param conn the cluster db connection
   * @param esConn the sessions ES connection
   * @param isES conn is an Elasticsearch/OpenSearch connection
   * @param isPG conn is Postgres, which gets its base fields from db/fields.json instead of db.pl
   * @param prefix, fixIndex build the sessions ES index names
   */
  constructor ({ conn, esConn, isES, isPG, prefix, fixIndex }) {
    this.#isES = isES;
    this.#isPG = isPG;
    // Only the sessions ES client knows the multi cluster routing
    this.#sharedES = conn === esConn;
    for (const collName of ['files', 'stats', 'dstats', 'fields', 'sequence']) {
      this[collName] = conn.collection(collName, isPG ? DbClusterImpl.collectionOptions(collName) : { es: { index: fixIndex(collName) } });
    }

    if (this.#isES) {
      this.#hunts = conn.collection('hunts', { es: { index: fixIndex('hunts') } });
      this.#queries = conn.collection('queries', { es: { index: fixIndex('queries') } });
      this.#history = conn.collection('history', {
        es: {
          index: fixIndex('history_v1-*'),
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

    // On ES db.pl creates these behind their aliases
    for (const [key, collName] of [['devices', 'featherprint'], ['history', 'featherprint_history'], ['alerts', 'featherprint_alerts']]) {
      this.featherprint[key] = conn.collection(collName, isES ? { es: { index: fixIndex(collName), dbpl: true } } : DbClusterImpl.collectionOptions(collName));
    }
  }

  // The index names the cluster db holds, every other index is a sessions or users one
  static INDEX_NAMES = ['files', 'stats', 'dstats', 'fields', 'sequence', 'hunts', 'queries', 'history', 'featherprint'];

  static collectionOptions (collName) {
    return COLLECTION_OPTIONS[collName] ?? {};
  }

  async init () {
    await Promise.all([this.#hunts.init(), this.#queries.init(), this.#history.init(), ...Object.values(this.featherprint).map(c => c.init())]);
    if (this.#isPG) { await DbClusterImpl.loadBaseFields(this.fields); }
  }

  /**
   * db.pl loads the base fields into ES, a Postgres cluster db gets them from
   * whoever connects first since capture only defines the fields it writes
   * @param fields the cluster db fields collection
   */
  static async loadBaseFields (fields) {
    const docs = JSON.parse(fs.readFileSync(path.join(__dirname, '../db/fields.json'), 'utf8'));
    await Promise.all(Object.entries(docs).map(([id, doc]) => fields.put(id, doc)));
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
