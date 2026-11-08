/* arkimeDb.es.js  -- Elasticsearch/OpenSearch collections for ArkimeDb
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const util = require('util');
const ArkimeDb = require('./arkimeDb');

// Each collection is an index, <prefix><name> unless es.index is set. Indices
// are only created when fields or es.create are set and es.dbpl isn't, otherwise db.pl owns them.
// With es.writeIndex the index is a read pattern and each put goes to
// writeIndex(), so gets and deletes by id search across the pattern.
// Every call takes an optional cluster, which multi cluster viewers route on.
class ESCollection extends ArkimeDb.Collection {
  #client;
  #index;
  #writeIndex;
  #multiES;

  constructor (client, prefix, collName, options, multiES) {
    super(collName, options);
    this.#client = client;
    this.#multiES = multiES;
    this.#index = options.es?.index ?? `${prefix}${collName}`;
    this.#writeIndex = options.es?.writeIndex;
  }

  get index () {
    return this.#index;
  }

  async init () {
    const { create, dbpl } = this.options.es ?? {};
    const fields = this.options.fields;
    if (dbpl || (!create && !fields)) { return; }

    const mappings = { ...create?.mappings };
    if (fields) {
      mappings.properties = { ...ArkimeDb.esProperties(fields), ...mappings.properties };
    }

    try {
      await this.#client.indices.create({ index: this.#index, body: { settings: create?.settings } });
    } catch (err) {
      if (err.meta?.body?.error?.type !== 'resource_already_exists_exception') {
        console.log(`ERROR - creating ${this.#index}`, util.inspect(err, false, 10));
        throw err;
      }
    }

    if (Object.keys(mappings).length) {
      await this.#client.indices.putMapping({ index: this.#index, body: mappings });
    }
  }

  async get (id, { cluster } = {}) {
    if (this.#writeIndex) {
      const { body: { hits } } = await this.#client.search({ index: this.#index, body: { query: { ids: { values: [id] } }, size: 1 }, cluster });
      return hits.hits[0]?._source ?? null;
    }
    try {
      const { body } = await this.#client.get({ index: this.#index, id, cluster });
      return body._source ?? null;
    } catch (err) {
      if (err.meta?.statusCode === 404) { return null; }
      throw err;
    }
  }

  async put (id, doc, { createOnly, refresh = true, cluster } = {}) {
    const params = { index: this.#writeIndex ? this.#writeIndex() : this.#index, body: doc, refresh, cluster };
    if (id !== null && id !== undefined) { params.id = id; }
    if (createOnly) { params.op_type = 'create'; }
    try {
      const { body } = await this.#client.index(params);
      return body._id;
    } catch (err) {
      if (createOnly && err.meta?.statusCode === 409) { throw new ArkimeDb.ConflictError(id); }
      throw err;
    }
  }

  async getVersioned (id, { cluster } = {}) {
    if (this.#writeIndex) { throw new Error(`getVersioned isn't supported on ${this.#index}`); }
    try {
      const { body } = await this.#client.get({ index: this.#index, id, cluster });
      return body._source ? { doc: body._source, version: { seqNo: body._seq_no, primaryTerm: body._primary_term } } : null;
    } catch (err) {
      if (err.meta?.statusCode === 404) { return null; }
      throw err;
    }
  }

  // A missing doc or a newer write is a 409
  async putIfVersion (id, doc, version, { refresh = true, cluster } = {}) {
    if (this.#writeIndex) { throw new Error(`putIfVersion isn't supported on ${this.#index}`); }
    try {
      await this.#client.index({ index: this.#index, id, body: doc, refresh, cluster, if_seq_no: version.seqNo, if_primary_term: version.primaryTerm });
    } catch (err) {
      if (err.meta?.statusCode === 409) { throw new ArkimeDb.ConflictError(id, `Document '${id}' was changed or deleted`); }
      throw err;
    }
  }

  async update (id, fields, { refresh = true, cluster } = {}) {
    if (this.#writeIndex) { throw new Error(`update isn't supported on ${this.#index}`); }
    try {
      await this.#client.update({ index: this.#index, id, body: { doc: fields }, retry_on_conflict: 3, refresh, cluster });
      return true;
    } catch (err) {
      if (err.meta?.statusCode === 404) { return false; }
      throw err;
    }
  }

  async delete (id, { refresh = true, cluster } = {}) {
    let index = this.#index;
    if (this.#writeIndex) {
      // Which dated index holds it, a delete by query would and multies has no route for one
      const { body: { hits } } = await this.#client.search({ index, body: { query: { ids: { values: [id] } }, size: 1, _source: false }, cluster });
      if (!hits.hits[0]) { return false; }
      index = hits.hits[0]._index;
    }
    try {
      // multies answers 200 with the cluster's body, a miss or failure is only in it
      const { body } = await this.#client.delete({ index, id, refresh, cluster });
      if (body?.error) { throw new Error(typeof body.error === 'string' ? body.error : body.error.reason ?? JSON.stringify(body.error)); }
      return body?.result !== 'not_found';
    } catch (err) {
      if (err.meta?.statusCode === 404) { return false; }
      throw err;
    }
  }

  async deleteWhere (where, { cluster } = {}) {
    const { body } = await this.#client.deleteByQuery({
      index: this.#index, body: { query: compile(where) }, refresh: true, conflicts: 'proceed', cluster
    });
    return body.deleted;
  }

  // The search body find sends, so validators like esProxy's can be tested against it
  searchBody ({ where, sort, from = 0, size, fields } = {}) {
    const body = {
      query: compile(where),
      from,
      size: size ?? 1000,
      track_total_hits: true
    };
    const sortList = ArkimeDb.sortList(sort);
    // One field goes as an object like the rest of viewer sends, multies only re-sorts arrays
    const sorts = sortList.map(([field, order]) => ({ [field === 'id' ? '_id' : field]: { order, missing: '_last' } }));
    if (sorts.length === 1) {
      body.sort = sorts[0];
    } else if (sorts.length > 1) {
      body.sort = sorts;
    }
    if (fields) { body._source = fields; }
    return body;
  }

  // With no size every match is returned, using scroll past the first 1000. ES caps
  // from + size at 10000 so bigger pages scroll too, multies does that scroll itself.
  // Hits also carry the ES index they came from.
  async find ({ where, sort, from = 0, size, version, fields, cluster } = {}) {
    // Callers pass query string values, '100' + '50' would be '10050'
    from = Math.max(0, +from || 0);
    if (size !== undefined) { size = Math.max(0, +size || 0); }
    const body = this.searchBody({ where, sort, from, size, fields });

    const toHit = (h) => {
      const hit = { id: h._id, doc: h._source, index: h._index };
      if (version) { hit.version = h._version; }
      return hit;
    };

    const params = { index: this.#index, body, rest_total_hits_as_int: true, version: !!version, cluster };
    if (size !== undefined && (from + size <= 10000 || this.#multiES)) {
      const { body: { hits } } = await this.#client.search(params);
      return { total: hits.total, data: hits.hits.map(toHit) };
    }

    // Like ES max_result_window, a deep from would buffer every hit before it
    if (from > 10000) { throw new Error(`Result window is too large, from ${from} is past 10000`); }

    // ES won't scroll with a from, so skip the first hits here instead
    body.from = 0;
    body.size = 1000;
    let { body: resp } = await this.#client.search({ ...params, scroll: '1m' });
    const total = resp.hits.total;
    const want = Math.min(total, size === undefined ? total : from + size);
    const data = [];
    while (resp.hits.hits.length > 0) {
      data.push(...resp.hits.hits.map(toHit));
      if (data.length >= want) { break; }
      ({ body: resp } = await this.#client.scroll({ scroll_id: resp._scroll_id, scroll: '1m', rest_total_hits_as_int: true, cluster }));
    }
    if (resp._scroll_id) {
      try { await this.#client.clearScroll({ body: { scroll_id: [resp._scroll_id] }, cluster }); } catch (err) { }
    }
    return { total, data: data.slice(from, want) };
  }

  async count (where, { cluster } = {}) {
    try {
      const { body } = await this.#client.count({ index: this.#index, body: { query: compile(where) }, ignore_unavailable: true, cluster });
      return body.count;
    } catch (err) {
      if (err.meta?.statusCode === 404) { return 0; }
      throw err;
    }
  }

  async aggregate ({ where, groupBy, metrics = {}, size = 1000, cluster } = {}) {
    ArkimeDb.checkMetrics(metrics);
    const aggs = {};
    for (const [metricName, metric] of Object.entries(metrics)) {
      const [op, field] = Object.entries(metric)[0];
      aggs[metricName] = { [op]: { field } };
    }
    const { body } = await this.#client.search({
      index: this.#index,
      body: {
        query: compile(where),
        size: 0,
        aggregations: { groups: { terms: { field: groupBy === 'id' ? '_id' : ArkimeDb.checkField(groupBy), size }, aggregations: aggs } }
      },
      rest_total_hits_as_int: true,
      cluster
    });
    return (body.aggregations?.groups?.buckets ?? []).map(b => {
      const bucket = { key: b.key, count: b.doc_count };
      for (const metricName of Object.keys(metrics)) { bucket[metricName] = b[metricName]?.value ?? null; }
      return bucket;
    });
  }

  // Indexing the doc bumps its _version, the counter capture also uses
  async nextSequence (id, { cluster } = {}) {
    const { body } = await this.#client.index({ index: this.#index, id, body: {}, cluster });
    return body._version;
  }

  // Take or renew a lease on doc id for owner, true if we hold it. Someone
  // else's lease can be taken once it is ttlMs old.
  async lease (id, owner, ttlMs) {
    try {
      const { body } = await this.#client.update({
        index: this.#index,
        id,
        body: {
          script: {
            lang: 'painless',
            source: `
              if (ctx._source.name == params.name) {
                ctx._source.lastRun = ctx['_now'];
              } else if (ctx['_now'] - ctx._source.lastRun >= params.ttl) {
                ctx._source.lastRun = ctx['_now'];
                ctx._source.name = params.name;
              } else {
                ctx.op = "none";
              }`,
            params: { name: owner, ttl: ttlMs }
          }
        }
      });
      return body.result === 'updated';
    } catch (err) {
      // There was no entry to update, just try and create a new record as ourself
      try {
        await this.put(id, { name: owner, lastRun: Date.now(), enabled: false }, { createOnly: true, refresh: false });
        return true;
      } catch (err2) {
        return false;
      }
    }
  }

  // Meta lives in the index mapping _meta, which putMapping replaces as a whole
  async #allMeta () {
    const { body } = await this.#client.indices.getMapping({ index: this.#index });
    return body[Object.keys(body)[0]]?.mappings?._meta ?? {};
  }

  async getMeta (key) {
    return (await this.#allMeta())[key];
  }

  async setMeta (obj) {
    const meta = { ...(await this.#allMeta()), ...obj };
    await this.#client.indices.putMapping({ index: this.#index, body: { _meta: meta } });
  }

  async refresh ({ cluster } = {}) {
    await this.#client.indices.refresh({ index: this.#index, cluster });
  }

  async flush ({ cluster } = {}) {
    await this.#client.indices.flush({ index: this.#index, cluster });
  }

  // The index exists, with es.checkMapping its indexed keyword fields are keywords
  async check () {
    try {
      await this.#client.indices.stats({ index: this.#index });
    } catch (err) {
      throw new Error(`Issue with '${this.#index}' index, make sure 'db/db.pl <host:port> init' has been run`, { cause: err });
    }
    const keywords = Object.entries(this.options.fields ?? {})
      .filter(([, spec]) => typeof spec === 'object' && spec.type === 'keyword' && spec.indexed).map(([field]) => field);
    if (!this.options.es?.checkMapping || !keywords.length) { return; }
    const { body } = await this.#client.indices.getMapping({ index: this.#index });
    for (const index of Object.values(body)) {
      const bad = keywords.find(f => index.mappings?.properties?.[f] && index.mappings.properties[f].type !== 'keyword');
      if (bad) {
        throw new Error(`Issue with '${this.#index}' index, ${bad} isn't a keyword, use 'db/db.pl <host:port> repair' to fix`);
      }
    }
  }
}

function esField (field) {
  return field === 'id' ? '_id' : field;
}

function wildcardEscape (str) {
  return String(str).replace(/[\\*?]/g, '\\$&');
}

function compileOp (field, op, c) {
  const f = esField(field);
  switch (op) {
  case '$eq': return field === 'id' ? { ids: { values: [c] } } : { term: { [f]: c } };
  case '$ne': return { bool: { must_not: [compileOp(field, '$eq', c)] } };
  case '$in': return field === 'id' ? { ids: { values: c } } : { terms: { [f]: c } };
  case '$nin': return { bool: { must_not: [compileOp(field, '$in', c)] } };
  case '$exists': return c ? { exists: { field: f } } : { bool: { must_not: [{ exists: { field: f } }] } };
  case '$gt': return { range: { [f]: { gt: c } } };
  case '$gte': return { range: { [f]: { gte: c } } };
  case '$lt': return { range: { [f]: { lt: c } } };
  case '$lte': return { range: { [f]: { lte: c } } };
  case '$contains': return { wildcard: { [f]: { value: `*${wildcardEscape(c)}*` } } };
  case '$icontains': return { wildcard: { [f]: { value: `*${wildcardEscape(c)}*`, case_insensitive: true } } };
  case '$prefix': return { prefix: { [f]: c } };
  case '$wildcard':
    if (field === 'id') { throw new Error('$wildcard is not supported on id'); }
    return { wildcard: { [f]: { value: String(c) } } };
  case '$regex':
    if (field === 'id') { throw new Error('$regex is not supported on id'); }
    return { regexp: { [f]: { value: String(c) } } };
  // The field must be mapped as ip, which takes a cidr in a term
  case '$cidr': return { term: { [f]: String(c) } };
  }
}

function compile (where) {
  if (!where) { return { match_all: {} }; }
  const filter = [];
  for (const [key, cond] of Object.entries(where)) {
    if (key === '$or') {
      filter.push(cond.length ? { bool: { should: cond.map(compile), minimum_should_match: 1 } } : { match_none: {} });
    } else if (key === '$and') {
      filter.push(...cond.map(compile));
    } else if (key === '$not') {
      filter.push({ bool: { must_not: [compile(cond)] } });
    } else {
      ArkimeDb.checkField(key);
      for (const [op, c] of ArkimeDb.condOps(cond)) {
        filter.push(compileOp(key, op, c));
      }
    }
  }
  return filter.length ? { bool: { filter } } : { match_all: {} };
}

// The reverse of compile, for multies answering the viewer's queries from a non ES cluster db
function decompileOne (type, clause) {
  const entries = Object.entries(clause ?? {});
  if (entries.length !== 1) { throw new Error(`Unsupported ${type} query`); }
  const [f, spec] = entries[0];
  const field = f === '_id' ? 'id' : f;
  const value = (spec !== null && typeof spec === 'object' && !Array.isArray(spec)) ? spec.value : spec;

  switch (type) {
  case 'term': return { [field]: value };
  case 'terms': return { [field]: { $in: spec } };
  case 'prefix': return { [field]: { $prefix: value } };
  case 'regexp': return { [field]: { $regex: value } };
  case 'range': {
    const ops = {};
    for (const [op, c] of Object.entries(spec)) {
      if (!['gt', 'gte', 'lt', 'lte'].includes(op)) { throw new Error(`Unsupported range ${op}`); }
      ops[`$${op}`] = c;
    }
    return { [field]: ops };
  }
  case 'wildcard': {
    const contains = String(value).match(/^\*((?:[^\\*?]|\\.)*)\*$/);
    if (contains) {
      return { [field]: { [spec.case_insensitive ? '$icontains' : '$contains']: contains[1].replace(/\\(.)/g, '$1') } };
    }
    if (spec.case_insensitive) { throw new Error('Unsupported case_insensitive wildcard'); }
    return { [field]: { $wildcard: value } };
  }
  }
}

function decompile (query) {
  if (!query) { return {}; }
  const [type, clause] = Object.entries(query)[0] ?? [];
  const list = (x) => x === undefined ? [] : (Array.isArray(x) ? x : [x]);
  switch (type) {
  case 'match_all': return {};
  case 'match_none': return { $or: [] };
  case 'ids': return { id: { $in: clause.values } };
  case 'exists': return { [clause.field === '_id' ? 'id' : clause.field]: { $exists: true } };
  case 'bool': {
    const parts = [...list(clause.filter), ...list(clause.must)].map(decompile);
    const should = list(clause.should);
    if (should.length && (clause.minimum_should_match ?? (parts.length ? 0 : 1)) > 0) {
      parts.push({ $or: should.map(decompile) });
    }
    parts.push(...list(clause.must_not).map(q => ({ $not: decompile(q) })));
    return parts.length === 1 ? parts[0] : { $and: parts };
  }
  case 'term': case 'terms': case 'range': case 'prefix': case 'wildcard': case 'regexp':
    return decompileOne(type, clause);
  }
  throw new Error(`Unsupported ${type} query`);
}

class ESConnection {
  client;
  prefix;
  #multiES;
  #closed = false;
  #collections = new Map();

  constructor (url, options) {
    this.prefix = options.prefix ?? '';
    this.#multiES = !!options.multiES;
    this.client = options.client ?? createClient(url, options);
  }

  withPrefix (prefix) {
    return prefix === this.prefix ? this : new ESConnection(undefined, { client: this.client, prefix, multiES: this.#multiES });
  }

  async schemaVersion (minVersion) {
    return ArkimeUtil.checkArkimeSchemaVersion(this.client, this.prefix, minVersion);
  }

  collection (collName, options = {}) {
    let coll = this.#collections.get(collName);
    if (!coll) {
      coll = new ESCollection(this.client, this.prefix, collName, options, this.#multiES);
      this.#collections.set(collName, coll);
    }
    return coll;
  }

  async close () {
    if (this.#closed) { return; }
    this.#closed = true;
    await this.client.close();
  }
}

function createClient (url, options) {
  const { Client } = require('@elastic/elasticsearch');

  const esSSLOptions = ArkimeUtil.tlsOptionsOrExit(options, 'OpenSearch/Elasticsearch');

  const esOptions = {
    node: [options.node ?? (url ?? 'http://localhost:9200').split(',')].flat().map(u => u.replace(/^(elasticsearch|opensearch)/i, 'http')),
    maxRetries: 2,
    requestTimeout: (parseInt(options.requestTimeout) + 30) * 1000 || 330000,
    ssl: esSSLOptions
  };

  if (options.apiKey) {
    esOptions.auth = { apiKey: options.apiKey };
  } else if (options.basicAuth) {
    let basicAuth = options.basicAuth;
    if (!basicAuth.includes(':')) {
      basicAuth = Buffer.from(basicAuth, 'base64').toString();
    }
    basicAuth = ArkimeUtil.splitRemain(basicAuth, ':', 1);
    esOptions.auth = { username: basicAuth[0], password: basicAuth[1] };
  }

  return new Client(esOptions);
}

ESConnection.compile = compile;
ESConnection.decompile = decompile;

module.exports = ESConnection;

const ArkimeUtil = require('./arkimeUtil');
