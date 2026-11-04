/******************************************************************************/
/* featherprint.db.js -- cluster db persistence for featherprint device records,
 *                       history, alerts and MAC-keyed tracking. On ES these are
 *                       the `featherprint`, `featherprint_history` and
 *                       `featherprint_alerts` aliases db.pl creates. The devices
 *                       collection holds several doc kinds keyed by id: the device
 *                       record (id = ip), monitor state (`__state__`) and MAC
 *                       tracking (`mac:<addr>`), leaving room for more kinds later.
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const nodeCrypto = require('crypto');
const Db = require('./db.js');
const ArkimeDb = require('../common/arkimeDb');

class FeatherprintDb {
  static MAC_ID_PREFIX = 'mac:';
  static STATE_ID = '__state__';

  static #devices () { return Db.featherprint().devices; }
  static #history () { return Db.featherprint().history; }
  static #alerts () { return Db.featherprint().alerts; }

  // --------------------------------------------------------------------------
  // Persisted monitor state (single doc, well-known id). Tracks the
  // last-processed @timestamp so restarts resume instead of jumping forward.
  static async getState () {
    return FeatherprintDb.#devices().get(FeatherprintDb.STATE_ID);
  }

  static async saveState (state) {
    const doc = { kind: 'state', ...state, lastUpdated: Date.now() };
    await FeatherprintDb.#devices().put(FeatherprintDb.STATE_ID, doc, { refresh: false });
    return doc;
  }

  // --------------------------------------------------------------------------
  // Get a single device record by IP. Returns the doc (with _id) or null.
  // A get by id reads its own writes, so writes don't force a refresh.
  static async getIp (ip) {
    return FeatherprintDb.#getById(FeatherprintDb.#devices(), ip);
  }

  // Upsert a device record. Used by monitor mode.
  static async upsertDevice (doc) {
    await FeatherprintDb.#devices().put(doc.ip, doc, { refresh: false });
    return doc.ip;
  }

  // Search devices. ip is an exact IP or CIDR, mac/name are substring matches.
  static async searchDevices ({ ip, mac, name: nameQ, limit = 200 } = {}) {
    // Requiring an ip leaves out the state and mac docs
    const where = { ip: ip ? { $cidr: ip } : { $exists: true } };
    if (mac) where['mac.value'] = { $contains: mac.toLowerCase() };
    if (nameQ) where['names.name'] = { $contains: nameQ };

    const { data } = await FeatherprintDb.#devices().find({ where, sort: { lastSeen: 'desc' }, size: limit });
    return data.map(h => ({ _id: h.id, ...h.doc }));
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
    const id = `${entry.ip}|${entry.kind}|${entry.ts}|${FeatherprintDb.#digest(entry)}`;
    await FeatherprintDb.#history().put(id, entry, { refresh: false });
    return id;
  }

  static async listHistory (ip, limit = 50) {
    const { data } = await FeatherprintDb.#history().find({ where: { ip: { $cidr: ip } }, sort: { ts: 'desc' }, size: limit });
    return data.map(h => ({ _id: h.id, ...h.doc }));
  }

  // --------------------------------------------------------------------------
  // Alerts: insert, list with optional ack filter, ack.
  // Deterministic id from {ip, kind, ts, content} -- same scheme as history --
  // so re-processing a window (failed saveState, restart, cron-leader change)
  // collapses onto one alert instead of re-firing. Create-only: if the id
  // already exists we leave it untouched, so a replay never resurrects an
  // already-acked alert.
  static async insertAlert (alertDoc) {
    const id = `${alertDoc.ip}|${alertDoc.kind}|${alertDoc.ts}|${FeatherprintDb.#digest(alertDoc)}`;
    try {
      await FeatherprintDb.#alerts().put(id, alertDoc, { createOnly: true });
    } catch (e) {
      if (e instanceof ArkimeDb.ConflictError) return { id, created: false };
      throw e;
    }
    return { id, created: true };
  }

  static async listAlerts ({ acked, limit = 500 } = {}) {
    const where = acked === undefined ? null : { acked };
    const { data } = await FeatherprintDb.#alerts().find({ where, sort: { ts: 'desc' }, size: limit });
    return data.map(h => ({ _id: h.id, ...h.doc }));
  }

  // Ack state is shared across the team, not per user -- ackedBy/ackedAt are
  // the audit trail of who triaged it. Un-acking clears both so a mistaken ack
  // leaves no misleading attribution behind.
  static #ackFields (acked, user, ackedAt) {
    return acked
      ? { acked: true, ackedBy: user, ackedAt }
      : { acked: false, ackedBy: null, ackedAt: null };
  }

  static async #ack (id, fields, refresh) {
    if (!await FeatherprintDb.#alerts().update(id, fields, { refresh })) {
      throw new Error(`Alert ${id} not found`);
    }
  }

  static async ackAlert (id, user, ackedAt = Date.now(), acked = true) {
    await FeatherprintDb.#ack(id, FeatherprintDb.#ackFields(acked, user, ackedAt), true);
  }

  // Bulk ack/un-ack. Refreshes once at the end instead of per doc -- a few
  // hundred refresh:true updates stall the index. Runs in small batches so a
  // big ack doesn't open hundreds of concurrent requests.
  static async ackAlerts (ids, user, ackedAt = Date.now(), acked = true) {
    const fields = FeatherprintDb.#ackFields(acked, user, ackedAt);
    let changed = 0;
    let lastError;
    for (let i = 0; i < ids.length; i += 10) {
      const batch = await Promise.allSettled(ids.slice(i, i + 10).map(id => FeatherprintDb.#ack(id, fields, false)));
      for (const r of batch) {
        if (r.status === 'fulfilled') { changed++; } else { lastError = r.reason; }
      }
    }
    await FeatherprintDb.#alerts().refresh();
    if (changed === 0 && lastError) { throw lastError; }
    return { acked: changed, failed: ids.length - changed };
  }

  // --------------------------------------------------------------------------
  // MAC-keyed tracking: one doc per MAC, with current IP and history so we
  // can emit changeIp when a known MAC shows up on a new IP (arpwatch-style).
  static async getMac (mac) {
    return FeatherprintDb.#getById(FeatherprintDb.#devices(), `${FeatherprintDb.MAC_ID_PREFIX}${mac}`);
  }

  static async upsertMac (doc) {
    const value = doc.mac?.value;
    await FeatherprintDb.#devices().put(`${FeatherprintDb.MAC_ID_PREFIX}${value}`, doc, { refresh: false });
    return value;
  }

  // --------------------------------------------------------------------------
  static async #getById (coll, id) {
    const doc = await coll.get(id);
    return doc ? { _id: id, ...doc } : null;
  }

  static #digest (doc) {
    return nodeCrypto.createHash('sha1')
      .update(JSON.stringify({ kind: doc.kind, before: doc.before ?? null, after: doc.after ?? null }))
      .digest('hex').slice(0, 16);
  }
}

module.exports = FeatherprintDb;
