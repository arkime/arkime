/******************************************************************************/
/* featherprint.db.js -- ES persistence for featherprint device records,
 *                       history, alerts and MAC-keyed tracking. Reads/writes
 *                       via the bare index aliases (`featherprint`,
 *                       `featherprint_history`, `featherprint_alerts`,
 *                       `featherprint_macs`) so re-indexes are transparent.
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const nodeCrypto = require('crypto');
const Db = require('./db.js');

class FeatherprintDb {
  static INDEX = 'featherprint';
  static HISTORY_INDEX = 'featherprint_history';
  static ALERTS_INDEX = 'featherprint_alerts';
  static MACS_INDEX = 'featherprint_macs';
  static STATE_ID = '__state__';

  // --------------------------------------------------------------------------
  // Persisted monitor state (single doc, well-known id). Tracks the
  // last-processed @timestamp so restarts resume instead of jumping forward.
  static async getState () {
    return FeatherprintDb.#findOne(FeatherprintDb.INDEX, { _id: FeatherprintDb.STATE_ID });
  }

  static async saveState (state) {
    const doc = { kind: 'state', ...state, lastUpdated: Date.now() };
    await Db.indexNow(FeatherprintDb.INDEX, FeatherprintDb.STATE_ID, doc);
    return doc;
  }

  // --------------------------------------------------------------------------
  // Get a single device record by IP. Returns the doc (with _id) or null.
  static async getIp (ip) {
    return FeatherprintDb.#findOne(FeatherprintDb.INDEX, { ip }, true);
  }

  // Upsert a device record. Used by monitor mode.
  static async upsertDevice (doc) {
    await Db.indexNow(FeatherprintDb.INDEX, doc.ip, doc);
    return doc.ip;
  }

  // Search devices. ip is an exact IP or CIDR (term query on the ip-typed
  // field -- wildcards are not supported on ip fields); mac/name are
  // substring matches.
  static async searchDevices ({ ip, mac, name: nameQ, limit = 200 } = {}) {
    // Always require an ip field so the __state__ doc is excluded.
    const must = [{ exists: { field: 'ip' } }];
    if (ip) must.push({ term: { ip } });
    if (mac) must.push({ wildcard: { 'mac.value': `*${mac.toLowerCase()}*` } });
    if (nameQ) must.push({ wildcard: { 'names.name': `*${nameQ}*` } });

    const r = await Db.search(FeatherprintDb.INDEX, {
      query: { bool: { must } },
      sort: [{ lastSeen: 'desc' }],
      size: limit
    });
    return (r?.hits?.hits ?? []).map(h => ({ _id: h._id, ...h._source }));
  }

  // --------------------------------------------------------------------------
  // History: append-only, one doc per (ip, kind, ts, seq). seq disambiguates
  // multiple same-kind events from one processing batch; duplicate writes
  // from re-run polling still collapse onto the same id.
  static async appendHistory (entry) {
    // Id keyed on the event content, not a per-batch sequence number: two
    // distinct same-kind events for one IP can share a ts (record.lastSeen
    // only ratchets up, and @timestamp-based chunking lags lastPacket, so a
    // later batch's events can carry an earlier batch's lastSeen). Hashing
    // {kind, before, after} keeps genuine re-runs collapsing onto one doc
    // while letting distinct events coexist.
    const digest = nodeCrypto.createHash('sha1')
      .update(JSON.stringify({ kind: entry.kind, before: entry.before ?? null, after: entry.after ?? null }))
      .digest('hex').slice(0, 16);
    const id = `${entry.ip}|${entry.kind}|${entry.ts}|${digest}`;
    await Db.index(FeatherprintDb.HISTORY_INDEX, id, entry);
    return id;
  }

  static async listHistory (ip, limit = 50) {
    const r = await Db.search(FeatherprintDb.HISTORY_INDEX, {
      query: { term: { ip } },
      sort: [{ ts: 'desc' }],
      size: limit
    });
    return (r?.hits?.hits ?? []).map(h => ({ _id: h._id, ...h._source }));
  }

  // --------------------------------------------------------------------------
  // Alerts: insert, list with optional ack filter, ack.
  static async insertAlert (alertDoc) {
    const r = await Db.indexNow(FeatherprintDb.ALERTS_INDEX, undefined, alertDoc);
    return r?.body?._id ?? r?._id;
  }

  static async listAlerts ({ acked, limit = 500 } = {}) {
    const r = await Db.search(FeatherprintDb.ALERTS_INDEX, {
      query: acked === undefined ? { match_all: {} } : { term: { acked } },
      sort: [{ ts: 'desc' }],
      size: limit
    });
    return (r?.hits?.hits ?? []).map(h => ({ _id: h._id, ...h._source }));
  }

  static async ackAlert (id, user, ackedAt = Date.now()) {
    await Db.update(FeatherprintDb.ALERTS_INDEX, id, {
      doc: { acked: true, ackedBy: user, ackedAt }
    }, { refresh: true });
  }

  // --------------------------------------------------------------------------
  // MAC-keyed tracking: one doc per MAC, with current IP and history so we
  // can emit changeIp when a known MAC shows up on a new IP (arpwatch-style).
  static async getMac (mac) {
    return FeatherprintDb.#findOne(FeatherprintDb.MACS_INDEX, { mac }, true);
  }

  static async upsertMac (doc) {
    await Db.indexNow(FeatherprintDb.MACS_INDEX, doc.mac, doc);
    return doc.mac;
  }

  // --------------------------------------------------------------------------
  // Shared helper: 1-hit term lookup. `term` is the inner clause body
  // (e.g. `{ ip }` or `{ _id: '__state__' }`). Returns _source (with _id
  // when includeId=true), or null on missing index / no hit.
  static async #findOne (index, term, includeId = false) {
    try {
      const r = await Db.search(index, { query: { term }, size: 1 });
      const hit = r?.hits?.hits?.[0];
      if (!hit) return null;
      return includeId ? { _id: hit._id, ...hit._source } : hit._source;
    } catch (e) {
      if (e?.meta?.statusCode === 404) return null;
      throw e;
    }
  }
}

module.exports = FeatherprintDb;
