/* arkimeDb.pg.js  -- Postgres collections for ArkimeDb
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const ArkimeDb = require('./arkimeDb');
const ArkimeConfig = require('./arkimeConfig');
const ArkimeUtil = require('./arkimeUtil');

// Each collection is a table of (id, doc jsonb, version). Fields are matched
// against their values as a set, so arrays and scalars behave the same,
// indexed fields are stored generated jsonb columns with an index and compare directly.
class PGCollection extends ArkimeDb.Collection {
  #pool;
  #table;
  #indexed;
  #numericIndexed;
  #sortIndexed;
  #meta;
  #inet;
  #ready;
  #op = (field, o, c, params) => compileOp(field, o, c, params, this.#indexed, this.#inet);

  constructor (pool, prefix, collName, options) {
    super(collName, options);
    this.#pool = pool;
    this.#table = ArkimeDb.checkField(options.pg?.table ?? `${prefix}${options.table ?? collName}`);
    // Per prefix so clusters sharing a database never run or write each other's
    this.#meta = ArkimeDb.checkField(`${prefix}arkime_meta`);
    this.#inet = ArkimeDb.checkField(`${prefix}arkime_inet`);
    this.#indexed = new Map(ArkimeDb.indexedFields(options).map(f => [f, ArkimeDb.fieldColumn(f)]));
    this.#numericIndexed = new Set(ArkimeDb.numericIndexedFields(options));
    this.#sortIndexed = new Set(ArkimeDb.sortIndexedFields(options));
  }

  // IF NOT EXISTS still races when several processes create the same table at
  // once, so the loser's duplicate errors mean it exists now
  init () {
    this.#ready ??= (async () => {
      const t = this.#table;
      const sqls = [
        `CREATE TABLE IF NOT EXISTS ${t} (id TEXT PRIMARY KEY, doc JSONB NOT NULL, version INTEGER NOT NULL DEFAULT 1)`,
        metaTable(this.#meta),
        inetFunction(this.#inet)
      ];
      for (const [field, col] of this.#indexed) {
        sqls.push(`ALTER TABLE ${t} ADD COLUMN IF NOT EXISTS ${col} JSONB GENERATED ALWAYS AS (NULLIF(doc #> ${pathLiteral(field)}, 'null'::jsonb)) STORED`,
          `CREATE INDEX IF NOT EXISTS ${t}_${col} ON ${t} (${col})`);
        // An ascending index only reads backwards as DESC NULLS FIRST
        if (this.#sortIndexed.has(field)) {
          sqls.push(`CREATE INDEX IF NOT EXISTS ${t}_${col}_desc ON ${t} (${col} DESC NULLS LAST)`);
        }
      }
      for (const sql of sqls) {
        await this.#create(sql);
      }

      // pg_trgm lets *wildcard* searches use an index, without it they scan
      const wildcard = ArkimeDb.wildcardIndexedFields(this.options);
      if (wildcard.length) {
        try {
          await this.#create('CREATE EXTENSION IF NOT EXISTS pg_trgm');
          for (const field of wildcard) {
            const col = this.#indexed.get(field);
            await this.#create(`CREATE INDEX IF NOT EXISTS ${t}_${col}_trgm ON ${t} USING GIN ((${col} #>> '{}') gin_trgm_ops)`);
          }
        } catch (err) {
          console.log(`WARNING - no pg_trgm index on ${t}, wildcard searches of ${wildcard.join(', ')} will scan it -`, err.message);
        }
      }
    })().catch((err) => {
      this.#ready = undefined; // the next call retries, a database that was down isn't a failure for good
      throw err;
    });
    return this.#ready;
  }

  async #create (sql) {
    try {
      await this.#pool.query(sql);
    } catch (err) {
      if (!['23505', '42P07', '42710', '42701', '42723'].includes(err.code)) { throw err; }
    }
  }

  async #query (sql, params) {
    await this.init();
    return params?.[PATTERNS] ? patternQuery(this.#pool, sql, params) : this.#pool.query(sql, params);
  }

  async get (id) {
    const { rows } = await this.#query(`SELECT doc FROM ${this.#table} WHERE id = $1`, [id]);
    return rows[0]?.doc ?? null;
  }

  async put (id, doc, { createOnly } = {}) {
    id ??= this.newId();
    const onConflict = createOnly ? 'DO NOTHING' : `DO UPDATE SET doc = excluded.doc, version = ${this.#table}.version + 1`;
    const { rowCount } = await this.#query(`INSERT INTO ${this.#table} (id, doc) VALUES ($1, $2) ON CONFLICT (id) ${onConflict}`,
      [id, jsonParam(doc)]);
    if (createOnly && rowCount === 0) { throw new ArkimeDb.ConflictError(id); }
    return id;
  }

  async getVersioned (id) {
    const { rows } = await this.#query(`SELECT doc, version FROM ${this.#table} WHERE id = $1`, [id]);
    return rows[0] ? { doc: rows[0].doc, version: rows[0].version } : null;
  }

  async putIfVersion (id, doc, version) {
    const { rowCount } = await this.#query(`UPDATE ${this.#table} SET doc = $1, version = version + 1 WHERE id = $2 AND version = $3`,
      [jsonParam(doc), id, version]);
    if (rowCount !== 1) { throw new ArkimeDb.ConflictError(id, `Document '${id}' was changed or deleted`); }
  }

  async aggregate ({ where, groupBy, metrics = {}, size = 1000 } = {}) {
    ArkimeDb.checkMetrics(metrics);
    const params = [];
    const w = compile(where, params, this.#op);
    // The id groups straight off the table, json fields join their values so every array
    // element counts. A FROM subquery can't see the table, so no (SELECT id) there.
    let from = this.#table;
    let key = `${this.#table}.id COLLATE "C"`;
    if (this.#indexed.has(ArkimeDb.checkField(groupBy))) {
      key = `${this.#table}.${this.#indexed.get(groupBy)}`;
    } else if (groupBy !== 'id') {
      from = `${this.#table}, ${valuesSource(groupBy, 'g')}`;
      key = 'g.value';
    }
    const cols = Object.entries(metrics).map(([metricName, metric]) => {
      const [op, field] = Object.entries(metric)[0];
      const v = this.#indexed.get(field) ?? `(doc #> ${pathLiteral(field)})`;
      const agg = `${op.toUpperCase()}(CASE WHEN jsonb_typeof(${v}) = 'number' THEN (${v} #>> '{}')::numeric END)`;
      return `${op === 'sum' ? `COALESCE(${agg}, 0)` : agg} AS "m_${ArkimeDb.checkField(metricName)}"`;
    });
    params.push(size);
    const { rows } = await this.#query(`SELECT ${key} AS key, COUNT(*)::int AS count${cols.map(c => ', ' + c).join('')}
      FROM ${from} WHERE ${w} AND ${key} IS NOT NULL
      GROUP BY ${key} ORDER BY count DESC, ${key} LIMIT $${params.length}`, params);
    return rows.map(r => {
      const bucket = { key: r.key, count: r.count };
      for (const metricName of Object.keys(metrics)) {
        const m = r[`m_${metricName}`];
        bucket[metricName] = m === null ? null : Number(m);
      }
      return bucket;
    });
  }

  async nextSequence (id) {
    const t = this.#table;
    const { rows } = await this.#query(`INSERT INTO ${t} (id, doc) VALUES ($1, '{"seq":1}')
      ON CONFLICT (id) DO UPDATE SET doc = jsonb_set(${t}.doc, '{seq}', to_jsonb(COALESCE((${t}.doc->>'seq')::bigint, 0) + 1))
      RETURNING (doc->>'seq')::bigint AS seq`, [id]);
    return Number(rows[0].seq);
  }

  async lease (id, owner, ttlMs) {
    const now = Date.now();
    const t = this.#table;
    const { rowCount } = await this.#query(`INSERT INTO ${t} (id, doc) VALUES ($1, $2)
      ON CONFLICT (id) DO UPDATE SET doc = ${t}.doc || jsonb_build_object('name', $3::text, 'lastRun', $4::bigint)
      WHERE ${t}.doc->>'name' = $3 OR $4 - (${t}.doc->>'lastRun')::bigint >= $5`,
    [id, jsonParam({ name: owner, lastRun: now, enabled: false }), owner, now, ttlMs]);
    return rowCount > 0;
  }

  async update (id, fields) {
    const { rowCount } = await this.#query(`UPDATE ${this.#table} SET doc = doc || $1::jsonb, version = version + 1 WHERE id = $2`,
      [jsonParam(fields), id]);
    return rowCount > 0;
  }

  async delete (id) {
    const { rowCount } = await this.#query(`DELETE FROM ${this.#table} WHERE id = $1`, [id]);
    return rowCount > 0;
  }

  async deleteWhere (where) {
    await this.init();
    const params = [];
    const { rowCount } = await this.#query(`DELETE FROM ${this.#table} WHERE ${compile(where, params, this.#op)}`, params);
    return rowCount;
  }

  async find ({ where, sort, from = 0, size, version } = {}) {
    const params = [];
    const w = compile(where, params, this.#op);
    const { rows: [{ total }] } = await this.#query(`SELECT COUNT(*)::int AS total FROM ${this.#table} WHERE ${w}`, params);
    if (size === 0) { return { total, data: [] }; }

    const order = [];
    for (const [field, dir] of ArkimeDb.sortList(sort)) {
      if (field === 'id') {
        order.push(`id COLLATE "C" ${dir}`);
        continue;
      }
      // jsonb numbers order numerically, so the column's index can do the sort
      if (this.#numericIndexed.has(field)) {
        order.push(`${this.#indexed.get(field)} ${dir} NULLS LAST`);
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
    const { rows } = await this.#query(`SELECT COUNT(*)::int AS total FROM ${this.#table} WHERE ${compile(where, params, this.#op)}`, params);
    return rows[0].total;
  }

  async getMeta (key) {
    const { rows } = await this.#query(`SELECT value FROM ${this.#meta} WHERE key = $1`, [`${this.#table}:${key}`]);
    return rows[0]?.value;
  }

  async setMeta (obj) {
    for (const [key, value] of Object.entries(obj)) {
      await this.#query(`INSERT INTO ${this.#meta} (key, value) VALUES ($1, $2)
        ON CONFLICT (key) DO UPDATE SET value = excluded.value`, [`${this.#table}:${key}`, jsonParam(value)]);
    }
  }
}

// jsonb can't hold a NUL or a lone surrogate, which would fail the whole write, JSON.stringify escapes both
function jsonParam (value) {
  // The character, not a \u escape, which a SQL_ASCII db refuses above 7F
  return JSON.stringify(value)?.replace(/(?<!\\)((?:\\\\)*)\\u(?:0000|d[89a-f][0-9a-f]{2})/gi, '$1\ufffd');
}

// * and ? to a LIKE pattern, LIKE's own % _ and \ are literal
function likePattern (c, params) {
  params[PATTERNS] = true;
  return String(c).replace(/[\\%_]/g, '\\$&').replace(/\*/g, '%').replace(/\?/g, '_');
}

// Postgres's regex engine only backtracks for a backreference, \digit. Lookarounds, (?flags), ***
// directors and \m \M \y \Y \Z are Postgres only. Refusing them all keeps a where matching
// what it does on the other dbs, and patternQuery bounds the rest.
const PG_ONLY_REGEX_RE = /^\*\*\*|(?<!\\)(?:\\\\)*(?:\\[\dmMyYZ]|\(\?(?!:))/;
function regexPattern (c, params) {
  const pattern = String(c);
  if (PG_ONLY_REGEX_RE.test(pattern)) {
    throw new Error(`Invalid db where $regex '${pattern}', backreferences, lookarounds, (?flags) and Postgres only escapes aren't supported`);
  }
  params[PATTERNS] = true;
  return pattern;
}

function jsonPath (field) {
  return `lax $${field.split('.').map(f => `."${f}"`).join('')}[*] ? (@ != null)`;
}

function metaTable (table) {
  return `CREATE TABLE IF NOT EXISTS ${table} (key TEXT PRIMARY KEY, value JSONB NOT NULL)`;
}

// arkime_inet is NULL for anything that isn't an ip, a plain cast would fail the whole query
function inetFunction (fn) {
  return `CREATE FUNCTION ${fn}(text) RETURNS inet LANGUAGE plpgsql IMMUTABLE AS
  'BEGIN RETURN $1::inet; EXCEPTION WHEN others THEN RETURN NULL; END'`;
}

function pathLiteral (field) {
  return `'{${field.split('.').join(',')}}'`;
}

// The values of a field as a set of jsonb rows, [] when missing or null. A dotted path can
// cross arrays of objects, which ES flattens, lax jsonpath unwraps them the same way.
function valuesSource (field, alias = 'e') {
  if (field.includes('.')) {
    return `jsonb_path_query(doc, '${jsonPath(field)}') AS ${alias}(value)`;
  }
  const v = `(doc #> ${pathLiteral(field)})`;
  return `jsonb_array_elements(CASE WHEN jsonb_typeof(${v}) = 'array' THEN ${v} ` +
    `WHEN ${v} IS NULL OR jsonb_typeof(${v}) = 'null' THEN '[]'::jsonb ELSE jsonb_build_array(${v}) END) AS ${alias}(value)`;
}

// The id and indexed fields hold one value so compare their column directly, which uses the index
function compileOp (field, op, c, params, indexed, inet) {
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
    case '$wildcard': return `id LIKE ${p(likePattern(c, params))} ESCAPE '\\'`;
    case '$regex': return `id ~ ('^(?:' || ${p(regexPattern(c, params))} || ')$')`;
    }
  }

  let negate = false;
  if (op === '$ne') { op = '$eq'; negate = true; }
  if (op === '$nin') { op = '$in'; negate = true; }

  const col = indexed.get(field);
  const value = col ?? 'value';
  const text = `(${value} #>> '{}')`;
  let test;
  switch (op) {
  case '$eq':
    test = `${value} = ${p(jsonParam(c))}::jsonb`;
    break;
  case '$in':
    test = `${value} = ANY(${p(c.map(v => jsonParam(v)))}::jsonb[])`;
    break;
  case '$exists':
    test = 'TRUE';
    negate = !c;
    break;
  case '$gt': case '$gte': case '$lt': case '$lte': {
    // jsonb numbers order numerically, which lets a column use its index
    const cmp = { $gt: '>', $gte: '>=', $lt: '<', $lte: '<=' }[op];
    test = typeof c === 'number'
      ? `jsonb_typeof(${value}) = 'number' AND ${value} ${cmp} ${p(JSON.stringify(c))}::jsonb`
      : `jsonb_typeof(${value}) = 'string' AND ${text} COLLATE "C" ${cmp} ${p(String(c))}`;
    break;
  }
  case '$contains':
    test = `jsonb_typeof(${value}) = 'string' AND strpos(${text}, ${p(String(c))}) > 0`;
    break;
  case '$icontains':
    test = `jsonb_typeof(${value}) = 'string' AND strpos(lower(${text}), lower(${p(String(c))})) > 0`;
    break;
  case '$prefix':
    test = `jsonb_typeof(${value}) = 'string' AND left(${text}, length(${p(String(c))})) = $${params.length}`;
    break;
  case '$wildcard':
    test = `jsonb_typeof(${value}) = 'string' AND ${text} LIKE ${p(likePattern(c, params))} ESCAPE '\\'`;
    break;
  case '$regex':
    test = `jsonb_typeof(${value}) = 'string' AND ${text} ~ ('^(?:' || ${p(regexPattern(c, params))} || ')$')`;
    break;
  case '$cidr':
    test = `jsonb_typeof(${value}) = 'string' AND ${inet}(${text}) <<= ${p(String(c))}::inet`;
    break;
  }

  if (col) {
    const direct = `(${col} IS NOT NULL AND ${test})`;
    return negate ? `NOT COALESCE(${direct}, FALSE)` : direct;
  }
  const sql = `EXISTS (SELECT 1 FROM ${valuesSource(field)} WHERE ${test})`;
  return negate ? `NOT ${sql}` : sql;
}

// op compiles one field condition, so json and typed collections share the boolean structure
function compile (where, params, op) {
  if (!where) { return 'TRUE'; }
  const parts = [];
  for (const [key, cond] of Object.entries(where)) {
    if (key === '$or') {
      parts.push(cond.length ? `(${cond.map(w => compile(w, params, op)).join(' OR ')})` : 'FALSE');
    } else if (key === '$and') {
      parts.push(cond.length ? `(${cond.map(w => compile(w, params, op)).join(' AND ')})` : 'TRUE');
    } else if (key === '$not') {
      parts.push(`NOT (${compile(cond, params, op)})`);
    } else {
      ArkimeDb.checkField(key);
      for (const [o, c] of ArkimeDb.condOps(cond)) {
        parts.push(op(key, o, c, params));
      }
    }
  }
  return parts.length ? parts.join(' AND ') : 'TRUE';
}

// ES mapping types to the column a typed collection gives them
const PG_TYPES = {
  long: 'bigint', date: 'bigint', integer: 'integer', short: 'smallint', byte: 'smallint',
  float: 'real', double: 'double precision', keyword: 'text', text: 'text', boolean: 'boolean'
};
const PG_NUMERIC = new Set(['bigint', 'integer', 'smallint', 'real', 'double precision', 'numeric']);

// Each field of a typed collection is its own column, quoted so it keeps its case. Columns a
// newer version added are read and written too, a doc only has the fields that aren't null.
class PGTypedCollection extends ArkimeDb.Collection {
  #pool;
  #table;
  #columns = new Map(); // field -> pg type, every column the table has
  #meta;
  #inet;
  #ready;
  #op = (field, o, c, params) => compileTypedOp(field, o, c, params, this.#columns, this.#inet);

  constructor (pool, prefix, collName, options) {
    super(collName, options);
    this.#pool = pool;
    this.#table = ArkimeDb.checkField(options.pg?.table ?? `${prefix}${options.table ?? collName}`);
    this.#meta = ArkimeDb.checkField(`${prefix}arkime_meta`);
    this.#inet = ArkimeDb.checkField(`${prefix}arkime_inet`);
  }

  // Columns are only added when missing, an ALTER takes a lock even when the column exists
  init () {
    this.#ready ??= (async () => {
      const t = this.#table;
      await this.#create(`CREATE TABLE IF NOT EXISTS ${t} (id TEXT PRIMARY KEY, version INTEGER NOT NULL DEFAULT 1)`);
      await this.#create(metaTable(this.#meta));
      await this.#create(inetFunction(this.#inet));

      const want = ArkimeDb.fieldTypes(this.options).map(([field, type]) => {
        if (field.includes('.')) { throw new Error(`typed ${t} field '${field}' can't be nested`); }
        if (!PG_TYPES[type]) { throw new Error(`typed ${t} field '${field}' has unsupported type '${type}'`); }
        return [field, PG_TYPES[type]];
      });
      const have = await this.#tableColumns();
      const missing = want.filter(([field]) => !have.has(field));
      if (missing.length) {
        const client = await this.#pool.connect();
        try {
          await client.query('BEGIN');
          await client.query("SET LOCAL lock_timeout = '10s'");
          await client.query(`ALTER TABLE ${t} ${missing.map(([field, type]) => `ADD COLUMN IF NOT EXISTS "${field}" ${type}`).join(', ')}`);
          await client.query('COMMIT');
        } catch (err) {
          await client.query('ROLLBACK');
          if (err.code !== '42701') { throw err; }
        } finally {
          client.release();
        }
      }
      this.#columns = await this.#tableColumns();

      for (const field of ArkimeDb.indexedFields(this.options)) {
        const col = ArkimeDb.fieldColumn(field);
        await this.#create(`CREATE INDEX IF NOT EXISTS ${t}_${col} ON ${t} ("${field}")`);
        if (ArkimeDb.sortIndexedFields(this.options).includes(field)) {
          await this.#create(`CREATE INDEX IF NOT EXISTS ${t}_${col}_desc ON ${t} ("${field}" DESC NULLS LAST)`);
        }
      }
    })().catch((err) => {
      this.#ready = undefined; // the next call retries, a database that was down isn't a failure for good
      throw err;
    });
    return this.#ready;
  }

  async #tableColumns () {
    const { rows } = await this.#pool.query(`SELECT column_name, data_type FROM information_schema.columns
      WHERE table_schema = current_schema() AND table_name = $1 AND column_name NOT IN ('id', 'version')`, [this.#table]);
    return new Map(rows.map(r => [r.column_name, r.data_type]));
  }

  async #create (sql) {
    try {
      await this.#pool.query(sql);
    } catch (err) {
      if (!['23505', '42P07', '42710', '42701', '42723'].includes(err.code)) { throw err; }
    }
  }

  async #query (sql, params) {
    await this.init();
    return params?.[PATTERNS] ? patternQuery(this.#pool, sql, params) : this.#pool.query(sql, params);
  }

  #cols (fields) {
    return fields.map(f => `"${f}"`).join(', ');
  }

  // The doc from a row aliased r, only the asked for fields when there are some
  #doc (fields) {
    const known = fields?.filter(f => this.#columns.has(f));
    if (!known?.length) { return 'jsonb_strip_nulls(to_jsonb(r) - \'id\' - \'version\')'; }
    return `jsonb_strip_nulls(jsonb_build_object(${known.map(f => `'${f}', r."${f}"`).join(', ')}))`;
  }

  #checkDoc (doc) {
    for (const field of Object.keys(doc)) {
      if (!this.#columns.has(field)) { throw new Error(`${this.#table} has no column for '${field}'`); }
    }
  }

  async get (id) {
    const { rows } = await this.#query(`SELECT ${this.#doc()} AS doc FROM ${this.#table} r WHERE id = $1`, [id]);
    return rows[0]?.doc ?? null;
  }

  // Every column is written, so fields missing from the doc become null like a doc replace
  async put (id, doc, { createOnly } = {}) {
    await this.init();
    this.#checkDoc(doc);
    id ??= this.newId();
    const t = this.#table;
    const fields = [...this.#columns.keys()];
    const onConflict = createOnly
      ? 'DO NOTHING'
      : `DO UPDATE SET ${fields.map(f => `"${f}" = excluded."${f}"`).join(', ')}, version = ${t}.version + 1`;
    const { rowCount } = await this.#query(`INSERT INTO ${t} (id, ${this.#cols(fields)})
      SELECT $1, ${fields.map(f => `p."${f}"`).join(', ')} FROM jsonb_populate_record(NULL::${t}, $2::jsonb) p
      ON CONFLICT (id) ${onConflict}`, [id, jsonParam(doc)]);
    if (createOnly && rowCount === 0) { throw new ArkimeDb.ConflictError(id); }
    return id;
  }

  async getVersioned (id) {
    const { rows } = await this.#query(`SELECT ${this.#doc()} AS doc, version FROM ${this.#table} r WHERE id = $1`, [id]);
    return rows[0] ? { doc: rows[0].doc, version: rows[0].version } : null;
  }

  async putIfVersion (id, doc, version) {
    await this.init();
    this.#checkDoc(doc);
    const fields = [...this.#columns.keys()];
    const { rowCount } = await this.#query(`UPDATE ${this.#table} SET (${this.#cols(fields)}) =
      (SELECT ${fields.map(f => `p."${f}"`).join(', ')} FROM jsonb_populate_record(NULL::${this.#table}, $1::jsonb) p),
      version = version + 1 WHERE id = $2 AND version = $3`, [jsonParam(doc), id, version]);
    if (rowCount !== 1) { throw new ArkimeDb.ConflictError(id, `Document '${id}' was changed or deleted`); }
  }

  async update (id, fields) {
    await this.init();
    this.#checkDoc(fields);
    const names = Object.keys(fields);
    if (!names.length) { return (await this.get(id)) !== null; }
    const { rowCount } = await this.#query(`UPDATE ${this.#table} SET (${this.#cols(names)}) =
      (SELECT ${names.map(f => `p."${f}"`).join(', ')} FROM jsonb_populate_record(NULL::${this.#table}, $1::jsonb) p),
      version = version + 1 WHERE id = $2`, [jsonParam(fields), id]);
    return rowCount > 0;
  }

  async delete (id) {
    const { rowCount } = await this.#query(`DELETE FROM ${this.#table} WHERE id = $1`, [id]);
    return rowCount > 0;
  }

  // The columns must be known before compiling a where
  async deleteWhere (where) {
    await this.init();
    const params = [];
    const { rowCount } = await this.#query(`DELETE FROM ${this.#table} WHERE ${compile(where, params, this.#op)}`, params);
    return rowCount;
  }

  async count (where) {
    await this.init();
    const params = [];
    const { rows } = await this.#query(`SELECT COUNT(*)::int AS total FROM ${this.#table} WHERE ${compile(where, params, this.#op)}`, params);
    return rows[0].total;
  }

  async find ({ where, sort, from = 0, size, version, fields } = {}) {
    await this.init();
    const params = [];
    const w = compile(where, params, this.#op);
    const { rows: [{ total }] } = await this.#query(`SELECT COUNT(*)::int AS total FROM ${this.#table} WHERE ${w}`, params);
    if (size === 0) { return { total, data: [] }; }

    const order = [];
    for (const [field, dir] of ArkimeDb.sortList(sort)) {
      if (field === 'id') {
        order.push(`id COLLATE "C" ${dir}`);
      } else if (this.#columns.has(field)) {
        const collate = PG_NUMERIC.has(this.#columns.get(field)) ? '' : ' COLLATE "C"';
        order.push(`"${field}"${collate} ${dir} NULLS LAST`);
      }
    }

    params.push(size ?? null, from);
    const { rows } = await this.#query(`SELECT id, ${this.#doc(fields)} AS doc, version FROM ${this.#table} r WHERE ${w}` +
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

  async aggregate ({ where, groupBy, metrics = {}, size = 1000 } = {}) {
    ArkimeDb.checkMetrics(metrics);
    await this.init();
    const params = [];
    const w = compile(where, params, this.#op);
    let key = 'id COLLATE "C"';
    let numericKey = false;
    if (ArkimeDb.checkField(groupBy) !== 'id') {
      if (!this.#columns.has(groupBy)) { return []; }
      key = `"${groupBy}"`;
      numericKey = PG_NUMERIC.has(this.#columns.get(groupBy));
    }
    const cols = Object.entries(metrics).map(([metricName, metric]) => {
      const [op, field] = Object.entries(metric)[0];
      const v = PG_NUMERIC.has(this.#columns.get(field)) ? `"${field}"` : 'NULL::numeric';
      const agg = `${op.toUpperCase()}(${v})`;
      return `${op === 'sum' ? `COALESCE(${agg}, 0)` : agg} AS "m_${ArkimeDb.checkField(metricName)}"`;
    });
    params.push(size);
    const { rows } = await this.#query(`SELECT ${key} AS key, COUNT(*)::int AS count${cols.map(c => ', ' + c).join('')}
      FROM ${this.#table} WHERE ${w} AND ${key} IS NOT NULL
      GROUP BY ${key} ORDER BY count DESC, ${key} LIMIT $${params.length}`, params);
    return rows.map(r => {
      const bucket = { key: numericKey ? Number(r.key) : r.key, count: r.count };
      for (const metricName of Object.keys(metrics)) {
        const m = r[`m_${metricName}`];
        bucket[metricName] = m === null ? null : Number(m);
      }
      return bucket;
    });
  }

  async nextSequence () {
    throw new Error(`nextSequence isn't supported on typed ${this.#table}`);
  }

  async lease () {
    throw new Error(`lease isn't supported on typed ${this.#table}`);
  }

  async getMeta (key) {
    const { rows } = await this.#query(`SELECT value FROM ${this.#meta} WHERE key = $1`, [`${this.#table}:${key}`]);
    return rows[0]?.value;
  }

  async setMeta (obj) {
    for (const [key, value] of Object.entries(obj)) {
      await this.#query(`INSERT INTO ${this.#meta} (key, value) VALUES ($1, $2)
        ON CONFLICT (key) DO UPDATE SET value = excluded.value`, [`${this.#table}:${key}`, jsonParam(value)]);
    }
  }
}

// A typed column holds one value of its type, a field with no column is never there
function compileTypedOp (field, op, c, params, columns, inet) {
  if (field === 'id') { return compileOp(field, op, c, params, new Map()); }
  const p = (v) => { params.push(v); return `$${params.length}`; };

  let negate = false;
  if (op === '$ne') { op = '$eq'; negate = true; }
  if (op === '$nin') { op = '$in'; negate = true; }

  const type = columns.get(field);
  const col = `"${field}"`;
  const numeric = PG_NUMERIC.has(type);
  const isText = type === 'text';
  // A value only matches a column it could be stored in, like an ES term on the wrong type
  const fits = (v) => numeric ? (typeof v === 'number' || (typeof v === 'string' && v.trim() !== '' && Number.isFinite(Number(v)))) : isText ? (typeof v === 'string' || typeof v === 'number') : typeof v === 'boolean';
  const cast = (v) => numeric ? Number(v) : isText ? String(v) : v;

  if (op === '$exists') {
    const exists = type ? `${col} IS NOT NULL` : 'FALSE';
    return c ? exists : `NOT ${exists}`;
  }

  let test = 'FALSE';
  if (type) {
    switch (op) {
    case '$eq':
      if (fits(c)) { test = `${col} = ${p(cast(c))}`; }
      break;
    case '$in': {
      const vals = c.filter(fits).map(cast);
      if (vals.length) { test = `${col} = ANY(${p(vals)}::${type}[])`; }
      break;
    }
    case '$gt': case '$gte': case '$lt': case '$lte': {
      const cmp = { $gt: '>', $gte: '>=', $lt: '<', $lte: '<=' }[op];
      if (numeric && typeof c === 'number') { test = `${col} ${cmp} ${p(c)}`; }
      if (isText && typeof c === 'string') { test = `${col} COLLATE "C" ${cmp} ${p(c)}`; }
      break;
    }
    case '$contains':
      if (isText) { test = `strpos(${col}, ${p(String(c))}) > 0`; }
      break;
    case '$icontains':
      if (isText) { test = `strpos(lower(${col}), lower(${p(String(c))})) > 0`; }
      break;
    case '$prefix':
      if (isText) { test = `left(${col}, length(${p(String(c))})) = $${params.length}`; }
      break;
    case '$wildcard':
      if (isText) { test = `${col} LIKE ${p(likePattern(c, params))} ESCAPE '\\'`; }
      break;
    case '$regex':
      if (isText) { test = `${col} ~ ('^(?:' || ${p(regexPattern(c, params))} || ')$')`; }
      break;
    case '$cidr':
      if (isText) { test = `${inet}(${col}) <<= ${p(String(c))}::inet`; }
      break;
    }
  }

  const direct = type ? `(${col} IS NOT NULL AND ${test})` : 'FALSE';
  return negate ? `NOT COALESCE(${direct}, FALSE)` : direct;
}

// Like the OpenSearch/Elasticsearch connections, a remote server's certificate is verified with caTrustFile
// or the system CAs, and --insecure still requires TLS. Ssl settings in the url win, loopback and unix
// socket servers are left alone.
function sslOptions (url, options) {
  let params;
  try { params = new URL(url).searchParams; } catch (e) { params = new URLSearchParams(); }
  if (['ssl', 'sslmode', 'sslcert', 'sslkey', 'sslrootcert'].some(k => params.has(k))) { return undefined; }
  // Unparseable is remote, a parse that isn't pg's could miss the host pg uses
  if (ArkimeConfig.isLoopbackHost(PGConnection.tlsHost(url))) { return undefined; }
  return ArkimeUtil.tlsOptionsOrExit({
    insecure: options.insecure ?? ArkimeConfig.insecure,
    caTrustFile: options.caTrustFile ?? ArkimeConfig.get('caTrustFile')
  }, 'Postgres');
}

// Set on a query's params when its where has a user's $regex or $wildcard pattern
const PATTERNS = Symbol('patterns');
const PATTERN_SLOTS = 2;
const PATTERN_QUEUE = 100;
const patternSlots = new WeakMap(); // pool -> { free, waiting }

// A user's pattern can run long, so each runs with postgresPatternTimeout and only a couple at a
// time per pool, the rest of the pool stays free for everything else
async function patternQuery (pool, sql, params) {
  let slots = patternSlots.get(pool);
  if (!slots) {
    slots = { free: PATTERN_SLOTS, waiting: [] };
    patternSlots.set(pool, slots);
  }
  if (slots.free > 0) {
    slots.free--;
  } else {
    if (slots.waiting.length >= PATTERN_QUEUE) { throw new Error('Too many $regex/$wildcard searches running, try again'); }
    await new Promise(resolve => slots.waiting.push(resolve));
  }

  try {
    const timeout = parseInt(ArkimeConfig.get('postgresPatternTimeout', 60), 10) || 0;
    const client = await pool.connect();
    let broken;
    try {
      await client.query('BEGIN');
      await client.query(`SET LOCAL statement_timeout = ${Math.max(0, timeout) * 1000}`);
      const result = await client.query(sql, params);
      await client.query('COMMIT');
      return result;
    } catch (err) {
      await client.query('ROLLBACK').catch((e) => { broken = e; });
      throw err;
    } finally {
      client.release(broken);
    }
  } finally {
    const next = slots.waiting.shift();
    if (next) { next(); } else { slots.free++; }
  }
}

const PG_SCHEMA_VERSION = 89;

class PGConnection {
  pool;
  prefix;
  #closed = false;
  #collections = new Map();

  constructor (url, options) {
    const { Pool } = require('pg');
    this.prefix = options.prefix ?? '';
    if (options.pool) {
      this.pool = options.pool;
      return;
    }
    const connectionString = url.replace(/^postgres(ql)?:/, 'postgres:');
    this.pool = new Pool({ connectionString, ssl: sslOptions(connectionString, options) });
    // An idle client losing its connection, like a Postgres restart, is an error event that would
    // otherwise end the process. The pool drops that client and the next query connects again.
    this.pool.on('error', (err) => {
      console.log('ERROR - Postgres idle connection lost:', err.message);
    });
  }

  withPrefix (prefix) {
    return prefix === this.prefix ? this : new PGConnection(undefined, { pool: this.pool, prefix });
  }

  // Collections make their own tables, so Postgres has everything the db.pl schema version it matches has
  async schemaVersion () {
    return PG_SCHEMA_VERSION;
  }

  collection (collName, options = {}) {
    let coll = this.#collections.get(collName);
    if (!coll) {
      coll = new (options.typed ? PGTypedCollection : PGCollection)(this.pool, this.prefix, collName, options);
      this.#collections.set(collName, coll);
    }
    return coll;
  }

  /**
   * The host pg will connect to, PGHOST and a repeated ?host= included, '' when pg can't parse it so it counts as remote
   */
  static tlsHost (url) {
    try {
      const ConnectionParameters = require('pg/lib/connection-parameters');
      return new ConnectionParameters({ connectionString: url }).host ?? '';
    } catch (e) {
      return '';
    }
  }

  async close () {
    if (this.#closed) { return; }
    this.#closed = true;
    await this.pool.end();
  }
}

module.exports = PGConnection;
