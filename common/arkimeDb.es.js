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
class ESCollection extends ArkimeDb.Collection {
  #client;
  #index;

  constructor (client, prefix, collName, options) {
    super(collName, options);
    this.#client = client;
    this.#index = options.es?.index ?? `${prefix}${collName}`;
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

  async get (id) {
    try {
      const { body } = await this.#client.get({ index: this.#index, id });
      return body._source ?? null;
    } catch (err) {
      if (err.meta?.statusCode === 404) { return null; }
      throw err;
    }
  }

  async put (id, doc, { createOnly, refresh = true } = {}) {
    const params = { index: this.#index, body: doc, refresh };
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

  async update (id, fields, { refresh = true } = {}) {
    try {
      await this.#client.update({ index: this.#index, id, body: { doc: fields }, retry_on_conflict: 3, refresh });
      return true;
    } catch (err) {
      if (err.meta?.statusCode === 404) { return false; }
      throw err;
    }
  }

  async delete (id, { refresh = true } = {}) {
    try {
      await this.#client.delete({ index: this.#index, id, refresh });
      return true;
    } catch (err) {
      if (err.meta?.statusCode === 404) { return false; }
      throw err;
    }
  }

  async deleteWhere (where) {
    const { body } = await this.#client.deleteByQuery({
      index: this.#index, body: { query: compile(where) }, refresh: true, conflicts: 'proceed'
    });
    return body.deleted;
  }

  // With no size every match is returned, using scroll past the first 1000
  async find ({ where, sort, from = 0, size, version, fields } = {}) {
    const body = {
      query: compile(where),
      from,
      size: size ?? 1000,
      track_total_hits: true
    };
    const sortList = ArkimeDb.sortList(sort);
    if (sortList.length) {
      body.sort = sortList.map(([field, order]) => ({ [field === 'id' ? '_id' : field]: { order, missing: '_last' } }));
    }
    if (fields) { body._source = fields; }

    const toHit = (h) => {
      const hit = { id: h._id, doc: h._source };
      if (version) { hit.version = h._version; }
      return hit;
    };

    const params = { index: this.#index, body, rest_total_hits_as_int: true, version: !!version };
    if (size !== undefined) {
      const { body: { hits } } = await this.#client.search(params);
      return { total: hits.total, data: hits.hits.map(toHit) };
    }

    let { body: resp } = await this.#client.search({ ...params, scroll: '1m' });
    const total = resp.hits.total;
    const data = [];
    while (resp.hits.hits.length > 0) {
      data.push(...resp.hits.hits.map(toHit));
      if (data.length >= total) { break; }
      ({ body: resp } = await this.#client.scroll({ scroll_id: resp._scroll_id, scroll: '1m', rest_total_hits_as_int: true }));
    }
    if (resp._scroll_id) {
      try { await this.#client.clearScroll({ body: { scroll_id: [resp._scroll_id] } }); } catch (err) { }
    }
    return { total, data };
  }

  async count (where) {
    try {
      const { body } = await this.#client.count({ index: this.#index, body: { query: compile(where) } });
      return body.count;
    } catch (err) {
      if (err.meta?.statusCode === 404) { return 0; }
      throw err;
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

  async refresh () {
    await this.#client.indices.refresh({ index: this.#index });
  }

  async flush () {
    await this.#client.indices.flush({ index: this.#index });
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
