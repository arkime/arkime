/******************************************************************************/
/* multiesAggs.js -- Merge aggregation results from several clusters into one,
 *                   for multies.js. Kept free of config and server state so
 *                   it can be unit tested.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';

// Aggregations are a pain, we convert from array to map to back to array
/// ///////////////////////////////////////////////////////////////////////////////

function agg2Obj (field, agg) {
  const obj = {};
  for (let i = 0; i < agg.length; i++) {
    obj[agg[i][field]] = agg[i];
  }
  return obj;
}

// Metric aggregation operators we can merge across clusters. Anything else
// (avg, percentiles, ...) can't be recombined from per-cluster results alone
// and is summed, which is what this code has always done.
const METRIC_OPS = ['sum', 'min', 'max', 'value_count', 'cardinality', 'avg'];

/**
 * The declared operator of each metric sub-aggregation of a bucket aggregation,
 * eg { bytes: 'sum', packets: 'sum', first: 'min' }, so merging buckets across
 * clusters can combine each sub-agg the way the query asked rather than
 * assuming they are all sums. apiSessions writes sub-aggs under `aggs`, while
 * apiStats and buildQuery use `aggregations`.
 * @returns {object|undefined} operator by sub-agg name (undefined if none)
 */
function subAggOps (aggConfig) {
  const subs = aggConfig?.aggs ?? aggConfig?.aggregations;
  if (!subs) { return undefined; }
  const ops = {};
  for (const subName in subs) {
    const op = METRIC_OPS.find(o => subs[subName]?.[o]);
    if (op) { ops[subName] = op; }
  }
  return Object.keys(ops).length ? ops : undefined;
}

/**
 * Combine two metric values per the aggregation's declared operator. A unique
 * count can't be recombined from per-cluster counts, so the larger one is kept
 * (a lower bound). An average is weighted by the bucket doc counts when the
 * caller has them, else summed as before.
 */
function mergeMetric (op, v1, v2, c1, c2) {
  if (op === 'min') { return Math.min(v1, v2); }
  if (op === 'max' || op === 'cardinality') { return Math.max(v1, v2); }
  if (op === 'avg' && c1 > 0 && c2 > 0) { return (v1 * c1 + v2 * c2) / (c1 + c2); }
  return v1 + v2;
}

/**
 * The terms sub-aggregations of a bucket aggregation (eg the summary widgets'
 * split, spigraphhierarchy's nested fields), with what merging their buckets
 * across clusters needs: the order, size, each metric sub-agg's operator and
 * the same again for their own terms sub-aggregations.
 * @returns {object|undefined} info by sub-agg name (undefined if none)
 */
function nestedTermsInfo (aggConfig) {
  const subs = aggConfig?.aggs ?? aggConfig?.aggregations;
  if (!subs) { return undefined; }
  const info = {};
  for (const subName in subs) {
    if (subs[subName]?.terms) {
      info[subName] = {
        order: subs[subName].terms.order,
        size: subs[subName].terms.size,
        subAggs: subAggOps(subs[subName]),
        nested: nestedTermsInfo(subs[subName])
      };
    }
  }
  return Object.keys(info).length ? info : undefined;
}

/**
 * Add one cluster's bucket into the merged bucket with the same key: the doc
 * counts, each metric sub-agg per its declared operator (defaulting to sum),
 * and each declared terms sub-agg bucket by bucket.
 */
function mergeBucket (b1, b2, subOps, nested) {
  const c1 = b1.doc_count;
  b1.doc_count += b2.doc_count;
  for (const sub in b2) {
    const v2 = b2[sub];
    if (v2 === null || typeof v2 !== 'object') { continue; }
    if (nested?.[sub] && Array.isArray(v2.buckets)) {
      if (Array.isArray(b1[sub]?.buckets)) {
        mergeNestedBuckets(b1[sub], v2, nested[sub]);
      } else {
        b1[sub] = v2;
      }
      continue;
    }
    if (typeof v2.value !== 'number') { continue; }
    if (b1[sub] === undefined) {
      b1[sub] = v2;
    } else if (typeof b1[sub].value === 'number') {
      b1[sub].value = mergeMetric(subOps?.[sub], b1[sub].value, v2.value, c1, b2.doc_count);
    }
  }
}

/**
 * Merge one cluster's nested terms buckets (an array, as ES returns them) into
 * the merged bucket's array: same keys merge, new keys append, then re-rank
 * by the declared order and cut back to the declared size.
 */
function mergeNestedBuckets (agg1, agg2, info) {
  const byKey = new Map(agg1.buckets.map(b => [b.key, b]));
  for (const b2 of agg2.buckets) {
    const b1 = byKey.get(b2.key);
    if (b1) {
      mergeBucket(b1, b2, info?.subAggs, info?.nested);
    } else {
      agg1.buckets.push(b2);
      byKey.set(b2.key, b2);
    }
  }
  if (agg1.sum_other_doc_count !== undefined && agg2.sum_other_doc_count !== undefined) {
    agg1.sum_other_doc_count += agg2.sum_other_doc_count;
  }
  agg1.buckets = agg2Arr(agg1.buckets, 'terms', info?.order);
  if (info?.size && agg1.buckets.length > info.size) { agg1.buckets.length = info.size; }
}

function agg2Arr (agg, type, order) {
  let arr = [];
  for (const attrname in agg) {
    arr.push(agg[attrname]);
  }

  if (type === 'histogram') {
    return arr.sort((a, b) => { return a.key - b.key; });
  }

  // terms: honor the order the query asked for (_count, _key or a metric
  // sub-agg's sum), matching what a single cluster returns; the ES default
  // is doc_count descending
  const orderKey = order ? Object.keys(order)[0] : '_count';
  const dir = order && order[orderKey] === 'asc' ? 1 : -1;
  const orderValue = (bucket) => {
    if (orderKey === '_count') { return bucket.doc_count; }
    if (orderKey === '_key') { return bucket.key; }
    return bucket[orderKey]?.value ?? 0;
  };
  arr = arr.sort((a, b) => {
    const av = orderValue(a);
    const bv = orderValue(b);
    if (av !== bv) {
      if (typeof av === 'string' || typeof bv === 'string') {
        return dir * String(av).localeCompare(String(bv));
      }
      return dir * (av - bv);
    }
    // ES breaks ties on the key, ascending, in the field's own term order.
    // Numeric keys we can reproduce. For string keys the right order depends
    // on the field type (an ip field orders numerically, a keyword field
    // lexicographically) and isn't recoverable from a response, so keep the
    // order the buckets were merged in — imposing one would disagree with what
    // a single cluster returns. Returning 0 rather than subtracting strings
    // (which yielded NaN) keeps the comparator consistent and the sort stable.
    if (typeof a.key === 'number' && typeof b.key === 'number') { return a.key - b.key; }
    return 0;
  });
  return arr;
}

/**
 * The empty merged aggregations for a search's aggregations, each carrying
 * what aggAdd needs to combine the clusters (operators, order, nested terms).
 */
function newAggregations (aggregations) {
  const result = {};
  for (const agg in aggregations) {
    // Check for metric aggregations (min, max, sum, avg, etc.)
    const aggConfig = aggregations[agg];
    if (aggConfig.min || aggConfig.max || aggConfig.sum || aggConfig.avg ||
        aggConfig.value_count || aggConfig.cardinality || aggConfig.stats ||
        aggConfig.extended_stats || aggConfig.percentiles) {
      // Metric aggregations have a value property - initialize to null
      // The first real value from a cluster will replace this
      result[agg] = { value: null, _op: METRIC_OPS.find(o => aggConfig[o]) };
    } else if (aggConfig.filter) {
      // Filter aggregations contain nested aggregations
      result[agg] = { doc_count: 0 };
      // Recursively initialize nested aggregations
      if (aggConfig.aggs || aggConfig.aggregations) {
        const nestedAggs = aggConfig.aggs || aggConfig.aggregations;
        for (const nestedAgg in nestedAggs) {
          if (nestedAggs[nestedAgg].histogram) {
            result[agg][nestedAgg] = { buckets: {}, _type: 'histogram', _subAggs: subAggOps(nestedAggs[nestedAgg]), doc_count_error_upper_bound: 0, sum_other_doc_count: 0 };
          } else {
            result[agg][nestedAgg] = { buckets: {}, _type: 'terms', _order: nestedAggs[nestedAgg].terms?.order, _subAggs: subAggOps(nestedAggs[nestedAgg]), doc_count_error_upper_bound: 0, sum_other_doc_count: 0 };
          }
        }
      }
    } else if (aggConfig.histogram) {
      result[agg] = { buckets: {}, _type: 'histogram', _subAggs: subAggOps(aggConfig), doc_count_error_upper_bound: 0, sum_other_doc_count: 0 };
    } else {
      result[agg] = { buckets: {}, _type: 'terms', _order: aggConfig.terms?.order, _subAggs: subAggOps(aggConfig), _nested: nestedTermsInfo(aggConfig), doc_count_error_upper_bound: 0, sum_other_doc_count: 0 };
    }
  }
  return result;
}

function aggConvert2Obj (aggs) {
  for (const aggname in aggs) {
    // Handle filter aggregations (have doc_count but no buckets or value at top level)
    if (aggs[aggname].doc_count !== undefined && !aggs[aggname].buckets && !aggs[aggname].value) {
      // Recursively convert nested aggregations within the filter
      for (const nestedAgg in aggs[aggname]) {
        if (nestedAgg !== 'doc_count' && aggs[aggname][nestedAgg] && aggs[aggname][nestedAgg].buckets) {
          aggs[aggname][nestedAgg].buckets = agg2Obj('key', aggs[aggname][nestedAgg].buckets);
        }
      }
    }
    // Handle standard bucket aggregations
    else if (aggs[aggname].buckets) {
      aggs[aggname].buckets = agg2Obj('key', aggs[aggname].buckets);
    }
  }
}

function aggConvert2Arr (aggs) {
  for (const aggname in aggs) {
    // Handle filter aggregations (have doc_count but no buckets or value at top level)
    if (aggs[aggname].doc_count !== undefined && !aggs[aggname].buckets && !aggs[aggname].value) {
      // Recursively convert nested aggregations within the filter
      for (const nestedAgg in aggs[aggname]) {
        if (nestedAgg !== 'doc_count' && aggs[aggname][nestedAgg] && aggs[aggname][nestedAgg].buckets) {
          aggs[aggname][nestedAgg].buckets = agg2Arr(aggs[aggname][nestedAgg].buckets, aggs[aggname][nestedAgg]._type, aggs[aggname][nestedAgg]._order);
          delete aggs[aggname][nestedAgg]._type;
          delete aggs[aggname][nestedAgg]._order;
          delete aggs[aggname][nestedAgg]._subAggs;
        }
      }
    }
    // Handle standard bucket aggregations
    else if (aggs[aggname].buckets) {
      aggs[aggname].buckets = agg2Arr(aggs[aggname].buckets, aggs[aggname]._type, aggs[aggname]._order);
      delete aggs[aggname]._type;
      delete aggs[aggname]._order;
      delete aggs[aggname]._subAggs;
      delete aggs[aggname]._nested;
    } else if (aggs[aggname].value !== undefined) {
      delete aggs[aggname]._op;
    }
  }
}

function aggAdd (obj1, obj2) {
  for (const aggname in obj2) {
    // Handle metric aggregations (min, max, sum) that have a 'value' property
    if (obj1[aggname].value !== undefined && obj2[aggname].value !== undefined) {
      // Skip if obj2 value is null (no results from that cluster)
      if (obj2[aggname].value === null) {
        continue;
      }
      // If obj1 value is null, use obj2 value
      if (obj1[aggname].value === null) {
        obj1[aggname].value = obj2[aggname].value;
        continue;
      }

      // The operator the query declared wins; the name is a fallback for
      // callers whose queries weren't seen (firstPacket/lastPacket style names)
      const op = obj1[aggname]._op;
      if (op) {
        obj1[aggname].value = mergeMetric(op, obj1[aggname].value, obj2[aggname].value);
      }
      // For min aggregations
      else if (aggname.toLowerCase().includes('min') || aggname === 'firstPacket') {
        obj1[aggname].value = Math.min(obj1[aggname].value, obj2[aggname].value);
      }
      // For max aggregations
      else if (aggname.toLowerCase().includes('max') || aggname === 'lastPacket') {
        obj1[aggname].value = Math.max(obj1[aggname].value, obj2[aggname].value);
      }
      // For sum aggregations and others, add the values
      else {
        obj1[aggname].value += obj2[aggname].value;
      }
      continue;
    }

    // Handle filter aggregations (have doc_count and nested aggregations)
    if (obj1[aggname].doc_count !== undefined && obj2[aggname].doc_count !== undefined &&
        !obj1[aggname].buckets) {
      obj1[aggname].doc_count += obj2[aggname].doc_count;
      // Recursively merge nested aggregations
      for (const nestedAgg in obj2[aggname]) {
        if (nestedAgg !== 'doc_count' && obj1[aggname][nestedAgg]) {
          // Recursively call aggAdd for nested aggregations
          const tempObj1 = { [nestedAgg]: obj1[aggname][nestedAgg] };
          const tempObj2 = { [nestedAgg]: obj2[aggname][nestedAgg] };
          aggAdd(tempObj1, tempObj2);
          obj1[aggname][nestedAgg] = tempObj1[nestedAgg];
        }
      }
      continue;
    }

    // Handle terms/buckets aggregations
    if (obj1[aggname].buckets && obj2[aggname].buckets) {
      if (obj1[aggname].doc_count_error_upper_bound !== undefined) {
        obj1[aggname].doc_count_error_upper_bound += obj2[aggname].doc_count_error_upper_bound;
      }
      if (obj1[aggname].sum_other_doc_count !== undefined) {
        obj1[aggname].sum_other_doc_count += obj2[aggname].sum_other_doc_count;
      }
      for (const entry in obj2[aggname].buckets) {
        if (!obj1[aggname].buckets[entry]) {
          obj1[aggname].buckets[entry] = obj2[aggname].buckets[entry];
        } else {
          // merge metric sub-aggs (the timeline's source.bytes/client.bytes, the
          // summary widgets' bytes/packets/mN, /api/stats' first, ...) and the
          // nested terms sub-aggs (the summary widgets' split, ...)
          mergeBucket(obj1[aggname].buckets[entry], obj2[aggname].buckets[entry], obj1[aggname]._subAggs, obj1[aggname]._nested);
        }
      }
    }
  }
}

module.exports = { newAggregations, aggConvert2Obj, aggAdd, aggConvert2Arr };
