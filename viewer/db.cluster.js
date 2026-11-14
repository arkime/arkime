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

// The stats doc capture writes, a SQL cluster db gives each its own column. ARKIME_DB_STATS_FIELDS in
// capture/db.c has the same list, a new field goes in both and its column is added the next time either starts.
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

// db.pl maps these as epoch_second dates, ES would read a number as epoch millis
const SECONDS = 'epoch_second';

// What expiry, the files and stats pages and capture look up, arkime_db_cluster_tables in capture/db.c is the same layout
const COLLECTION_OPTIONS = {
  stats: { typed: true, fields: STATS_FIELDS, es: { dateFormats: { currentTime: SECONDS } } },
  history: { es: { dateFormats: { timestamp: SECONDS } } },
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
    },
    es: { dateFormats: { currentTime: SECONDS } }
  }
};

function twoDigitString (value) {
  return (value < 10) ? ('0' + value) : value.toString();
}

// Key order independent, Postgres stores jsonb with its own key order
function canonical (value) {
  if (Array.isArray(value)) { return `[${value.map(canonical).join(',')}]`; }
  if (value !== null && typeof value === 'object') {
    return `{${Object.keys(value).sort().map(k => `${JSON.stringify(k)}:${canonical(value[k])}`).join(',')}}`;
  }
  return JSON.stringify(value);
}

class DbClusterImpl {
  #hunts;
  #queries;
  #history;
  featherprint = {};
  #sharedES;
  #multiES;

  // capture writes these, to the cluster db like the rest
  files;
  stats;
  dstats;
  fields;
  sequence;

  /**
   * Each collection gets its fields for the other backends plus es options naming the index db.pl made,
   * every backend reads only its own options.
   * @param conn the cluster db connection
   * @param esConn the sessions ES connection
   * @param prefix, fixIndex build the sessions ES index names
   * @param multiES conn goes through multies, which only answers the calls it routes
   */
  constructor ({ conn, esConn, prefix, fixIndex, multiES }) {
    this.#multiES = !!multiES;
    // Only the sessions ES client knows the multi cluster routing
    this.#sharedES = conn === esConn;
    const options = (collName, es = {}) => {
      const base = DbClusterImpl.collectionOptions(collName);
      return { ...base, es: { ...base.es, index: fixIndex(collName), dbpl: true, ...es } };
    };
    for (const collName of ['files', 'stats', 'dstats', 'fields', 'sequence']) {
      this[collName] = conn.collection(collName, options(collName, collName === 'files' ? { checkMapping: true } : {}));
    }

    this.#hunts = conn.collection('hunts', options('hunts'));
    this.#queries = conn.collection('queries', options('queries'));
    this.#history = conn.collection('history', options('history', {
      index: fixIndex('history_v1-*'),
      writeIndex: () => {
        const d = new Date(Date.now());
        const jan = new Date(d.getUTCFullYear(), 0, 0);
        return prefix + 'history_v1-' + twoDigitString(d.getUTCFullYear() % 100) + 'w' +
          twoDigitString(Math.floor((d - jan) / 604800000));
      }
    }));

    for (const [key, collName] of [['devices', 'featherprint'], ['history', 'featherprint_history'], ['alerts', 'featherprint_alerts']]) {
      this.featherprint[key] = conn.collection(collName, options(collName));
    }
  }

  // The index names the cluster db holds, every other index is a sessions or users one
  static INDEX_NAMES = ['files', 'stats', 'dstats', 'fields', 'sequence', 'hunts', 'queries', 'history', 'featherprint'];

  static collectionOptions (collName) {
    return COLLECTION_OPTIONS[collName] ?? {};
  }

  async init () {
    await Promise.all([this.#hunts.init(), this.#queries.init(), this.#history.init(), ...Object.values(this.featherprint).map(c => c.init())]);
  }

  /**
   * Throws when what capture writes isn't set up, a multi viewer only checks what multies routes
   */
  async check () {
    for (const coll of [this.stats, this.dstats, this.sequence]) { await coll.check(); }
    if (this.#multiES) { return; }
    await this.files.check();
    try {
      await DbClusterImpl.loadBaseFields(this.fields);
    } catch (err) {
      console.log('WARNING - loading the base fields', err.message);
    }
  }

  /**
   * capture only defines the fields it writes, so the base fields come from db/fields.json.
   * db.pl loads the same file into ES, a db without db.pl gets them the first time a viewer starts.
   * Only a missing or changed doc is written, every viewer start gets here
   * @param fields the cluster db fields collection
   */
  static async loadBaseFields (fields) {
    const docs = JSON.parse(fs.readFileSync(path.join(__dirname, '../db/fields.json'), 'utf8'));
    const { data } = await fields.find({ size: 10000 });
    const have = new Map(data.map(h => [h.id, canonical(h.doc)]));
    await Promise.all(Object.entries(docs)
      .filter(([id, doc]) => have.get(id) !== canonical(doc))
      .map(([id, doc]) => fields.put(id, doc)));
  }

  #cluster (cluster) {
    return this.#sharedES ? cluster : undefined;
  }

  // The regression tests clear these wherever the cluster db is
  testCollection (collName) {
    return { hunts: this.#hunts, queries: this.#queries }[collName];
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
    // Older versions kept it in the queries index _meta. multies has no _mapping route, a request there never answers
    if (this.#multiES) { return { node: undefined, updateTime: undefined }; }
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
