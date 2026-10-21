/* arkimeDb.es.js  -- Elasticsearch/OpenSearch collections for ArkimeDb
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const fs = require('fs');
const util = require('util');
const ArkimeDb = require('./arkimeDb');

// Each collection is an index, <prefix><name> unless es.index is set. Indices
// are only created when es.create is set, otherwise db.pl owns them.
// With es.writeIndex the index is a read pattern and each put goes to
// writeIndex(), so gets and deletes by id search across the pattern.
// Every call takes an optional cluster, which multi cluster viewers route on.
class ESCollection extends ArkimeDb.Collection {
  #client;
  #index;
  #writeIndex;

  constructor (client, prefix, collName, options) {
    super(collName, options);
    this.#client = client;
    this.#index = options.es?.index ?? `${prefix}${collName}`;
    this.#writeIndex = options.es?.writeIndex;
  }

  get index () {
    return this.#index;
  }

  async init () {
    const create = this.options.es?.create;
    if (!create) { return; }

    try {
      await this.#client.indices.create({ index: this.#index, body: { settings: create.settings } });
    } catch (err) {
      if (err.meta?.body?.error?.type !== 'resource_already_exists_exception') {
        console.log(`ERROR - creating ${this.#index}`, util.inspect(err, false, 10));
        throw err;
      }
    }

    if (create.mappings) {
      await this.#client.indices.putMapping({ index: this.#index, body: create.mappings });
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
    if (this.#writeIndex) {
      return (await this.deleteWhere({ id }, { cluster })) > 0;
    }
    try {
      await this.#client.delete({ index: this.#index, id, refresh, cluster });
      return true;
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

  // With no size every match is returned, using scroll past the first 1000.
  // Hits also carry the ES index they came from.
  async find ({ where, sort, from = 0, size, version, fields, cluster } = {}) {
    const body = this.searchBody({ where, sort, from, size, fields });

    const toHit = (h) => {
      const hit = { id: h._id, doc: h._source, index: h._index };
      if (version) { hit.version = h._version; }
      return hit;
    };

    const params = { index: this.#index, body, rest_total_hits_as_int: true, version: !!version, cluster };
    if (size !== undefined) {
      const { body: { hits } } = await this.#client.search(params);
      return { total: hits.total, data: hits.hits.map(toHit) };
    }

    // ES won't scroll with a from, so skip the first hits here instead
    body.from = 0;
    let { body: resp } = await this.#client.search({ ...params, scroll: '1m' });
    const total = resp.hits.total;
    const data = [];
    while (resp.hits.hits.length > 0) {
      data.push(...resp.hits.hits.map(toHit));
      if (data.length >= total) { break; }
      ({ body: resp } = await this.#client.scroll({ scroll_id: resp._scroll_id, scroll: '1m', rest_total_hits_as_int: true, cluster }));
    }
    if (resp._scroll_id) {
      try { await this.#client.clearScroll({ body: { scroll_id: [resp._scroll_id] }, cluster }); } catch (err) { }
    }
    return { total, data: from > 0 ? data.slice(from) : data };
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

class ESConnection {
  client;
  prefix;
  #closed = false;
  #collections = new Map();

  constructor (url, options) {
    this.prefix = options.prefix ?? '';
    this.client = options.client ?? createClient(url, options);
  }

  collection (collName, options = {}) {
    let coll = this.#collections.get(collName);
    if (!coll) {
      coll = new ESCollection(this.client, this.prefix, collName, options);
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

  const esSSLOptions = { rejectUnauthorized: !options.insecure };
  if (options.caTrustFile) { esSSLOptions.ca = ArkimeUtil.certificateFileToArray(options.caTrustFile); }
  if (options.clientKey) {
    esSSLOptions.key = fs.readFileSync(options.clientKey);
    esSSLOptions.cert = fs.readFileSync(options.clientCert);
    if (options.clientKeyPass) {
      esSSLOptions.passphrase = options.clientKeyPass;
    }
  }

  const esOptions = {
    node: options.node ?? url.split(',').map(u => u.replace(/^(elasticsearch|opensearch)/, 'http')),
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

module.exports = ESConnection;

const ArkimeUtil = require('./arkimeUtil');
