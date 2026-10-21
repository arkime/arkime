/* arkimeDb.redis.js  -- Redis collections for ArkimeDb
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const ArkimeDb = require('./arkimeDb');

// Each doc is a json string at <prefix><id>. Only GET/MGET/SET/DEL/KEYS are used
// so it works with every redis flavor and the test mini-redis.
class RedisCollection extends ArkimeDb.ScanCollection {
  #client;
  #prefix;
  #version;

  constructor (client, collName, options) {
    super(collName, options);
    this.#client = client;
    this.#prefix = options.redis?.prefix ?? `${collName}:`;
    this.#version = !!options.version;
  }

  // MGET needs every key on one node, so a cluster gets parallel GETs instead
  async #getMany (keys) {
    if (keys.length === 0) { return []; }
    if (this.#client.isCluster) { return Promise.all(keys.map(key => this.#client.get(key))); }
    const values = [];
    for (let i = 0; i < keys.length; i += 1000) {
      values.push(...await this.#client.mget(keys.slice(i, i + 1000)));
    }
    return values;
  }

  async scan () {
    const keys = await this.#client.keys(`${this.#prefix}*`);
    const jsons = await this.#getMany(keys);
    const ids = keys.map(key => key.slice(this.#prefix.length));
    const versions = this.#version ? await this.#getMany(ids.map(id => this.#versionKey(id))) : [];
    const items = [];
    for (let i = 0; i < keys.length; i++) {
      if (!jsons[i]) { continue; }
      const item = { id: ids[i], doc: JSON.parse(jsons[i]) };
      if (this.#version) { item.version = parseInt(versions[i] ?? '1'); }
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

  #versionKey (id) {
    return `__version:${this.#prefix}${id}`;
  }

  async get (id) {
    const json = await this.#client.get(this.#prefix + id);
    return json ? JSON.parse(json) : null;
  }

  async put (id, doc, { createOnly } = {}) {
    id ??= this.newId();
    if (createOnly) {
      if (!await this.#client.setnx(this.#prefix + id, JSON.stringify(doc))) { throw new ArkimeDb.ConflictError(id); }
      if (this.#version) { await this.#client.set(this.#versionKey(id), '1'); }
      return id;
    }
    if (this.#version) {
      const exists = await this.#client.get(this.#prefix + id);
      const version = exists ? parseInt(await this.#client.get(this.#versionKey(id)) ?? '1') + 1 : 1;
      await this.#client.set(this.#versionKey(id), String(version));
    }
    await this.#client.set(this.#prefix + id, JSON.stringify(doc));
    return id;
  }

  async delete (id) {
    const count = await this.#client.del(this.#prefix + id);
    if (this.#version) { await this.#client.del(this.#versionKey(id)); }
    return count > 0;
  }

  async getMeta (key) {
    const json = await this.#client.get(`__meta:${this.#prefix}${key}`);
    return json ? JSON.parse(json) : undefined;
  }

  async setMeta (obj) {
    for (const [key, value] of Object.entries(obj)) {
      await this.#client.set(`__meta:${this.#prefix}${key}`, JSON.stringify(value));
    }
  }
}

class RedisConnection {
  client;
  #closed = false;
  #collections = new Map();

  constructor (url, options) {
    this.client = options.redisClient ?? ArkimeDb.createRedisClient(url, options.section ?? 'db');
  }

  collection (collName, options = {}) {
    let coll = this.#collections.get(collName);
    if (!coll) {
      coll = new RedisCollection(this.client, collName, options);
      this.#collections.set(collName, coll);
    }
    return coll;
  }

  async close () {
    if (this.#closed) { return; }
    this.#closed = true;
    this.client.disconnect();
  }
}

module.exports = RedisConnection;
