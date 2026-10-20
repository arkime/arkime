/* arkimeDb.lmdb.js  -- LMDB collections for ArkimeDb
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const ArkimeDb = require('./arkimeDb');

// Each collection is a named db in the environment. Docs are stored as
// objects, or as json strings when lmdb.json is set (older layouts).
class LMDBCollection extends ArkimeDb.ScanCollection {
  #store;
  #meta;
  #versions;
  #json;

  constructor (root, collName, options) {
    super(collName, options);
    const dbName = options.lmdb?.db ?? collName;
    this.#store = root.openDB(dbName);
    this.#meta = root.openDB(`${dbName}__meta`);
    if (options.version) { this.#versions = root.openDB(`${dbName}__version`); }
    this.#json = !!options.lmdb?.json;
  }

  #decode (value) {
    if (value === undefined) { return null; }
    return this.#json ? JSON.parse(value) : value;
  }

  async scan () {
    const items = [];
    for (const { key, value } of this.#store.getRange({})) {
      const item = { id: key, doc: this.#decode(value) };
      if (this.#versions) { item.version = this.#versions.get(key) ?? 1; }
      items.push(item);
    }
    return items;
  }

  async find (options = {}) {
    const result = await super.find(options);
    if (!options.version) {
      for (const hit of result.data) { delete hit.version; }
    }
    return result;
  }

  async get (id) {
    return this.#decode(this.#store.get(id));
  }

  async put (id, doc) {
    id ??= this.newId();
    if (this.#versions) {
      const exists = this.#store.get(id) !== undefined;
      await this.#versions.put(id, exists ? (this.#versions.get(id) ?? 1) + 1 : 1);
    }
    await this.#store.put(id, this.#json ? JSON.stringify(doc) : doc);
    return id;
  }

  async delete (id) {
    const exists = this.#store.get(id) !== undefined;
    if (exists) {
      await this.#store.remove(id);
      if (this.#versions) { await this.#versions.remove(id); }
    }
    return exists;
  }

  async getMeta (key) {
    return this.#meta.get(key);
  }

  async setMeta (obj) {
    for (const [key, value] of Object.entries(obj)) {
      await this.#meta.put(key, value);
    }
  }
}

class LMDBConnection {
  root;
  #collections = new Map();

  constructor (url, options) {
    this.root = options.root ?? ArkimeDb.createLMDBStore(url);
  }

  collection (collName, options = {}) {
    let coll = this.#collections.get(collName);
    if (!coll) {
      coll = new LMDBCollection(this.root, collName, options);
      this.#collections.set(collName, coll);
    }
    return coll;
  }

  async close () {
    await this.root.close();
  }
}

module.exports = LMDBConnection;
