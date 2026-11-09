/* esQuery.js -- the ES query DSL the viewer builds as a small tree, so a sessions db that isn't
 *               OpenSearch/Elasticsearch only compiles the tree for its own query language
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

// A node is one of:
//   { all: true }, { none: true }, { and: [nodes] }, { or: [nodes] }, { not: node }
//   { field, op, value } where op is
//     eq, in            a value or any of the values, value is an array for in
//     lookup            a shortcut, value is the terms lookup { index, id, path }
//     range             value is [[gt|gte|lt|lte, bound], ...], one value of the field meets every bound
//     exists            the field has a value
//     prefix, wildcard  a string starts with value, or matches the * and ? pattern
//     regexp            the whole string matches the ES regexp value
//     match, phrase     ES match (contains, any case) and match_phrase (contains)
// The id is the field 'id'.

function defaultUnsupported (feature) {
  return new Error(`Unsupported ${feature}`);
}

function fieldValue (obj, key = 'value') {
  const field = Object.keys(obj)[0];
  const v = obj[field];
  return [field, v !== null && typeof v === 'object' && !Array.isArray(v) && key in v ? v[key] : v];
}

class EsQuery {
  // The _scroll_id a sessions db returns when its first answer had every hit, Db.scroll then has nothing more
  static SCROLL_DONE = 'arkime:done';

  /**
   * The tree for an ES query clause
   * @param {object} clause - The query, like body.query
   * @param {object} [opts]
   * @param {function} [opts.unsupported] - Makes the error thrown for a clause or syntax it can't do, given its name
   */
  static parse (clause, opts = {}) {
    const unsupported = opts.unsupported ?? defaultUnsupported;
    const parse = (c) => EsQuery.parse(c, opts);

    if (!clause || Object.keys(clause).length === 0 || clause.match_all) { return { all: true }; }
    if (clause.bool) { return EsQuery.#bool(clause.bool, parse, unsupported); }
    if (clause.term) {
      const [field, value] = fieldValue(clause.term);
      return { field, op: 'eq', value };
    }
    if (clause.terms) {
      const field = Object.keys(clause.terms)[0];
      const value = clause.terms[field];
      // An ES terms lookup ({ index, id, path }) is a shortcut reference
      if (value && !Array.isArray(value) && typeof value === 'object' && value.id !== undefined && value.path) {
        return { field, op: 'lookup', value };
      }
      return { field, op: 'in', value: Array.isArray(value) ? value : [value] };
    }
    if (clause.range) {
      const field = Object.keys(clause.range)[0];
      const range = clause.range[field];
      const value = ['gt', 'gte', 'lt', 'lte'].filter(op => range[op] !== undefined).map(op => [op, range[op]]);
      return value.length ? { field, op: 'range', value } : { all: true };
    }
    if (clause.exists) { return { field: clause.exists.field, op: 'exists' }; }
    if (clause.prefix) {
      const [field, value] = fieldValue(clause.prefix);
      return { field, op: 'prefix', value };
    }
    if (clause.wildcard) {
      const [field, value] = fieldValue(clause.wildcard);
      return { field, op: 'wildcard', value };
    }
    if (clause.regexp) {
      const [field, value] = fieldValue(clause.regexp);
      return { field, op: 'regexp', value };
    }
    if (clause.match) {
      const [field, value] = fieldValue(clause.match, 'query');
      return { field, op: 'match', value };
    }
    if (clause.match_phrase) {
      const [field, value] = fieldValue(clause.match_phrase, 'query');
      return { field, op: 'phrase', value };
    }
    if (clause.ids) { return { field: 'id', op: 'in', value: clause.ids.values || [] }; }
    if (clause.query_string) { return EsQuery.queryString(clause.query_string.query ?? clause.query_string, opts); }
    if (clause.nested) { throw unsupported('nested queries'); }
    if (clause.script) { throw unsupported('script queries'); }
    throw unsupported(`query clause ${Object.keys(clause).join(',')}`);
  }

  // must and filter all match, each must_not doesn't, and with nothing else at least one should does
  static #bool (bool, parse, unsupported) {
    const list = (items) => items === undefined ? [] : Array.isArray(items) ? items : [items];
    const and = [...list(bool.must), ...list(bool.filter)].map(parse);
    for (const item of list(bool.must_not)) { and.push({ not: parse(item) }); }
    const should = list(bool.should);
    const min = Number(bool.minimum_should_match ?? (and.length ? 0 : 1));
    if (min > 1) { throw unsupported('bool minimum_should_match > 1'); }
    if (min === 1 && should.length) { and.push({ or: should.map(parse) }); }
    if (and.length === 0) { return { all: true }; }
    return and.length === 1 ? and[0] : { and };
  }

  /**
   * The tree for the field:value query_string syntax the viewer uses: AND, OR, NOT and parentheses,
   * with OR between terms that have neither, ES's default operator
   */
  static queryString (query, opts = {}) {
    const unsupported = opts.unsupported ?? defaultUnsupported;
    if (typeof query !== 'string') { throw unsupported('query_string object syntax'); }
    if (/[[\]{}~^]|\bTO\b|[<>]=?/.test(query)) { throw unsupported('query_string advanced syntax'); }
    const tokens = query.match(/\(|\)|\bAND\b|\bOR\b|\bNOT\b|[^\s()]+/gi) || [];
    let pos = 0;
    const peek = () => tokens[pos]?.toUpperCase();

    const term = () => {
      const token = tokens[pos++];
      if (token === undefined) { throw unsupported('query_string syntax'); }
      if (token === '(') {
        const node = or();
        if (tokens[pos++] !== ')') { throw unsupported('query_string syntax'); }
        return node;
      }
      if (token.toUpperCase() === 'NOT') { return { not: term() }; }
      const match = token.match(/^([^:]+):(.+)$/);
      if (!match) { throw unsupported('query_string free text'); }
      const value = match[2].replace(/^"|"$/g, '');
      if (value.includes('*') || value.includes('?')) { return { field: match[1], op: 'wildcard', value }; }
      return { field: match[1], op: 'eq', value: value.replace(/\\([:\\])/g, '$1') };
    };
    const and = () => {
      const nodes = [term()];
      while (peek() === 'AND') { pos++; nodes.push(term()); }
      return nodes.length === 1 ? nodes[0] : { and: nodes };
    };
    const or = () => {
      const nodes = [and()];
      while (pos < tokens.length && tokens[pos] !== ')') {
        if (peek() === 'OR') { pos++; }
        nodes.push(and());
      }
      return nodes.length === 1 ? nodes[0] : { or: nodes };
    };

    if (tokens.length === 0) { return { all: true }; }
    const tree = or();
    if (pos !== tokens.length) { throw unsupported('query_string syntax'); }
    return tree;
  }

  /**
   * ES rounds a second resolution date up for lte and gt, lte "10:27:57" means through 10:27:57.999.
   * The ms a date bound means, or the bound when it isn't a date string.
   */
  static dateBound (op, value) {
    if (typeof value !== 'string') { return value; }
    const parsed = Date.parse(value);
    if (Number.isNaN(parsed)) { return value; }
    return (op === 'lte' || op === 'gt') && parsed % 1000 === 0 ? parsed + 999 : parsed;
  }
}

module.exports = EsQuery;
