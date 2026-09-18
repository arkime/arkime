/******************************************************************************/
/* featherprint.db.js -- ES persistence for featherprint device records,
 *                       history, alerts and MAC-keyed tracking. Reads/writes
 *                       via the bare index aliases (`featherprint`,
 *                       `featherprint_history`, `featherprint_alerts`) so
 *                       re-indexes are transparent. The `featherprint` index
 *                       holds several doc kinds keyed by id: the device record
 *                       (id = ip), monitor state (`__state__`) and MAC tracking
 *                       (`mac:<addr>`), leaving room for more kinds later.
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
  static MAC_ID_PREFIX = 'mac:';
  static STATE_ID = '__state__';

  // --------------------------------------------------------------------------
  // Persisted monitor state (single doc, well-known id). Tracks the
  // last-processed @timestamp so restarts resume instead of jumping forward.
  static async getState () {
    return FeatherprintDb.#getById(FeatherprintDb.INDEX, FeatherprintDb.STATE_ID);
  }

  static async saveState (state) {
    const doc = { kind: 'state', ...state, lastUpdated: Date.now() };
    await Db.index(FeatherprintDb.INDEX, FeatherprintDb.STATE_ID, doc);
    return doc;
  }

  // --------------------------------------------------------------------------
  // Get a single device record by IP. Returns the doc (with _id) or null.
  // Realtime GET-by-id -- the doc id IS the ip -- so it reads-your-writes
  // without forcing an index refresh on every write.
  static async getIp (ip) {
    return FeatherprintDb.#getById(FeatherprintDb.INDEX, ip, true);
  }

  // Upsert a device record. Used by monitor mode. Plain index (no refresh):
  // getIp reads via realtime GET, so a forced refresh per write isn't needed.
  static async upsertDevice (doc) {
    await Db.index(FeatherprintDb.INDEX, doc.ip, doc);
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
  // Deterministic id from {ip, kind, ts, content} -- same scheme as history --
  // so re-processing a window (failed saveState, restart, cron-leader change)
  // collapses onto one alert instead of re-firing. Create-only: if the id
  // already exists we leave it untouched, so a replay never resurrects an
  // already-acked alert.
  static async insertAlert (alertDoc) {
    const digest = nodeCrypto.createHash('sha1')
      .update(JSON.stringify({ kind: alertDoc.kind, before: alertDoc.before ?? null, after: alertDoc.after ?? null }))
      .digest('hex').slice(0, 16);
    const id = `${alertDoc.ip}|${alertDoc.kind}|${alertDoc.ts}|${digest}`;
    try {
      await Db.get(FeatherprintDb.ALERTS_INDEX, id);
      return { id, created: false }; // already exists, don't overwrite
    } catch (e) {
      if (e?.meta?.statusCode !== 404) throw e;
    }
    await Db.indexNow(FeatherprintDb.ALERTS_INDEX, id, alertDoc);
    return { id, created: true };
  }

  static async listAlerts ({ acked, limit = 500 } = {}) {
    const r = await Db.search(FeatherprintDb.ALERTS_INDEX, {
      query: acked === undefined ? { match_all: {} } : { term: { acked } },
      sort: [{ ts: 'desc' }],
      size: limit
    });
    return (r?.hits?.hits ?? []).map(h => ({ _id: h._id, ...h._source }));
  }

  // Ack state is shared across the team, not per user -- ackedBy/ackedAt are
  // the audit trail of who triaged it. Un-acking clears both so a mistaken ack
  // leaves no misleading attribution behind.
  static #ackDoc (acked, user, ackedAt) {
    return acked
      ? { doc: { acked: true, ackedBy: user, ackedAt } }
      : { doc: { acked: false, ackedBy: null, ackedAt: null } };
  }

  static async ackAlert (id, user, ackedAt = Date.now(), acked = true) {
    await Db.update(FeatherprintDb.ALERTS_INDEX, id,
      FeatherprintDb.#ackDoc(acked, user, ackedAt), { refresh: true });
  }

  // Bulk ack/un-ack. Refreshes once at the end instead of per doc -- a few
  // hundred refresh:true updates stall the index. Runs in small batches so a
  // big ack doesn't open hundreds of concurrent requests.
  static async ackAlerts (ids, user, ackedAt = Date.now(), acked = true) {
    const doc = FeatherprintDb.#ackDoc(acked, user, ackedAt);
    let changed = 0;
    let lastError;
    for (let i = 0; i < ids.length; i += 10) {
      const batch = await Promise.allSettled(ids.slice(i, i + 10).map(
        id => Db.update(FeatherprintDb.ALERTS_INDEX, id, doc)
      ));
      for (const r of batch) {
        if (r.status === 'fulfilled') { changed++; } else { lastError = r.reason; }
      }
    }
    await Db.refresh(FeatherprintDb.ALERTS_INDEX);
    if (changed === 0 && lastError) { throw lastError; }
    return { acked: changed, failed: ids.length - changed };
  }

  // --------------------------------------------------------------------------
  // MAC-keyed tracking: one doc per MAC, with current IP and history so we
  // can emit changeIp when a known MAC shows up on a new IP (arpwatch-style).
  static async getMac (mac) {
    return FeatherprintDb.#getById(FeatherprintDb.INDEX, `${FeatherprintDb.MAC_ID_PREFIX}${mac}`, true);
  }

  static async upsertMac (doc) {
    const value = doc.mac?.value;
    await Db.index(FeatherprintDb.INDEX, `${FeatherprintDb.MAC_ID_PREFIX}${value}`, doc);
    return value;
  }

  // --------------------------------------------------------------------------
  // Shared helper: realtime GET by doc id (each of these doc types is keyed
  // by a known id -- state id, ip, or mac). Returns _source (with _id when
  // includeId=true), or null on a missing doc / missing index (404).
  static async #getById (index, id, includeId = false) {
    try {
      const { body } = await Db.get(index, id);
      if (!body || body.found === false || !body._source) return null;
      return includeId ? { _id: body._id, ...body._source } : body._source;
    } catch (e) {
      if (e?.meta?.statusCode === 404) return null;
      throw e;
    }
  }
}

module.exports = FeatherprintDb;
