/******************************************************************************/
/* db.sqlite.js -- SQLite implementation of the viewer DB backend
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';

const { v4: uuidv4 } = require('uuid');

class DbSQLiteImpl {
  #db;

  constructor (db) {
    this.#db = db;

    this.#db.exec(`CREATE TABLE IF NOT EXISTS views (
      id TEXT PRIMARY KEY,
      json TEXT NOT NULL
    )`);

    this.#db.exec(`CREATE TABLE IF NOT EXISTS shareables (
      id TEXT PRIMARY KEY,
      json TEXT NOT NULL
    )`);
  }

  // --------------------------------------------------------------------------
  // VIEWS
  // --------------------------------------------------------------------------
  async searchViews (params) {
    const rows = this.#db.prepare('SELECT id, json FROM views').all();
    let hits = rows.map(r => ({ id: r.id, source: JSON.parse(r.json) }));

    hits = this.#filterViews(hits, params);
    const total = hits.length;

    this.#sortHits(hits, params.sortField || 'name', params.sortOrder || 'asc');

    const from = params.from || 0;
    const size = params.size || 50;
    hits = hits.slice(from, from + size);

    return { data: hits, total };
  }

  async numberOfViews (params) {
    const rows = this.#db.prepare('SELECT id, json FROM views').all();
    let hits = rows.map(r => ({ id: r.id, source: JSON.parse(r.json) }));
    hits = this.#filterViews(hits, params);
    return hits.length;
  }

  async getView (id) {
    const row = this.#db.prepare('SELECT json FROM views WHERE id = ?').get(id);
    if (!row) { throw new Error('View not found'); }
    return JSON.parse(row.json);
  }

  async getViewByIdOrName (idOrName, user, roles) {
    // Try by id first
    let row = this.#db.prepare('SELECT id, json FROM views WHERE id = ?').get(idOrName);
    if (!row) {
      // Try by name - scan all and match
      const rows = this.#db.prepare('SELECT id, json FROM views').all();
      for (const r of rows) {
        const doc = JSON.parse(r.json);
        if (doc.name === idOrName) {
          row = r;
          break;
        }
      }
    }
    if (!row) { return null; }

    const source = typeof row.json === 'string' ? JSON.parse(row.json) : row.json;

    // Check permissions
    if (source.user === user) { return source; }
    if (source.users?.includes(user)) { return source; }
    if (source.roles?.some(r => roles.includes(r))) { return source; }

    return null;
  }

  async createView (doc) {
    const id = uuidv4();
    this.#db.prepare('INSERT INTO views (id, json) VALUES (?, ?)').run(id, JSON.stringify(doc));
    return id;
  }

  async deleteView (id) {
    this.#db.prepare('DELETE FROM views WHERE id = ?').run(id);
  }

  async deleteAllViews () {
    this.#db.prepare('DELETE FROM views').run();
  }

  async setView (id, doc) {
    this.#db.prepare('INSERT OR REPLACE INTO views (id, json) VALUES (?, ?)').run(id, JSON.stringify(doc));
  }

  // --------------------------------------------------------------------------
  // SHAREABLES
  // --------------------------------------------------------------------------
  async searchShareables (params) {
    const rows = this.#db.prepare('SELECT id, json FROM shareables').all();
    let hits = rows.map(r => ({ id: r.id, source: JSON.parse(r.json) }));

    hits = this.#filterShareables(hits, params);
    const total = hits.length;

    // Sort by name ascending
    this.#sortHits(hits, 'name', 'asc');

    const from = params.from || 0;
    const size = params.size || 50;
    hits = hits.slice(from, from + size);

    return { data: hits, total };
  }

  async numberOfShareables (params) {
    const rows = this.#db.prepare('SELECT id, json FROM shareables').all();
    let hits = rows.map(r => ({ id: r.id, source: JSON.parse(r.json) }));
    hits = this.#filterShareables(hits, params);
    return hits.length;
  }

  async getShareable (id) {
    const row = this.#db.prepare('SELECT json FROM shareables WHERE id = ?').get(id);
    if (!row) { throw new Error('Shareable not found'); }
    return JSON.parse(row.json);
  }

  async createShareable (doc) {
    const id = uuidv4();
    this.#db.prepare('INSERT INTO shareables (id, json) VALUES (?, ?)').run(id, JSON.stringify(doc));
    return id;
  }

  async deleteShareable (id) {
    this.#db.prepare('DELETE FROM shareables WHERE id = ?').run(id);
  }

  async setShareable (id, doc) {
    this.#db.prepare('INSERT OR REPLACE INTO shareables (id, json) VALUES (?, ?)').run(id, JSON.stringify(doc));
  }

  // --------------------------------------------------------------------------
  // Flush/Refresh — no-ops for SQLite
  // --------------------------------------------------------------------------
  async flush () {}
  async refresh () {}

  // --------------------------------------------------------------------------
  // PRIVATE HELPERS
  // --------------------------------------------------------------------------
  #filterViews (hits, params) {
    if (params.all) {
      // Admin override — no permission filtering
    } else {
      hits = hits.filter(h => {
        const s = h.source;
        if (s.user === params.user) { return true; }
        if (s.users?.includes(params.user)) { return true; }
        if (s.roles?.some(r => params.roles?.includes(r))) { return true; }
        if (s.editRoles?.some(r => params.roles?.includes(r))) { return true; }
        return false;
      });
    }

    if (params.searchTerm) {
      const term = params.searchTerm.toLowerCase();
      hits = hits.filter(h => h.source.name?.toLowerCase().includes(term));
    }

    return hits;
  }

  #filterShareables (hits, params) {
    // Filter by type
    hits = hits.filter(h => h.source.type === params.type);

    // Filter by permissions
    hits = hits.filter(h => {
      const s = h.source;
      if (s.creator === params.user) { return true; }

      if (params.viewOnly) {
        if (s.viewUsers?.includes(params.user)) { return true; }
        if (s.viewRoles?.some(r => params.roles?.includes(r))) { return true; }
      } else {
        if (s.editUsers?.includes(params.user)) { return true; }
        if (s.editRoles?.some(r => params.roles?.includes(r))) { return true; }
        if (s.viewUsers?.includes(params.user)) { return true; }
        if (s.viewRoles?.some(r => params.roles?.includes(r))) { return true; }
      }

      return false;
    });

    return hits;
  }

  #sortHits (hits, field, order) {
    hits.sort((a, b) => {
      const va = a.source[field] ?? '';
      const vb = b.source[field] ?? '';
      if (va < vb) { return order === 'asc' ? -1 : 1; }
      if (va > vb) { return order === 'asc' ? 1 : -1; }
      return 0;
    });
  }
}

module.exports = DbSQLiteImpl;
