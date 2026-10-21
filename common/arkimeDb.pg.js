/* arkimeDb.pg.js  -- Postgres collections for ArkimeDb
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const ArkimeDb = require('./arkimeDb');

// Each collection is a table of (id, doc jsonb, version). Fields are matched
// against their values as a set, so arrays and scalars behave the same.
class PGCollection extends ArkimeDb.Collection {
  #pool;
  #table;
  #ready;

  constructor (pool, prefix, collName, options) {
    super(collName, options);
    this.#pool = pool;
    this.#table = ArkimeDb.checkField(options.pg?.table ?? `${prefix}${options.table ?? collName}`);
  }

  init () {
    this.#ready ??= this.#pool.query(`
      CREATE TABLE IF NOT EXISTS ${this.#table} (id TEXT PRIMARY KEY, doc JSONB NOT NULL, version INTEGER NOT NULL DEFAULT 1);
      CREATE TABLE IF NOT EXISTS arkime_meta (key TEXT PRIMARY KEY, value JSONB NOT NULL)
    `);
    return this.#ready;
  }

  async #query (sql, params) {
    await this.init();
    return this.#pool.query(sql, params);
  }

  async get (id) {
    const { rows } = await this.#query(`SELECT doc FROM ${this.#table} WHERE id = $1`, [id]);
    return rows[0]?.doc ?? null;
  }

  async put (id, doc, { createOnly } = {}) {
    id ??= this.newId();
    const onConflict = createOnly ? 'DO NOTHING' : `DO UPDATE SET doc = excluded.doc, version = ${this.#table}.version + 1`;
    const { rowCount } = await this.#query(`INSERT INTO ${this.#table} (id, doc) VALUES ($1, $2) ON CONFLICT (id) ${onConflict}`,
      [id, JSON.stringify(doc)]);
    if (createOnly && rowCount === 0) { throw new ArkimeDb.ConflictError(id); }
    return id;
  }

  async update (id, fields) {
    const { rowCount } = await this.#query(`UPDATE ${this.#table} SET doc = doc || $1::jsonb, version = version + 1 WHERE id = $2`,
      [JSON.stringify(fields), id]);
    return rowCount > 0;
  }

  async delete (id) {
    const { rowCount } = await this.#query(`DELETE FROM ${this.#table} WHERE id = $1`, [id]);
    return rowCount > 0;
  }

  async deleteWhere (where) {
    const params = [];
    const { rowCount } = await this.#query(`DELETE FROM ${this.#table} WHERE ${compile(where, params)}`, params);
    return rowCount;
  }

  async find ({ where, sort, from = 0, size, version } = {}) {
    const params = [];
    const w = compile(where, params);
    const { rows: [{ total }] } = await this.#query(`SELECT COUNT(*)::int AS total FROM ${this.#table} WHERE ${w}`, params);
    if (size === 0) { return { total, data: [] }; }

    const order = [];
    for (const [field, dir] of ArkimeDb.sortList(sort)) {
      if (field === 'id') {
        order.push(`id COLLATE "C" ${dir}`);
        continue;
      }
      const v = `(doc #> ${pathLiteral(field)})`;
      order.push(`(${v} IS NULL OR jsonb_typeof(${v}) = 'null')`,
        `CASE WHEN jsonb_typeof(${v}) = 'number' THEN (${v} #>> '{}')::numeric END ${dir}`,
        `(${v} #>> '{}') COLLATE "C" ${dir}`);
    }

    params.push(size ?? null, from);
    const { rows } = await this.#query(`SELECT id, doc, version FROM ${this.#table} WHERE ${w}` +
      (order.length ? ` ORDER BY ${order.join(', ')}` : '') +
      ` LIMIT $${params.length - 1} OFFSET $${params.length}`, params);

    return {
      total,
      data: rows.map(r => {
        const hit = { id: r.id, doc: r.doc };
        if (version && this.options.version) { hit.version = r.version; }
        return hit;
      })
    };
  }

  async count (where) {
    const params = [];
    const { rows } = await this.#query(`SELECT COUNT(*)::int AS total FROM ${this.#table} WHERE ${compile(where, params)}`, params);
    return rows[0].total;
  }

  async getMeta (key) {
    const { rows } = await this.#query('SELECT value FROM arkime_meta WHERE key = $1', [`${this.#table}:${key}`]);
    return rows[0]?.value;
  }

  async setMeta (obj) {
    for (const [key, value] of Object.entries(obj)) {
      await this.#query(`INSERT INTO arkime_meta (key, value) VALUES ($1, $2)
        ON CONFLICT (key) DO UPDATE SET value = excluded.value`, [`${this.#table}:${key}`, JSON.stringify(value)]);
    }
  }
}

function pathLiteral (field) {
  return `'{${field.split('.').join(',')}}'`;
}

// The values of a field as a set of jsonb rows, [] when missing or null
function valuesSource (field) {
  const v = `(doc #> ${pathLiteral(field)})`;
  return `jsonb_array_elements(CASE WHEN jsonb_typeof(${v}) = 'array' THEN ${v} ` +
    `WHEN ${v} IS NULL OR jsonb_typeof(${v}) = 'null' THEN '[]'::jsonb ELSE jsonb_build_array(${v}) END) AS e(value)`;
}

function compileOp (field, op, c, params) {
  const p = (v) => { params.push(v); return `$${params.length}`; };

  if (field === 'id') {
    switch (op) {
    case '$eq': return `id = ${p(c)}`;
    case '$ne': return `id <> ${p(c)}`;
    case '$in': return `id = ANY(${p(c)}::text[])`;
    case '$nin': return `NOT (id = ANY(${p(c)}::text[]))`;
    case '$exists': return c ? 'TRUE' : 'FALSE';
    case '$gt': return `id COLLATE "C" > ${p(c)}`;
    case '$gte': return `id COLLATE "C" >= ${p(c)}`;
    case '$lt': return `id COLLATE "C" < ${p(c)}`;
    case '$lte': return `id COLLATE "C" <= ${p(c)}`;
    case '$contains': return `strpos(id, ${p(String(c))}) > 0`;
    case '$icontains': return `strpos(lower(id), lower(${p(String(c))})) > 0`;
    case '$prefix': return `left(id, length(${p(String(c))})) = $${params.length}`;
    }
  }

  let negate = false;
  if (op === '$ne') { op = '$eq'; negate = true; }
  if (op === '$nin') { op = '$in'; negate = true; }

  const text = "(value #>> '{}')";
  let test;
  switch (op) {
  case '$eq':
    test = `value = ${p(JSON.stringify(c))}::jsonb`;
    break;
  case '$in':
    test = `value = ANY(${p(c.map(v => JSON.stringify(v)))}::jsonb[])`;
    break;
  case '$exists':
    test = 'TRUE';
    negate = !c;
    break;
  case '$gt': case '$gte': case '$lt': case '$lte': {
    const cmp = { $gt: '>', $gte: '>=', $lt: '<', $lte: '<=' }[op];
    test = typeof c === 'number'
      ? `jsonb_typeof(value) = 'number' AND ${text}::numeric ${cmp} ${p(c)}`
      : `jsonb_typeof(value) = 'string' AND ${text} COLLATE "C" ${cmp} ${p(String(c))}`;
    break;
  }
  case '$contains':
    test = `jsonb_typeof(value) = 'string' AND strpos(${text}, ${p(String(c))}) > 0`;
    break;
  case '$icontains':
    test = `jsonb_typeof(value) = 'string' AND strpos(lower(${text}), lower(${p(String(c))})) > 0`;
    break;
  case '$prefix':
    test = `jsonb_typeof(value) = 'string' AND left(${text}, length(${p(String(c))})) = $${params.length}`;
    break;
  }

  const sql = `EXISTS (SELECT 1 FROM ${valuesSource(field)} WHERE ${test})`;
  return negate ? `NOT ${sql}` : sql;
}

function compile (where, params) {
  if (!where) { return 'TRUE'; }
  const parts = [];
  for (const [key, cond] of Object.entries(where)) {
    if (key === '$or') {
      parts.push(cond.length ? `(${cond.map(w => compile(w, params)).join(' OR ')})` : 'FALSE');
    } else if (key === '$and') {
      parts.push(cond.length ? `(${cond.map(w => compile(w, params)).join(' AND ')})` : 'TRUE');
    } else if (key === '$not') {
      parts.push(`NOT (${compile(cond, params)})`);
    } else {
      ArkimeDb.checkField(key);
      for (const [op, c] of ArkimeDb.condOps(cond)) {
        parts.push(compileOp(key, op, c, params));
      }
    }
  }
  return parts.length ? parts.join(' AND ') : 'TRUE';
}

class PGConnection {
  pool;
  prefix;
  #closed = false;
  #collections = new Map();

  constructor (url, options) {
    const { Pool } = require('pg');
    this.prefix = options.prefix ?? '';
    this.pool = options.pool ?? new Pool({ connectionString: url.replace(/^postgres(ql)?:/, 'postgres:') });
  }

  collection (collName, options = {}) {
    let coll = this.#collections.get(collName);
    if (!coll) {
      coll = new PGCollection(this.pool, this.prefix, collName, options);
      this.#collections.set(collName, coll);
    }
    return coll;
  }

  async close () {
    if (this.#closed) { return; }
    this.#closed = true;
    await this.pool.end();
  }
}

module.exports = PGConnection;
