/* arkimeDb.sqlite.js  -- SQLite collections for ArkimeDb
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const ArkimeDb = require('./arkimeDb');

// Each collection is a table of (id, json[, version][, extra columns]).
// json fields are matched through json_each so arrays and scalars behave the same.
class SQLiteCollection extends ArkimeDb.Collection {
  #db;
  #table;
  #columns;
  #version;

  constructor (db, collName, options) {
    super(collName, options);
    this.#db = db;
    this.#table = ArkimeDb.checkField(options.sqlite?.table ?? collName);
    this.#columns = options.sqlite?.columns ?? {};
    this.#version = !!options.version;

    const cols = ['id TEXT PRIMARY KEY', 'json TEXT NOT NULL'];
    if (this.#version) { cols.push('version INTEGER DEFAULT 1'); }
    for (const [col, type] of Object.entries(this.#columns)) {
      cols.push(`${ArkimeDb.checkField(col)} ${type}`);
    }
    db.exec(`CREATE TABLE IF NOT EXISTS ${this.#table} (${cols.join(', ')})`);
    db.exec('CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT NOT NULL)');
  }

  async get (id) {
    const row = this.#db.prepare(`SELECT json FROM ${this.#table} WHERE id = ?`).get(id);
    return row ? JSON.parse(row.json) : null;
  }

  async put (id, doc) {
    id ??= this.newId();
    const cols = Object.keys(this.#columns);
    const names = ['id', 'json', ...cols];
    const updates = ['json = excluded.json', ...cols.map(c => `${c} = excluded.${c}`)];
    if (this.#version) { updates.push('version = version + 1'); }
    this.#db.prepare(`INSERT INTO ${this.#table} (${names.join(', ')}) VALUES (${names.map(() => '?').join(', ')})
      ON CONFLICT(id) DO UPDATE SET ${updates.join(', ')}`)
      .run(id, JSON.stringify(doc), ...cols.map(c => sqlValue(doc[c])));
    return id;
  }

  async delete (id) {
    return this.#db.prepare(`DELETE FROM ${this.#table} WHERE id = ?`).run(id).changes > 0;
  }

  async deleteWhere (where) {
    const w = this.#where(where);
    return this.#db.prepare(`DELETE FROM ${this.#table} ${w.sql}`).run(...w.params).changes;
  }

  async find ({ where, sort, from = 0, size, version } = {}) {
    const w = this.#where(where);
    const total = this.#db.prepare(`SELECT COUNT(*) AS total FROM ${this.#table} ${w.sql}`).get(...w.params).total;
    if (size === 0) { return { total, data: [] }; }

    const order = ArkimeDb.sortList(sort).map(([field, dir]) => {
      const expr = this.#expr(field);
      return `(${expr} IS NULL), ${expr} ${dir.toUpperCase()}`;
    });
    const sql = `SELECT id, json${this.#version ? ', version' : ''} FROM ${this.#table} ${w.sql}` +
      (order.length ? ` ORDER BY ${order.join(', ')}` : '') + ' LIMIT ? OFFSET ?';
    const rows = this.#db.prepare(sql).all(...w.params, size ?? -1, from);

    return {
      total,
      data: rows.map(r => {
        const hit = { id: r.id, doc: JSON.parse(r.json) };
        if (version && this.#version) { hit.version = r.version; }
        return hit;
      })
    };
  }

  async count (where) {
    const w = this.#where(where);
    return this.#db.prepare(`SELECT COUNT(*) AS total FROM ${this.#table} ${w.sql}`).get(...w.params).total;
  }

  async getMeta (key) {
    const row = this.#db.prepare('SELECT value FROM meta WHERE key = ?').get(`${this.#table}:${key}`);
    return row ? JSON.parse(row.value) : undefined;
  }

  async setMeta (obj) {
    const stmt = this.#db.prepare('INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)');
    for (const [key, value] of Object.entries(obj)) {
      stmt.run(`${this.#table}:${key}`, JSON.stringify(value));
    }
  }

  // --------------------------------------------------------------------------
  #expr (field) {
    if (field === 'id' || this.#columns[field]) { return field; }
    return `json_extract(json, '$.${field}')`;
  }

  #where (where) {
    const params = [];
    const sql = this.#compile(where, params);
    return { sql: sql === '1' ? '' : `WHERE ${sql}`, params };
  }

  #compile (where, params) {
    if (!where) { return '1'; }
    const parts = [];
    for (const [key, cond] of Object.entries(where)) {
      if (key === '$or') {
        parts.push(cond.length ? `(${cond.map(w => this.#compile(w, params)).join(' OR ')})` : '0');
      } else if (key === '$and') {
        parts.push(cond.length ? `(${cond.map(w => this.#compile(w, params)).join(' AND ')})` : '1');
      } else if (key === '$not') {
        parts.push(`NOT (${this.#compile(cond, params)})`);
      } else {
        ArkimeDb.checkField(key);
        for (const [op, c] of ArkimeDb.condOps(cond)) {
          parts.push(this.#compileOp(key, op, c, params));
        }
      }
    }
    return parts.length ? parts.join(' AND ') : '1';
  }

  // Every op is "some value of the field matches", except the negated ones
  #compileOp (field, op, c, params) {
    let negate = false;
    if (op === '$ne') { op = '$eq'; negate = true; }
    if (op === '$nin') { op = '$in'; negate = true; }

    let test;
    switch (op) {
    case '$eq':
      test = 'value = ?';
      params.push(sqlValue(c));
      break;
    case '$in':
      if (c.length === 0) { return negate ? '1' : '0'; }
      test = `value IN (${c.map(() => '?').join(', ')})`;
      params.push(...c.map(sqlValue));
      break;
    case '$exists':
      test = 'value IS NOT NULL';
      negate = !c;
      break;
    case '$gt': case '$gte': case '$lt': case '$lte':
      test = `type IN (${typeof c === 'number' ? "'integer', 'real'" : "'text'"}) AND value ${{ $gt: '>', $gte: '>=', $lt: '<', $lte: '<=' }[op]} ?`;
      params.push(sqlValue(c));
      break;
    case '$contains':
      test = "type = 'text' AND instr(value, ?) > 0";
      params.push(String(c));
      break;
    case '$icontains':
      test = "type = 'text' AND instr(lower(value), lower(?)) > 0";
      params.push(String(c));
      break;
    case '$prefix':
      test = "type = 'text' AND substr(value, 1, length(?)) = ?";
      params.push(String(c), String(c));
      break;
    }

    let source;
    if (field === 'id' || this.#columns[field]) {
      source = `(SELECT ${this.#table}.${field} AS value, typeof(${this.#table}.${field}) AS type)`;
    } else {
      source = `json_each(${this.#table}.json, '$.${field}')`;
    }
    const sql = `EXISTS (SELECT 1 FROM ${source} WHERE value IS NOT NULL AND ${test})`;
    return negate ? `NOT ${sql}` : sql;
  }
}

function sqlValue (v) {
  if (v === true) { return 1; }
  if (v === false) { return 0; }
  return v ?? null;
}

class SQLiteConnection {
  db;
  #collections = new Map();

  constructor (url, options) {
    this.db = options.db ?? ArkimeDb.createSQLiteDB(url);
  }

  collection (collName, options = {}) {
    let coll = this.#collections.get(collName);
    if (!coll) {
      coll = new SQLiteCollection(this.db, collName, options);
      this.#collections.set(collName, coll);
    }
    return coll;
  }

  async close () {
    this.db.close();
  }
}

module.exports = SQLiteConnection;
