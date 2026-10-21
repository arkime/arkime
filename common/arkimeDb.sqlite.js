/* arkimeDb.sqlite.js  -- SQLite collections for ArkimeDb
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const ArkimeDb = require('./arkimeDb');

// Each collection is a table of (id, json[, version][, extra columns]), sqlite.idColumn
// renames the id column for older tables and sqlite.legacyMeta { key: oldMetaKey } moves their meta.
// json fields are matched through json_each so arrays and scalars behave the same.
class SQLiteCollection extends ArkimeDb.Collection {
  #db;
  #table;
  #idCol;
  #columns;
  #version;

  constructor (db, collName, options) {
    super(collName, options);
    this.#db = db;
    this.#table = ArkimeDb.checkField(options.sqlite?.table ?? options.table ?? collName);
    this.#idCol = ArkimeDb.checkField(options.sqlite?.idColumn ?? 'id');
    this.#columns = options.sqlite?.columns ?? {};
    this.#version = !!options.version;

    const cols = [`${this.#idCol} TEXT PRIMARY KEY`, 'json TEXT NOT NULL'];
    if (this.#version) { cols.push('version INTEGER DEFAULT 1'); }
    for (const [col, type] of Object.entries(this.#columns)) {
      cols.push(`${ArkimeDb.checkField(col)} ${type}`);
    }
    db.exec(`CREATE TABLE IF NOT EXISTS ${this.#table} (${cols.join(', ')})`);
    db.exec('CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT NOT NULL)');

    // Older layouts kept meta under their own key names, move those under <table>:<key>
    for (const [key, legacyKey] of Object.entries(options.sqlite?.legacyMeta ?? {})) {
      const row = db.prepare('SELECT value FROM meta WHERE key = ?').get(legacyKey);
      if (!row) { continue; }
      db.prepare('INSERT OR IGNORE INTO meta (key, value) VALUES (?, ?)').run(`${this.#table}:${key}`, row.value);
      db.prepare('DELETE FROM meta WHERE key = ?').run(legacyKey);
    }
  }

  async get (id) {
    const row = this.#db.prepare(`SELECT json FROM ${this.#table} WHERE ${this.#idCol} = ?`).get(id);
    return row ? JSON.parse(row.json) : null;
  }

  async put (id, doc, { createOnly } = {}) {
    id ??= this.newId();
    const cols = Object.keys(this.#columns);
    const names = [this.#idCol, 'json', ...cols];
    const updates = ['json = excluded.json', ...cols.map(c => `${c} = excluded.${c}`)];
    if (this.#version) { updates.push('version = version + 1'); }
    const upsert = createOnly ? '' : ` ON CONFLICT(${this.#idCol}) DO UPDATE SET ${updates.join(', ')}`;
    try {
      this.#db.prepare(`INSERT INTO ${this.#table} (${names.join(', ')}) VALUES (${names.map(() => '?').join(', ')})${upsert}`)
        .run(id, JSON.stringify(doc), ...cols.map(c => sqlValue(doc[c])));
    } catch (err) {
      if (createOnly && err.code === 'SQLITE_CONSTRAINT_PRIMARYKEY') { throw new ArkimeDb.ConflictError(id); }
      throw err;
    }
    return id;
  }

  async aggregate ({ where, groupBy, metrics = {}, size = 1000 } = {}) {
    ArkimeDb.checkMetrics(metrics);
    const t = this.#table;
    const w = this.#where(where);
    const keySource = groupBy === 'id' || this.#columns[groupBy]
      ? `(SELECT ${t}.${groupBy === 'id' ? this.#idCol : groupBy} AS value)`
      : `json_each(${t}.json, '$.${ArkimeDb.checkField(groupBy)}')`;
    const cols = Object.entries(metrics).map(([metricName, metric]) => {
      const [op, field] = Object.entries(metric)[0];
      const agg = `${op.toUpperCase()}(${this.#expr(field).replace(/^json_extract\(json/, `json_extract(${t}.json`)})`;
      return `${op === 'sum' ? `COALESCE(${agg}, 0)` : agg} AS m_${ArkimeDb.checkField(metricName)}`;
    });
    const rows = this.#db.prepare(`SELECT g.value AS key, COUNT(*) AS count${cols.map(c => ', ' + c).join('')}
      FROM ${t}, ${keySource} AS g ${w.sql ? w.sql + ' AND' : 'WHERE'} g.value IS NOT NULL
      GROUP BY g.value ORDER BY count DESC, g.value LIMIT ?`).all(...w.params, size);
    return rows.map(r => {
      const bucket = { key: r.key, count: r.count };
      for (const metricName of Object.keys(metrics)) { bucket[metricName] = r[`m_${metricName}`]; }
      return bucket;
    });
  }

  async nextSequence (id) {
    const t = this.#table;
    return this.#db.prepare(`INSERT INTO ${t} (${this.#idCol}, json) VALUES (?, '{"seq":1}')
      ON CONFLICT(${this.#idCol}) DO UPDATE SET json = json_set(${t}.json, '$.seq', COALESCE(json_extract(${t}.json, '$.seq'), 0) + 1)
      RETURNING json_extract(json, '$.seq') AS seq`).get(id).seq;
  }

  async lease (id, owner, ttlMs) {
    const now = Date.now();
    const t = this.#table;
    return this.#db.prepare(`INSERT INTO ${t} (${this.#idCol}, json) VALUES (?, ?)
      ON CONFLICT(${this.#idCol}) DO UPDATE SET json = json_set(${t}.json, '$.name', ?, '$.lastRun', ?)
      WHERE json_extract(${t}.json, '$.name') = ? OR ? - json_extract(${t}.json, '$.lastRun') >= ?`)
      .run(id, JSON.stringify({ name: owner, lastRun: now, enabled: false }), owner, now, owner, now, ttlMs).changes > 0;
  }

  // Top level fields replace, json_patch would drop nulls and merge nested objects
  async update (id, fields) {
    const cols = Object.keys(this.#columns).filter(c => c in fields);
    const sets = ['json = ?', ...cols.map(c => `${c} = ?`)];
    if (this.#version) { sets.push('version = version + 1'); }
    return this.#db.transaction(() => {
      const row = this.#db.prepare(`SELECT json FROM ${this.#table} WHERE ${this.#idCol} = ?`).get(id);
      if (!row) { return false; }
      const doc = Object.assign(JSON.parse(row.json), fields);
      this.#db.prepare(`UPDATE ${this.#table} SET ${sets.join(', ')} WHERE ${this.#idCol} = ?`)
        .run(JSON.stringify(doc), ...cols.map(c => sqlValue(fields[c])), id);
      return true;
    })();
  }

  async delete (id) {
    return this.#db.prepare(`DELETE FROM ${this.#table} WHERE ${this.#idCol} = ?`).run(id).changes > 0;
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
    const sql = `SELECT ${this.#idCol} AS id, json${this.#version ? ', version' : ''} FROM ${this.#table} ${w.sql}` +
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
    if (field === 'id') { return this.#idCol; }
    if (this.#columns[field]) { return field; }
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
    case '$regex':
      test = "type = 'text' AND arkime_regexp(?, value)";
      params.push(String(c));
      break;
    case '$wildcard':
      // GLOB is case sensitive with the same * and ?, [ starts a class so match it literally
      test = "type = 'text' AND value GLOB ?";
      params.push(String(c).replace(/\[/g, '[[]'));
      break;
    }

    let source;
    if (field === 'id' || this.#columns[field]) {
      const col = `${this.#table}.${field === 'id' ? this.#idCol : field}`;
      source = `(SELECT ${col} AS value, typeof(${col}) AS type)`;
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
    this.db.function('arkime_regexp', { deterministic: true }, (pattern, value) => ArkimeDb.regex(pattern).test(value) ? 1 : 0);
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
    if (this.db.open) { this.db.close(); }
  }
}

module.exports = SQLiteConnection;
