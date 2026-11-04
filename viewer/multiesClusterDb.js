/******************************************************************************/
/* multiesClusterDb.js -- Answer the ES calls a multi viewer makes on the
 *                        cluster db indices from a cluster's Postgres clusterDbUrl
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';

const ESConnection = require('../common/arkimeDb.es.js');

// Only history has dated indices
const INDEX_RE = /^MULTIPREFIX_(?:(files|stats|dstats|fields|sequence|hunts|queries)(?:_v\d+)?|(history)(?:_v1(?:-[^,/]*)?)?)\*?$/;

/**
 * The cluster db collection a multi viewer request is for
 * @param {string} path - the request path, like /MULTIPREFIX_files/_search
 * @returns {string|undefined} the collection name
 */
function collectionName (path) {
  const match = path.split('/')[1]?.match(INDEX_RE);
  return match?.[1] ?? match?.[2];
}

function getPath (doc, field) {
  return field.split('.').reduce((o, k) => o?.[k], doc);
}

function sortList (sort) {
  return (Array.isArray(sort) ? sort : (sort ? [sort] : [])).map((s) => {
    const [field, spec] = typeof s === 'string' ? [s, 'asc'] : Object.entries(s)[0];
    return [field === '_id' ? 'id' : field, typeof spec === 'string' ? spec : (spec?.order ?? 'asc')];
  });
}

async function search (coll, index, query, body) {
  const where = ESConnection.decompile(body.query);
  const total = (count) => query.rest_total_hits_as_int === 'true' ? count : { value: count, relation: 'eq' };

  const aggs = body.aggregations ?? body.aggs;
  if (aggs) {
    const [aggName, agg] = Object.entries(aggs)[0];
    if (!agg.terms) { throw new Error('Only terms aggregations are supported'); }
    const metrics = {};
    for (const [metricName, metric] of Object.entries(agg.aggregations ?? agg.aggs ?? {})) {
      const [op, { field }] = Object.entries(metric)[0];
      metrics[metricName] = { [op]: field };
    }
    const groupBy = agg.terms.field === '_id' ? 'id' : agg.terms.field;
    const buckets = await coll.aggregate({ where, groupBy, metrics, size: agg.terms.size ?? 10 });
    return {
      hits: { total: total(await coll.count(where)), hits: [] },
      aggregations: {
        [aggName]: {
          buckets: buckets.map((b) => {
            const bucket = { key: b.key, doc_count: b.count };
            for (const metricName of Object.keys(metrics)) { bucket[metricName] = { value: b[metricName] }; }
            return bucket;
          })
        }
      }
    };
  }

  const sort = sortList(body.sort);
  // A scroll wants everything, there is no scroll id so callers stop after this
  const size = query.scroll ? undefined : (body.size ?? 10);
  const { total: n, data } = await coll.find({ where, sort, from: +body.from || 0, size, version: query.version === 'true' });
  return {
    hits: {
      total: total(n),
      hits: data.map((h) => {
        const hit = { _index: index, _id: h.id, _source: h.doc };
        if (h.version !== undefined) { hit._version = h.version; }
        if (sort.length) { hit.sort = sort.map(([f]) => f === 'id' ? h.id : getPath(h.doc, f)); }
        return hit;
      })
    }
  };
}

/**
 * The ES shaped response for a multi viewer request on a cluster db index
 * @param {object} coll - the cluster's ArkimeDb collection
 * @param {string} op - the operation of the multies route that matched, never taken from the path
 * @param {string} [id] - the route's :id param
 * @param {object} query - the request query parameters
 * @param {string} bodyStr - the request body
 */
async function answer (coll, op, id, query, bodyStr) {
  // The index names the ES cluster dbs use
  const index = coll.name === 'history' ? 'MULTIPREFIX_history_v1' : `MULTIPREFIX_${coll.name}_v30`;
  const body = bodyStr ? JSON.parse(bodyStr) : {};
  id ??= null;

  switch (op) {
  case 'search':
    return search(coll, index, query, body);
  case 'count':
    return { count: await coll.count(ESConnection.decompile(body.query)) };
  case 'refresh':
    return {};
  case 'get': {
    const doc = id === null ? null : await coll.get(id);
    return doc ? { _index: index, _id: id, found: true, _source: doc } : { _index: index, _id: id, found: false };
  }
  case 'delete':
    if (id === null) { break; }
    return { _index: index, _id: id, result: await coll.delete(id) ? 'deleted' : 'not_found' };
  case 'update':
    if (id === null) { break; }
    if (!body.doc) { throw new Error('Only doc updates are supported'); }
    return await coll.update(id, body.doc)
      ? { _index: index, _id: id, result: 'updated' }
      : { status: 404, error: { type: 'document_missing_exception', reason: `[${id}]: document missing` } };
  case 'history':
    if (coll.name !== 'history') { break; }
    // falls through
  case 'index':
    // Indexing an empty doc is how ES bumps a sequence
    if (coll.name === 'sequence' && id !== null && Object.keys(body).length === 0) {
      return { _index: index, _id: id, _version: await coll.nextSequence(id), result: 'updated' };
    }
    return { _index: index, _id: await coll.put(id, body), _version: 1, result: 'created' };
  }
  throw new Error(`Unsupported ${op ?? 'operation'} on ${coll.name}`);
}

module.exports = { collectionName, answer };
