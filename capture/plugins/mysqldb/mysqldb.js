/* mysqldb.js  -- MySQL collections for ArkimeDb, a dbPlugins module for mysql:// urls
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const fs = require('fs');
const net = require('net');

// Ids, meta keys and indexed keyword columns fit an InnoDB index entry, at most 3072 bytes
const ID_TYPE = 'VARBINARY(3072)';
const KEY_CHARS = 768;
const TEXT_TYPE = `VARCHAR(${KEY_CHARS}) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_bin`;
const BIN = 'COLLATE utf8mb4_0900_bin';
const NUM_JSON = "'INTEGER', 'UNSIGNED INTEGER', 'DOUBLE', 'DECIMAL'";
const NUMERIC = new Set(['long', 'date', 'integer', 'short', 'byte', 'double', 'float', 'half_float', 'scaled_float', 'unsigned_long']);
const NO_LIMIT = '18446744073709551615';

const ER_DUP_FIELDNAME = 1060;
const ER_DUP_KEYNAME = 1061;
const ER_DUP_ENTRY = 1062;
const ER_LOCK_DEADLOCK = 1213;

let ArkimeDb, ArkimeConfig, ArkimeUtil;

// '$."a"."b"', the one value at a path
function jsonPath (field) {
  return `'$${field.split('.').map(f => `."${f}"`).join('')}'`;
}

// Every value along a path, [0 to last] wraps a lone value so arrays at any level flatten like ES
function valuesPath (field) {
  return `'$${field.split('.').map(f => `."${f}"[0 to last]`).join('')}'`;
}

// JSON can't hold a lone surrogate, which would fail the whole write, JSON.stringify escapes them
function jsonParam (value) {
  return JSON.stringify(value)?.replace(/(?<!\\)((?:\\\\)*)\\ud[89a-f][0-9a-f]{2}/gi, '$1�');
}

function codePoints (s) {
  return [...s].length;
}

// * and ? to a LIKE pattern with ! as the escape, LIKE's own % _ and ! are literal
function likePattern (c) {
  return String(c).replace(/[!%_]/g, '!$&').replace(/\*/g, '%').replace(/\?/g, '_');
}

function prefixPattern (c) {
  return String(c).replace(/[!%_]/g, '!$&') + '%';
}

// RE2 checks the syntax, which refuses backreferences and lookarounds. ICU then runs it, so the
// syntax ICU reads differently is refused too: (?flags) and named groups, and a [ inside a class
// that isn't a [:posix:] class or && in one, which ICU takes as set operations.
function regexPattern (c) {
  const pattern = String(c);
  ArkimeDb.regex(pattern);
  let inClass = false;
  for (let i = 0; i < pattern.length; i++) {
    const ch = pattern[i];
    if (ch === '\\') { i++; continue; }
    if (inClass) {
      if (ch === ']' && pattern[i - 1] !== '[' && !(pattern[i - 1] === '^' && pattern[i - 2] === '[')) { inClass = false; }
      if ((ch === '[' && pattern[i + 1] !== ':') || (ch === '&' && pattern[i + 1] === '&')) { refuse(); }
    } else if (ch === '[') {
      inClass = true;
    } else if (ch === '(' && pattern[i + 1] === '?' && pattern[i + 2] !== ':') {
      refuse();
    }
  }
  return `^(?:${pattern})\\z`;

  function refuse () {
    throw new Error(`Invalid db where $regex '${pattern}', (?flags), named groups and nested classes aren't supported`);
  }
}

function ipBytes (ip) {
  ip = ip.replace(/%.*$/, '');
  const family = net.isIP(ip);
  if (family === 4) { return Buffer.from(ip.split('.').map(Number)); }
  if (family !== 6) { return null; }
  const bytes = Buffer.alloc(16);
  let [head, tail] = ip.split('::');
  const groups = (s) => {
    const out = [];
    for (const g of s ? s.split(':') : []) {
      if (g.includes('.')) {
        const v4 = ipBytes(g);
        out.push(v4.readUInt16BE(0), v4.readUInt16BE(2));
      } else {
        out.push(parseInt(g, 16));
      }
    }
    return out;
  };
  head = groups(head);
  tail = tail === undefined ? [] : groups(tail);
  [...head, ...Array(8 - head.length - tail.length).fill(0), ...tail].forEach((g, i) => bytes.writeUInt16BE(g, i * 2));
  return bytes;
}

// The first and last address of a cidr, or the one ip when it has no /
function cidrRange (cidr) {
  const [ip, bits] = String(cidr).split('/');
  const start = ipBytes(ip);
  const end = Buffer.from(start);
  const keep = bits === undefined ? start.length * 8 : Number(bits);
  for (let i = 0; i < start.length; i++) {
    const block = 2 ** (8 - Math.max(0, Math.min(8, keep - i * 8)));
    start[i] -= start[i] % block;
    end[i] = start[i] + block - 1;
  }
  return { start, end };
}

// INET6_ATON fails a strict write on a string that isn't an ip, so check first
function inetExpr (s) {
  return `IF(IS_IPV4(${s}) OR IS_IPV6(${s}), INET6_ATON(${s}), NULL)`;
}

function inCidr (s, c, q) {
  const { start, end } = cidrRange(c);
  const ip = inetExpr(s);
  return `(LENGTH(${ip}) = ${start.length} AND ${ip} BETWEEN ${q.p(start)} AND ${q.p(end)})`;
}

// A LIMIT number, inlined since a prepared LIMIT ? needs an integer param
function count (v) {
  const n = Math.trunc(Number(v));
  return Number.isFinite(n) && n > 0 ? n : 0;
}

function kindOf (type) {
  if (NUMERIC.has(type)) { return 'number'; }
  if (type === 'boolean' || type === 'ip') { return type; }
  return 'string';
}

// The generated column an indexed field of a json collection gets, NULL when the value isn't of
// its type. capture/plugins/mysqldb/mysqldb.c creates the files table's with the same sql.
function indexedColumn (field, type) {
  const j = `JSON_EXTRACT(doc, ${jsonPath(field)})`;
  switch (kindOf(type)) {
  case 'number':
    return `DOUBLE GENERATED ALWAYS AS (CASE WHEN JSON_TYPE(${j}) IN (${NUM_JSON}) THEN CAST(${j} AS DOUBLE) END) VIRTUAL`;
  case 'boolean':
    return `TINYINT GENERATED ALWAYS AS (CASE WHEN JSON_TYPE(${j}) = 'BOOLEAN' THEN ${j} = CAST('true' AS JSON) END) VIRTUAL`;
  case 'ip':
    return `VARBINARY(16) GENERATED ALWAYS AS (CASE WHEN JSON_TYPE(${j}) = 'STRING' THEN ${inetExpr(`JSON_UNQUOTE(${j})`)} END) VIRTUAL`;
  default:
    return `${TEXT_TYPE} GENERATED ALWAYS AS (CASE WHEN JSON_TYPE(${j}) = 'STRING' THEN LEFT(JSON_UNQUOTE(${j}), ${KEY_CHARS}) END) VIRTUAL`;
  }
}

// ES mapping types to the column a typed collection gives them
const MYSQL_TYPES = {
  long: 'BIGINT', date: 'BIGINT', integer: 'INT', short: 'SMALLINT', byte: 'SMALLINT', float: 'DOUBLE', double: 'DOUBLE',
  keyword: TEXT_TYPE, text: TEXT_TYPE, ip: TEXT_TYPE, boolean: 'BOOLEAN'
};

function columnKind (dataType, columnType) {
  if (columnType === 'tinyint(1)') { return 'boolean'; }
  if (['bigint', 'int', 'mediumint', 'smallint', 'tinyint', 'double', 'float', 'decimal'].includes(dataType)) { return 'number'; }
  return 'string';
}

function checkName (table) {
  ArkimeDb.checkField(table);
  if (table.includes('.') || table.length > 64) { throw new Error(`MySQL table name '${table}' is too long or has a .`); }
  return table;
}

// The params of one statement, $regex and $wildcard mark it as a pattern query
class Query {
  params = [];
  patterns = false;
  p (v) { this.params.push(v); return '?'; }
  // Padded to a power of two so a few prepared statements serve every list length
  list (values) {
    let n = 1;
    while (n < values.length) { n *= 2; }
    for (let i = 0; i < n; i++) { this.params.push(values[Math.min(i, values.length - 1)]); }
    return Array(n).fill('?').join(', ');
  }
}

function idOp (t, op, c, q) {
  const id = `${t}.id`;
  const text = `CONVERT(${id} USING utf8mb4) ${BIN}`;
  const b = (v) => q.p(Buffer.from(String(v)));
  switch (op) {
  case '$eq': return `${id} = ${b(c)}`;
  case '$ne': return `${id} <> ${b(c)}`;
  case '$in': return c.length ? `${id} IN (${q.list(c.map(v => Buffer.from(String(v))))})` : 'FALSE';
  case '$nin': return c.length ? `${id} NOT IN (${q.list(c.map(v => Buffer.from(String(v))))})` : 'TRUE';
  case '$exists': return c ? 'TRUE' : 'FALSE';
  case '$gt': return `${id} > ${b(c)}`;
  case '$gte': return `${id} >= ${b(c)}`;
  case '$lt': return `${id} < ${b(c)}`;
  case '$lte': return `${id} <= ${b(c)}`;
  case '$contains': return `INSTR(${id}, ${b(c)}) > 0`;
  case '$icontains': return `INSTR(LOWER(${text}), LOWER(${q.p(String(c))})) > 0`;
  case '$prefix': return `${id} LIKE ${q.p(Buffer.from(prefixPattern(c)))} ESCAPE '!'`;
  case '$wildcard': q.patterns = true; return `${text} LIKE ${q.p(likePattern(c))} ESCAPE '!'`;
  case '$regex': q.patterns = true; return `REGEXP_LIKE(${text}, ${q.p(regexPattern(c))}, 'cn')`;
  case '$cidr': return inCidr(text, c, q);
  }
}

// A test on one json value v, the same for every value of a field
function valueTest (v, op, c, q) {
  const s = `JSON_UNQUOTE(${v}) ${BIN}`;
  const isString = `JSON_TYPE(${v}) = 'STRING'`;
  switch (op) {
  case '$gt': case '$gte': case '$lt': case '$lte': {
    const cmp = { $gt: '>', $gte: '>=', $lt: '<', $lte: '<=' }[op];
    return typeof c === 'number'
      ? `JSON_TYPE(${v}) IN (${NUM_JSON}) AND ${v} ${cmp} CAST(${q.p(JSON.stringify(c))} AS JSON)`
      : `${isString} AND ${s} ${cmp} ${q.p(String(c))}`;
  }
  case '$contains': return `${isString} AND INSTR(${s}, ${q.p(String(c))}) > 0`;
  case '$icontains': return `${isString} AND INSTR(LOWER(${s}), LOWER(${q.p(String(c))})) > 0`;
  case '$prefix': return `${isString} AND ${s} LIKE ${q.p(prefixPattern(c))} ESCAPE '!'`;
  case '$wildcard': q.patterns = true; return `${isString} AND ${s} LIKE ${q.p(likePattern(c))} ESCAPE '!'`;
  case '$regex': q.patterns = true; return `${isString} AND REGEXP_LIKE(${s}, ${q.p(regexPattern(c))}, 'cn')`;
  case '$cidr': return `${isString} AND ${inCidr(`JSON_UNQUOTE(${v})`, c, q)}`;
  }
}

// A json collection's field, matched against its values as a set so arrays and scalars behave the same
function fieldOp (t, field, op, c, q) {
  const values = `JSON_EXTRACT(${t}.doc, ${valuesPath(field)})`;
  // NO_SEMIJOIN, as a semijoin MySQL reads the JSON_TABLE of one row for every row
  const each = (test) => `EXISTS (SELECT /*+ NO_SEMIJOIN() */ 1 FROM JSON_TABLE(${t}.doc, ${valuesPath(field)} COLUMNS (v JSON PATH '$')) e WHERE JSON_TYPE(e.v) <> 'NULL' AND ${test})`;
  switch (op) {
  case '$eq': return c === null ? 'FALSE' : `COALESCE(CAST(${q.p(jsonParam(c))} AS JSON) MEMBER OF (${values}), FALSE)`;
  case '$in': {
    const vals = c.filter(v => v !== null);
    return vals.length ? `COALESCE(JSON_OVERLAPS(${values}, CAST(${q.p(jsonParam(vals))} AS JSON)), FALSE)` : 'FALSE';
  }
  case '$exists': return c ? each('TRUE') : `NOT ${each('TRUE')}`;
  default: return each(valueTest('e.v', op, c, q));
  }
}

// An indexed field holds one value, its column answers when the value is of the column's type and
// the json value checks what the column can't tell, a long string or a big number
function indexedOp (t, field, kind, op, c, q) {
  const col = `${t}.${ArkimeDb.fieldColumn(field)}`;
  const j = `JSON_EXTRACT(${t}.doc, ${jsonPath(field)})`;
  const json = (v) => `CAST(${q.p(jsonParam(v))} AS JSON)`;
  const fits = (v) => (kind === 'number' && typeof v === 'number') || (kind === 'boolean' && typeof v === 'boolean') ||
    (kind === 'string' && typeof v === 'string') || (kind === 'ip' && typeof v === 'string' && net.isIP(v) !== 0);
  const short = (v) => typeof v === 'string' && codePoints(v) < KEY_CHARS;

  const eq = (v) => {
    if (v === null) { return 'FALSE'; }
    if (!fits(v)) { return `${j} = ${json(v)}`; }
    if (kind === 'boolean') { return `${col} = ${v ? 1 : 0}`; }
    if (kind === 'number') { return `(${col} = ${q.p(v)} AND ${j} = ${json(v)})`; }
    if (kind === 'ip') { return `(${col} = ${q.p(ipBytes(v))} AND JSON_UNQUOTE(${j}) ${BIN} = ${q.p(v)})`; }
    return short(v) ? `${col} = ${q.p(v)}` : `(${col} = ${q.p([...v].slice(0, KEY_CHARS).join(''))} AND JSON_UNQUOTE(${j}) ${BIN} = ${q.p(v)})`;
  };

  switch (op) {
  case '$eq': return eq(c);
  case '$in': {
    const fitting = c.filter(v => v !== null && fits(v));
    const other = c.filter(v => v !== null && !fits(v));
    const parts = [];
    if (fitting.length) {
      const keys = fitting.map(v => kind === 'boolean' ? (v ? 1 : 0) : kind === 'ip' ? ipBytes(v) : kind === 'string' ? [...v].slice(0, KEY_CHARS).join('') : v);
      const direct = `${col} IN (${q.list(keys)})`;
      parts.push(kind === 'boolean' || (kind === 'string' && fitting.every(short)) ? direct : `(${direct} AND ${j} MEMBER OF (CAST(${q.p(jsonParam(fitting))} AS JSON)))`);
    }
    if (other.length) { parts.push(`${j} MEMBER OF (CAST(${q.p(jsonParam(other))} AS JSON))`); }
    if (!parts.length) { return 'FALSE'; }
    return parts.length === 1 ? parts[0] : `(${parts.join(' OR ')})`;
  }
  case '$exists': return c ? `COALESCE(JSON_TYPE(${j}) <> 'NULL', FALSE)` : `NOT COALESCE(JSON_TYPE(${j}) <> 'NULL', FALSE)`;
  case '$gt': case '$gte': case '$lt': case '$lte': {
    const cmp = { $gt: '>', $gte: '>=', $lt: '<', $lte: '<=' }[op];
    if (kind === 'number' && typeof c === 'number') { return `(${col} ${cmp} ${q.p(c)} AND ${j} ${cmp} ${json(c)})`; }
    if (kind === 'string' && typeof c === 'string' && short(c)) { return `${col} ${cmp} ${q.p(c)}`; }
    break;
  }
  case '$prefix':
    if (kind === 'string' && short(String(c))) { return `${col} LIKE ${q.p(prefixPattern(c))} ESCAPE '!'`; }
    break;
  case '$cidr':
    if (kind === 'ip') {
      const { start, end } = cidrRange(c);
      return `(LENGTH(${col}) = ${start.length} AND ${col} BETWEEN ${q.p(start)} AND ${q.p(end)})`;
    }
    break;
  }
  return `COALESCE(${valueTest(j, op, c, q)}, FALSE)`;
}

function compile (where, q, op) {
  if (!where) { return 'TRUE'; }
  const parts = [];
  for (const [key, cond] of Object.entries(where)) {
    if (key === '$or') {
      parts.push(cond.length ? `(${cond.map(w => compile(w, q, op)).join(' OR ')})` : 'FALSE');
    } else if (key === '$and') {
      parts.push(cond.length ? `(${cond.map(w => compile(w, q, op)).join(' AND ')})` : 'TRUE');
    } else if (key === '$not') {
      parts.push(`NOT COALESCE(${compile(cond, q, op)}, FALSE)`);
    } else {
      ArkimeDb.checkField(key);
      for (const [o, c] of ArkimeDb.condOps(cond)) {
        if ((o === '$ne' || o === '$nin') && key !== 'id') {
          // A field with no value matches $ne like ES, a NULL from its column is false first
          parts.push(`NOT COALESCE(${op(key, o === '$ne' ? '$eq' : '$in', c, q)}, FALSE)`);
        } else {
          parts.push(op(key, o, c, q));
        }
      }
    }
  }
  return parts.length ? parts.join(' AND ') : 'TRUE';
}

const PATTERN_SLOTS = 2;
const PATTERN_QUEUE = 100;

function defineCollections () {
  // Shared by the json and typed collections, the table plumbing and statements that don't care how a doc is stored
  class MySQLBase extends ArkimeDb.Collection {
    conn;
    table;
    meta;
    #ready;

    constructor (conn, collName, options) {
      super(collName, options);
      this.conn = conn;
      this.table = checkName(options.mysql?.table ?? `${conn.prefix}${options.table ?? collName}`);
      // Per prefix so clusters sharing a database never read or write each other's
      this.meta = checkName(`${conn.prefix}arkime_meta`);
    }

    init () {
      this.#ready ??= (async () => {
        await this.conn.checkVersion();
        await this.ddl(`CREATE TABLE IF NOT EXISTS \`${this.meta}\` (id ${ID_TYPE} NOT NULL PRIMARY KEY, \`value\` JSON NOT NULL) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_bin`);
        await this.createTables();
      })().catch((err) => {
        this.#ready = undefined; // the next call retries, a database that was down isn't a failure for good
        throw err;
      });
      return this.#ready;
    }

    // Several processes can create the same column or index at once, the loser's duplicate means it exists now
    async ddl (sql) {
      try {
        await this.conn.pool.query(sql);
      } catch (err) {
        if (![ER_DUP_FIELDNAME, ER_DUP_KEYNAME].includes(err.errno)) { throw err; }
      }
    }

    async columns () {
      const [rows] = await this.conn.pool.execute(`SELECT COLUMN_NAME AS name, DATA_TYPE AS dataType, COLUMN_TYPE AS columnType FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = ? AND COLUMN_NAME NOT IN ('id', 'doc', 'version')`, [this.table]);
      return new Map(rows.map(r => [r.name, columnKind(r.dataType.toLowerCase(), r.columnType.toLowerCase())]));
    }

    async indexes () {
      const [rows] = await this.conn.pool.execute('SELECT DISTINCT INDEX_NAME AS name FROM information_schema.STATISTICS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = ?', [this.table]);
      return new Set(rows.map(r => r.name.toLowerCase()));
    }

    async query (sql, q = new Query()) {
      await this.init();
      return this.conn.run(sql, q);
    }

    async get (id) {
      await this.init();
      const [rows] = await this.query(`SELECT ${this.docSelect()} FROM \`${this.table}\` WHERE id = ?`, { params: [Buffer.from(id)] });
      return rows[0] ? this.toDoc(rows[0]) : null;
    }

    async getVersioned (id) {
      await this.init();
      const [rows] = await this.query(`SELECT ${this.docSelect()}, version FROM \`${this.table}\` WHERE id = ?`, { params: [Buffer.from(id)] });
      return rows[0] ? { doc: this.toDoc(rows[0]), version: rows[0].version } : null;
    }

    async delete (id) {
      const [result] = await this.query(`DELETE FROM \`${this.table}\` WHERE id = ?`, { params: [Buffer.from(id)] });
      return result.affectedRows > 0;
    }

    async deleteWhere (where) {
      await this.init();
      const q = new Query();
      const [result] = await this.query(`DELETE FROM \`${this.table}\` WHERE ${compile(where, q, this.op)}`, q);
      return result.affectedRows;
    }

    async count (where) {
      await this.init();
      const q = new Query();
      const [rows] = await this.query(`SELECT COUNT(*) AS total FROM \`${this.table}\` WHERE ${compile(where, q, this.op)}`, q);
      return Number(rows[0].total);
    }

    async find ({ where, sort, from = 0, size, version, fields } = {}) {
      await this.init();
      const q = new Query();
      const w = compile(where, q, this.op);
      const total = Number((await this.query(`SELECT COUNT(*) AS total FROM \`${this.table}\` WHERE ${w}`, q))[0][0].total);
      if (size === 0) { return { total, data: [] }; }

      const order = [];
      for (const [field, dir] of ArkimeDb.sortList(sort)) {
        order.push(...(field === 'id' ? [`id ${dir}`] : this.sortBy(field, dir)));
      }
      const limit = `LIMIT ${count(from)}, ${size === undefined || size === null ? NO_LIMIT : count(size)}`;
      const [rows] = await this.query(`SELECT id, ${this.docSelect(fields)}, version FROM \`${this.table}\` WHERE ${w}` +
        (order.length ? ` ORDER BY ${order.join(', ')}` : '') + ` ${limit}`, q);

      return {
        total,
        data: rows.map(r => {
          const hit = { id: r.id.toString(), doc: this.toDoc(r) };
          if (version && this.options.version) { hit.version = r.version; }
          return hit;
        })
      };
    }

    async aggregate ({ where, groupBy, metrics = {}, size = 1000 } = {}) {
      ArkimeDb.checkMetrics(metrics);
      ArkimeDb.checkField(groupBy);
      await this.init();
      const q = new Query();
      const w = compile(where, q, this.op);
      const group = groupBy === 'id' ? { from: '', key: `\`${this.table}\`.id`, isId: true } : this.groupKey(groupBy);
      if (!group) { return []; }
      const cols = Object.entries(metrics).map(([metricName, metric]) => {
        const [op, field] = Object.entries(metric)[0];
        const agg = `${op.toUpperCase()}(${this.metricValue(ArkimeDb.checkField(field))})`;
        return `${op === 'sum' ? `COALESCE(${agg}, 0)` : agg} AS \`m_${ArkimeDb.checkField(metricName)}\``;
      });
      const [rows] = await this.query(`SELECT ${group.key} AS k, COUNT(*) AS n${cols.map(c => ', ' + c).join('')}
        FROM \`${this.table}\`${group.from} WHERE ${w} AND ${group.key} IS NOT NULL
        GROUP BY k ORDER BY n DESC, k LIMIT ${count(size)}`, q);
      return rows.map(r => {
        const bucket = { key: group.isId ? r.k.toString() : group.numeric ? Number(r.k) : r.k, count: Number(r.n) };
        for (const metricName of Object.keys(metrics)) {
          const m = r[`m_${metricName}`];
          bucket[metricName] = m === null ? null : Number(m);
        }
        return bucket;
      });
    }

    async getMeta (key) {
      const [rows] = await this.query(`SELECT \`value\` FROM \`${this.meta}\` WHERE id = ?`, { params: [Buffer.from(`${this.table}:${key}`)] });
      return rows[0]?.value;
    }

    async setMeta (obj) {
      for (const [key, value] of Object.entries(obj)) {
        await this.query(`INSERT INTO \`${this.meta}\` (id, \`value\`) VALUES (?, CAST(? AS JSON)) AS new ON DUPLICATE KEY UPDATE \`value\` = new.\`value\``,
          { params: [Buffer.from(`${this.table}:${key}`), jsonParam(value)] });
      }
    }
  }

  // Each collection is a table of (id, doc json, version). Indexed fields are virtual generated
  // columns of their type with an index.
  class MySQLCollection extends MySQLBase {
    #indexed;
    op = (field, o, c, q) => {
      const t = `\`${this.table}\``;
      if (field === 'id') { return idOp(t, o, c, q); }
      const kind = this.#indexed.get(field);
      return kind ? indexedOp(t, field, kind, o, c, q) : fieldOp(t, field, o, c, q);
    };

    constructor (conn, collName, options) {
      super(conn, collName, options);
      const types = new Map(ArkimeDb.fieldTypes(options));
      this.#indexed = new Map(ArkimeDb.indexedFields(options).map(f => [f, kindOf(types.get(f))]));
    }

    async createTables () {
      const t = `\`${this.table}\``;
      await this.ddl(`CREATE TABLE IF NOT EXISTS ${t} (id ${ID_TYPE} NOT NULL PRIMARY KEY, doc JSON NOT NULL CHECK (JSON_TYPE(doc) = 'OBJECT'), version INT NOT NULL DEFAULT 1) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_bin`);
      const indexed = ArkimeDb.indexedFields(this.options);
      if (!indexed.length) { return; }

      const types = new Map(ArkimeDb.fieldTypes(this.options));
      const sorted = new Set(ArkimeDb.sortIndexedFields(this.options));
      const have = await this.columns();
      const haveIndexes = await this.indexes();
      for (const field of indexed) {
        const col = ArkimeDb.fieldColumn(field);
        if (!have.has(col)) { await this.ddl(`ALTER TABLE ${t} ADD COLUMN ${col} ${indexedColumn(field, types.get(field))}`); }
        if (!haveIndexes.has(col.toLowerCase())) { await this.ddl(`CREATE INDEX ${col} ON ${t} (${col})`); }
        // Ascending with the missing values last, a descending sort reads the plain index backwards
        if (sorted.has(field) && !haveIndexes.has(`${col}_nl`.toLowerCase())) { await this.ddl(`CREATE INDEX ${col}_nl ON ${t} ((${col} IS NULL), ${col})`); }
      }
    }

    docSelect () { return 'doc'; }
    toDoc (row) { return row.doc; }

    sortBy (field, dir) {
      if (this.#indexed.get(field) === 'number' && ArkimeDb.sortIndexedFields(this.options).includes(field)) {
        const col = ArkimeDb.fieldColumn(field);
        return dir === 'desc' ? [`${col} DESC`] : [`(${col} IS NULL)`, `${col} ASC`];
      }
      const j = `JSON_EXTRACT(doc, ${jsonPath(field)})`;
      return [`(${j} IS NULL OR JSON_TYPE(${j}) = 'NULL')`,
        `CASE WHEN JSON_TYPE(${j}) IN (${NUM_JSON}) THEN CAST(${j} AS DOUBLE) END ${dir}`,
        `CAST(JSON_UNQUOTE(${j}) AS CHAR) ${BIN} ${dir}`];
    }

    // An indexed field groups by its one value, others by every value so each array element counts
    groupKey (field) {
      if (this.#indexed.has(field)) {
        const j = `JSON_EXTRACT(\`${this.table}\`.doc, ${jsonPath(field)})`;
        return { from: '', key: `IF(JSON_TYPE(${j}) = 'NULL', NULL, ${j})` };
      }
      return { from: `, JSON_TABLE(\`${this.table}\`.doc, ${valuesPath(field)} COLUMNS (value JSON PATH '$')) AS g`, key: "IF(JSON_TYPE(g.value) = 'NULL', NULL, g.value)" };
    }

    metricValue (field) {
      const j = `JSON_EXTRACT(\`${this.table}\`.doc, ${jsonPath(field)})`;
      return `CASE WHEN JSON_TYPE(${j}) IN (${NUM_JSON}) THEN CAST(${j} AS DECIMAL(65, 10)) END`;
    }

    async put (id, doc, { createOnly } = {}) {
      id ??= this.newId();
      const upsert = createOnly ? '' : ' AS new ON DUPLICATE KEY UPDATE doc = new.doc, version = version + 1';
      try {
        await this.query(`INSERT INTO \`${this.table}\` (id, doc) VALUES (?, CAST(? AS JSON))${upsert}`, { params: [Buffer.from(id), jsonParam(doc)] });
      } catch (err) {
        if (createOnly && err.errno === ER_DUP_ENTRY) { throw new ArkimeDb.ConflictError(id); }
        throw err;
      }
      return id;
    }

    async putIfVersion (id, doc, version) {
      const [result] = await this.query(`UPDATE \`${this.table}\` SET doc = CAST(? AS JSON), version = version + 1 WHERE id = ? AND version = ?`,
        { params: [jsonParam(doc), Buffer.from(id), version] });
      if (result.affectedRows !== 1) { throw new ArkimeDb.ConflictError(id, `Document '${id}' was changed or deleted`); }
    }

    // Top level fields replace, JSON_MERGE_PATCH would drop nulls and merge nested objects
    async update (id, fields) {
      const keys = Object.keys(fields);
      if (!keys.length) { return (await this.get(id)) !== null; }
      const params = [];
      const sets = keys.map(k => { params.push(`$.${JSON.stringify(k)}`, jsonParam(fields[k]) ?? 'null'); return '?, CAST(? AS JSON)'; });
      params.push(Buffer.from(id));
      const [result] = await this.query(`UPDATE \`${this.table}\` SET doc = JSON_SET(doc, ${sets.join(', ')}), version = version + 1 WHERE id = ?`, { params });
      return result.affectedRows > 0;
    }

    // LAST_INSERT_ID(expr) hands back the new value of an existing sequence, a new one starts at 1
    async nextSequence (id) {
      const [result] = await this.query(`INSERT INTO \`${this.table}\` (id, doc) VALUES (?, '{"seq": 1}') ON DUPLICATE KEY UPDATE
        doc = JSON_SET(doc, '$.seq', LAST_INSERT_ID(COALESCE(CAST(JSON_EXTRACT(doc, '$.seq') AS UNSIGNED), 0) + 1)), version = version + 1`,
      { params: [Buffer.from(id)] });
      return result.affectedRows === 1 ? 1 : Number(result.insertId);
    }

    // Inserted is 1 row, taken is 2 and still held is 0, the connection doesn't count found rows
    async lease (id, owner, ttlMs) {
      const now = Date.now();
      const held = "(JSON_EXTRACT(doc, '$.name') = ? OR CAST(? AS SIGNED) - CAST(JSON_EXTRACT(doc, '$.lastRun') AS SIGNED) >= ?)";
      const [result] = await this.query(`INSERT INTO \`${this.table}\` (id, doc) VALUES (?, CAST(? AS JSON)) ON DUPLICATE KEY UPDATE
        version = IF(${held}, version + 1, version),
        doc = IF(${held}, JSON_SET(doc, '$.name', ?, '$.lastRun', CAST(? AS SIGNED)), doc)`,
      { params: [Buffer.from(id), jsonParam({ name: owner, lastRun: now, enabled: false }), owner, String(now), ttlMs, owner, String(now), ttlMs, owner, String(now)] });
      return result.affectedRows > 0;
    }
  }

  // Each field of a typed collection is its own column. Columns a newer version added are read
  // and written too, a doc only has the fields that aren't null.
  class MySQLTypedCollection extends MySQLBase {
    #columns = new Map(); // field -> number, string or boolean
    op = (field, o, c, q) => this.#typedOp(field, o, c, q);

    // Columns are only added when missing, an ALTER waits for the table's metadata lock even when it exists
    async createTables () {
      const t = `\`${this.table}\``;
      await this.ddl(`CREATE TABLE IF NOT EXISTS ${t} (id ${ID_TYPE} NOT NULL PRIMARY KEY, version INT NOT NULL DEFAULT 1) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_bin`);
      const want = ArkimeDb.fieldTypes(this.options).map(([field, type]) => {
        if (field.includes('.')) { throw new Error(`typed ${this.table} field '${field}' can't be nested`); }
        if (!MYSQL_TYPES[type]) { throw new Error(`typed ${this.table} field '${field}' has unsupported type '${type}'`); }
        return [field, MYSQL_TYPES[type]];
      });
      const have = await this.columns();
      const missing = want.filter(([field]) => !have.has(field));
      if (missing.length) {
        await this.ddl(`ALTER TABLE ${t} ${missing.map(([field, type]) => `ADD COLUMN \`${field}\` ${type}`).join(', ')}`);
      }
      this.#columns = await this.columns();

      const haveIndexes = await this.indexes();
      const sorted = new Set(ArkimeDb.sortIndexedFields(this.options));
      for (const field of ArkimeDb.indexedFields(this.options)) {
        const col = ArkimeDb.fieldColumn(field);
        if (!haveIndexes.has(col.toLowerCase())) { await this.ddl(`CREATE INDEX ${col} ON ${t} (\`${field}\`)`); }
        if (sorted.has(field) && !haveIndexes.has(`${col}_nl`.toLowerCase())) { await this.ddl(`CREATE INDEX ${col}_nl ON ${t} ((\`${field}\` IS NULL), \`${field}\`)`); }
      }
    }

    docSelect (fields) {
      const known = fields?.filter(f => this.#columns.has(f));
      return (known?.length ? known : [...this.#columns.keys()]).map(f => `\`${f}\``).join(', ') || 'NULL AS none';
    }

    toDoc (row) {
      const doc = {};
      for (const [field, kind] of this.#columns) {
        const v = row[field];
        if (v === null || v === undefined) { continue; }
        doc[field] = kind === 'boolean' ? !!v : kind === 'number' ? Number(v) : v;
      }
      return doc;
    }

    sortBy (field, dir) {
      if (!this.#columns.has(field)) { return []; }
      return dir === 'desc' ? [`\`${field}\` DESC`] : [`(\`${field}\` IS NULL)`, `\`${field}\` ASC`];
    }

    groupKey (field) {
      if (!this.#columns.has(field)) { return null; }
      return { from: '', key: `\`${field}\``, numeric: this.#columns.get(field) === 'number' };
    }

    metricValue (field) {
      return this.#columns.get(field) === 'number' ? `\`${field}\`` : 'NULL';
    }

    #checkDoc (doc) {
      for (const field of Object.keys(doc)) {
        if (!this.#columns.has(field)) { throw new Error(`${this.table} has no column for '${field}'`); }
      }
    }

    #value (field, v) {
      if (v === undefined || v === null) { return null; }
      return this.#columns.get(field) === 'boolean' ? (v ? 1 : 0) : v;
    }

    // Every column is written, so fields missing from the doc become null like a doc replace
    async put (id, doc, { createOnly } = {}) {
      await this.init();
      this.#checkDoc(doc);
      id ??= this.newId();
      const fields = [...this.#columns.keys()];
      const cols = fields.map(f => `\`${f}\``);
      const upsert = createOnly ? '' : ` AS new ON DUPLICATE KEY UPDATE ${cols.map(c => `${c} = new.${c}`).join(', ')}, version = version + 1`;
      try {
        await this.query(`INSERT INTO \`${this.table}\` (id${cols.map(c => ', ' + c).join('')}) VALUES (?${', ?'.repeat(fields.length)})${upsert}`,
          { params: [Buffer.from(id), ...fields.map(f => this.#value(f, doc[f]))] });
      } catch (err) {
        if (createOnly && err.errno === ER_DUP_ENTRY) { throw new ArkimeDb.ConflictError(id); }
        throw err;
      }
      return id;
    }

    async putIfVersion (id, doc, version) {
      await this.init();
      this.#checkDoc(doc);
      const fields = [...this.#columns.keys()];
      const [result] = await this.query(`UPDATE \`${this.table}\` SET ${fields.map(f => `\`${f}\` = ?`).join(', ')}, version = version + 1 WHERE id = ? AND version = ?`,
        { params: [...fields.map(f => this.#value(f, doc[f])), Buffer.from(id), version] });
      if (result.affectedRows !== 1) { throw new ArkimeDb.ConflictError(id, `Document '${id}' was changed or deleted`); }
    }

    async update (id, fields) {
      await this.init();
      this.#checkDoc(fields);
      const names = Object.keys(fields);
      if (!names.length) { return (await this.get(id)) !== null; }
      const [result] = await this.query(`UPDATE \`${this.table}\` SET ${names.map(f => `\`${f}\` = ?`).join(', ')}, version = version + 1 WHERE id = ?`,
        { params: [...names.map(f => this.#value(f, fields[f])), Buffer.from(id)] });
      return result.affectedRows > 0;
    }

    async nextSequence () {
      throw new Error(`nextSequence isn't supported on typed ${this.table}`);
    }

    async lease () {
      throw new Error(`lease isn't supported on typed ${this.table}`);
    }

    // A typed column holds one value of its type, a field with no column is never there
    #typedOp (field, op, c, q) {
      if (field === 'id') { return idOp(`\`${this.table}\``, op, c, q); }
      const kind = this.#columns.get(field);
      if (op === '$exists') { return kind && c ? `\`${field}\` IS NOT NULL` : kind ? `\`${field}\` IS NULL` : c ? 'FALSE' : 'TRUE'; }
      if (!kind) { return 'FALSE'; }

      const col = `\`${field}\``;
      // A value only matches a column it could be stored in, like an ES term on the wrong type
      const fits = (v) => kind === 'number' ? (typeof v === 'number' || (typeof v === 'string' && v.trim() !== '' && Number.isFinite(Number(v)))) : kind === 'string' ? (typeof v === 'string' || typeof v === 'number') : typeof v === 'boolean';
      const cast = (v) => kind === 'number' ? Number(v) : kind === 'string' ? String(v) : (v ? 1 : 0);
      let test = 'FALSE';
      switch (op) {
      case '$eq':
        if (fits(c)) { test = `${col} = ${q.p(cast(c))}`; }
        break;
      case '$in': {
        const vals = c.filter(fits).map(cast);
        if (vals.length) { test = `${col} IN (${q.list(vals)})`; }
        break;
      }
      case '$gt': case '$gte': case '$lt': case '$lte': {
        const cmp = { $gt: '>', $gte: '>=', $lt: '<', $lte: '<=' }[op];
        if (kind === 'number' && typeof c === 'number') { test = `${col} ${cmp} ${q.p(c)}`; }
        if (kind === 'string' && typeof c === 'string') { test = `${col} ${cmp} ${q.p(c)}`; }
        break;
      }
      case '$contains':
        if (kind === 'string') { test = `INSTR(${col}, ${q.p(String(c))}) > 0`; }
        break;
      case '$icontains':
        if (kind === 'string') { test = `INSTR(LOWER(${col}), LOWER(${q.p(String(c))})) > 0`; }
        break;
      case '$prefix':
        if (kind === 'string') { test = `${col} LIKE ${q.p(prefixPattern(c))} ESCAPE '!'`; }
        break;
      case '$wildcard':
        if (kind === 'string') { q.patterns = true; test = `${col} LIKE ${q.p(likePattern(c))} ESCAPE '!'`; }
        break;
      case '$regex':
        if (kind === 'string') { q.patterns = true; test = `REGEXP_LIKE(${col}, ${q.p(regexPattern(c))}, 'cn')`; }
        break;
      case '$cidr':
        if (kind === 'string') { test = inCidr(col, c, q); }
        break;
      }
      return `(${col} IS NOT NULL AND ${test})`;
    }
  }

  return { MySQLCollection, MySQLTypedCollection };
}

// Like the OpenSearch/Elasticsearch connections, a remote server's certificate is verified with caTrustFile
// or the system CAs, and --insecure still requires TLS. ssl-mode, ssl-ca, ssl-cert and ssl-key in the url
// win, loopback and unix socket servers are left alone.
function sslOptions (url, options) {
  const params = url.searchParams;
  const mode = params.get('ssl-mode')?.toUpperCase();
  if (mode === 'DISABLED') { return undefined; }
  if (mode !== undefined && !['REQUIRED', 'VERIFY_CA', 'VERIFY_IDENTITY'].includes(mode)) {
    throw new Error(`MySQL ssl-mode '${mode}' isn't one of DISABLED, REQUIRED, VERIFY_CA or VERIFY_IDENTITY`);
  }
  if (mode || params.has('ssl-ca') || params.has('ssl-cert')) {
    const tls = { rejectUnauthorized: mode !== 'REQUIRED' };
    if (params.has('ssl-ca')) { tls.ca = fs.readFileSync(params.get('ssl-ca')); }
    if (params.has('ssl-cert')) { tls.cert = fs.readFileSync(params.get('ssl-cert')); }
    if (params.has('ssl-key')) { tls.key = fs.readFileSync(params.get('ssl-key')); }
    if (mode === 'VERIFY_CA') { tls.checkServerIdentity = () => undefined; }
    return tls;
  }
  if (params.has('socket') || ArkimeConfig.isLoopbackHost(MySQLConnection.tlsHost(url.href))) { return undefined; }
  return ArkimeUtil.tlsOptionsOrExit({
    insecure: options.insecure ?? ArkimeConfig.insecure,
    caTrustFile: options.caTrustFile ?? ArkimeConfig.get('caTrustFile')
  }, 'MySQL');
}

class MySQLConnection {
  pool;
  prefix;
  #closed = false;
  #collections = new Map();
  #slots = { free: PATTERN_SLOTS, waiting: [] };

  /**
   * mysql://user:password@host[:port]/database[?socket=/path&ssl-mode=...&ssl-ca=...&connectionLimit=10]
   */
  constructor (url, options) {
    this.prefix = options.prefix ?? '';
    if (options.pool) {
      this.pool = options.pool;
      this.#slots = options.slots;
      return;
    }

    const u = new URL(url);
    const database = decodeURIComponent(u.pathname.slice(1));
    if (!database) { throw new Error('A mysql:// url needs a database, mysql://user:pass@host/database'); }
    const mysql = require('mysql2/promise');
    this.pool = mysql.createPool({
      host: u.hostname.replace(/^\[|\]$/g, '') || 'localhost',
      port: u.port ? Number(u.port) : 3306,
      socketPath: u.searchParams.get('socket') ?? undefined,
      user: decodeURIComponent(u.username),
      password: decodeURIComponent(u.password),
      database,
      ssl: sslOptions(u, options),
      connectionLimit: Number(u.searchParams.get('connectionLimit') ?? 10),
      maxPreparedStatements: 500,
      // JSON_TYPE and JSON_UNQUOTE answer in utf8mb4_bin, so literals use it too, data compares say COLLATE utf8mb4_0900_bin
      charset: 'utf8mb4_bin',
      supportBigNumbers: true,
      // Changed rows, not matched, so an upsert that keeps the row says 0
      flags: ['-FOUND_ROWS'],
      enableKeepAlive: true,
      // Idle connections close like a pg pool's, so a tool that is done can exit
      maxIdle: 0,
      idleTimeout: 10000
    });
    // Each connection gets the same sql_mode, whatever the server's default is
    this.pool.pool.on('connection', (c) => {
      c.query("SET SESSION sql_mode = 'STRICT_ALL_TABLES,NO_ENGINE_SUBSTITUTION', SESSION lock_wait_timeout = 30, SESSION time_zone = '+00:00'");
      c.on('error', (err) => console.log('ERROR - MySQL connection lost:', err.message));
    });
  }

  #version;
  // The oldest MySQL this is written for, it uses that release's JSON, index and optimizer hint support
  static MIN_VERSION = [26, 10];

  checkVersion () {
    this.#version ??= this.pool.query('SELECT VERSION() AS v').then(([rows]) => {
      const [major, minor] = rows[0].v.split(/[.-]/).map(Number);
      const [wantMajor, wantMinor] = MySQLConnection.MIN_VERSION;
      if (major < wantMajor || (major === wantMajor && minor < wantMinor)) {
        throw new Error(`MySQL ${rows[0].v} is too old, mysql:// needs ${wantMajor}.${wantMinor} or newer`);
      }
    }).catch((err) => {
      this.#version = undefined;
      throw err;
    });
    return this.#version;
  }

  withPrefix (prefix) {
    return prefix === this.prefix ? this : new MySQLConnection(undefined, { pool: this.pool, slots: this.#slots, prefix });
  }

  collection (collName, options = {}) {
    let coll = this.#collections.get(collName);
    if (!coll) {
      const { MySQLCollection, MySQLTypedCollection } = MySQLConnection.#classes ??= defineCollections();
      coll = new (options.typed ? MySQLTypedCollection : MySQLCollection)(this, collName, options);
      this.#collections.set(collName, coll);
    }
    return coll;
  }

  static #classes;

  // A deadlock between two upserts is retried, the server already rolled back the loser
  async run (sql, q) {
    if (q.patterns) { return this.#patternRun(sql, q.params); }
    for (let tries = 0; ; tries++) {
      try {
        return await this.pool.execute(sql, q.params);
      } catch (err) {
        if (err.errno !== ER_LOCK_DEADLOCK || tries >= 3) { throw err; }
      }
    }
  }

  // A user's pattern can run long, so each SELECT runs with mysqlPatternTimeout and only a couple
  // at a time, the rest of the pool stays free for everything else
  async #patternRun (sql, params) {
    const slots = this.#slots;
    if (slots.free > 0) {
      slots.free--;
    } else {
      if (slots.waiting.length >= PATTERN_QUEUE) { throw new Error('Too many $regex/$wildcard searches running, try again'); }
      await new Promise(resolve => slots.waiting.push(resolve));
    }
    try {
      const timeout = Math.max(0, parseInt(ArkimeConfig.get('mysqlPatternTimeout', 60), 10) || 0) * 1000;
      return await this.pool.execute(sql.replace(/^SELECT /, `SELECT /*+ MAX_EXECUTION_TIME(${timeout}) */ `), params);
    } finally {
      const next = slots.waiting.shift();
      if (next) { next(); } else { slots.free++; }
    }
  }

  /**
   * The host the connection goes to, undefined for a unix socket
   */
  static tlsHost (url) {
    try {
      const u = new URL(url);
      if (u.searchParams.has('socket')) { return undefined; }
      return u.hostname || 'localhost';
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

exports.registerDb = (api) => {
  ({ ArkimeDb, ArkimeConfig, ArkimeUtil } = api);
  api.registerDbScheme('mysql', MySQLConnection, { cluster: true });
};
exports.MySQLConnection = MySQLConnection;
