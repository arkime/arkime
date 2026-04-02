/******************************************************************************/
/* notifier.js  -- common notifier code
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';

const cryptoLib = require('crypto');
const path = require('path');
const util = require('util');
const fs = require('fs');
const User = require('./user');
const ArkimeUtil = require('./arkimeUtil');
const ArkimeConfig = require('./arkimeConfig');

class Notifier {
  static notifierTypes;

  static #implementation;
  static #defaultAlerts = { esRed: false, esDown: false, esDropped: false, outOfDate: false, noPackets: false };

  static initialize (options) {
    const url = options.url ?? User.getDbUrl();

    if (!url || url.startsWith('http')) {
      Notifier.#implementation = new NotifierESImplementation(options);
    } else if (url.startsWith('lmdb')) {
      Notifier.#implementation = new NotifierLMDBImplementation(options);
    } else if (url.startsWith('redis')) {
      Notifier.#implementation = new NotifierRedisImplementation({ ...options, url });
    } else if (url.startsWith('sqlite')) {
      Notifier.#implementation = new NotifierSQLiteImplementation({ ...options, url });
    } else {
      Notifier.#implementation = new NotifierESImplementation(options);
    }

    Notifier.notifierTypes = {};

    const api = {
      register: (str, info) => {
        if (options.issueTypes) { info.alerts = options.issueTypes; }
        Notifier.notifierTypes[str] = info;
      }
    };

    // look for all notifier providers and initialize them
    const files = fs.globSync(path.join(__dirname, 'notifier.*.js'));
    for (const file of files) {
      const notifier = require(file);
      notifier.init(api);
    }
  }

  // --------------------------------------------------------------------------
  // DB INTERACTIONS
  // --------------------------------------------------------------------------

  /**
   * @ignore
   * Get a notifier by id
   * @returns {object} { id, ...notifierDoc }
   */
  static async getNotifier (id) {
    return Notifier.#implementation.getNotifier(id);
  }

  /**
   * @ignore
   * Search notifiers with optional filters
   * @param {object} options
   * @param {string} options.name - Filter by exact name
   * @param {string} options.userId - Filter to notifiers accessible by this userId
   * @param {Array} options.roles - Filter to notifiers accessible by these roles
   * @param {boolean} options.all - If true, return all notifiers (admin mode)
   * @returns {Array} Array of { id, ...notifierDoc }
   */
  static async searchNotifiers (options = {}) {
    const results = await Notifier.#implementation.searchNotifiers(options);
    results.sort((a, b) => (a.created ?? 0) - (b.created ?? 0) || (a.name ?? '').localeCompare(b.name ?? ''));
    return results;
  }

  /**
   * @ignore
   * Create a notifier document
   * @returns {string} The id of the created notifier
   */
  static async createNotifier (doc) {
    return Notifier.#implementation.createNotifier(doc);
  }

  /**
   * @ignore
   * Delete a notifier by id
   */
  static async deleteNotifier (id) {
    return Notifier.#implementation.deleteNotifier(id);
  }

  /**
   * @ignore
   * Update a notifier by id
   */
  static async setNotifier (id, doc) {
    return Notifier.#implementation.setNotifier(id, doc);
  }

  /**
   * @ignore
   * Delete all notifiers (for regression tests)
   */
  static async deleteAllNotifiers () {
    return Notifier.#implementation.deleteAllNotifiers();
  }

  static async apiDeleteAllNotifiers (req, res) {
    await Notifier.deleteAllNotifiers();
    return res.send({ success: true });
  }

  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------

  /**
   * @private
   *
   * Checks that the notifier is valid.
   * Mutates the notifier if on/alerts properties are missing or the name has special chars.
   * @param {object} notifier - The notifier object to be checked
   * @returns {string|undefined} - String message to describe check error or undefined if all is good
   */
  static #validateNotifier (notifier) {
    if (!ArkimeUtil.isString(notifier.name)) {
      return 'Missing a notifier name';
    }

    if (!ArkimeUtil.isString(notifier.type)) {
      return 'Missing notifier type';
    }

    if (!notifier.fields) {
      return 'Missing notifier fields';
    }

    if (!Array.isArray(notifier.fields)) {
      return 'Notifier fields must be an array';
    }

    notifier.name = notifier.name.replace(/[^-a-zA-Z0-9_: ]/g, '');
    if (notifier.name.length === 0) {
      return 'Notifier name empty';
    }

    notifier.on ??= false;
    if (typeof notifier.on !== 'boolean') {
      return 'Notifier on state must be true or false';
    }

    const type = notifier.type.toLowerCase();

    let foundNotifier;
    for (const n in Notifier.notifierTypes) {
      if (Notifier.notifierTypes[n].type === type) {
        foundNotifier = Notifier.notifierTypes[n];
      }
    }

    if (!foundNotifier) {
      return 'Unknown notifier type';
    }

    // check that required notifier fields exist
    for (const field of foundNotifier.fields) {
      if (field.required) {
        const sentField = notifier.fields.find(f => f.name === field.name);
        if (!sentField || !sentField.value) {
          return `Missing a value for ${field.name}`;
        }
      }
    }

    notifier.alerts ??= Notifier.#defaultAlerts;
    if (typeof notifier.alerts !== 'object') {
      return 'Alerts must be an object';
    }
    for (const a in notifier.alerts) {
      if (typeof notifier.alerts[a] !== 'boolean') {
        return 'Alert must be true or false';
      }
    }

    return;
  }

  static async issueAlert (id, alertMessage, continueProcess) {
    if (!id) {
      return continueProcess('No id supplied for notifier');
    }

    try {
      const notifier = await Notifier.getNotifier(id);

      if (!notifier) {
        const msg = `ERROR - Cannot find notifier (${id}), no alert can be issued`;
        if (ArkimeConfig.debug) { console.log(msg); }
        return continueProcess(msg);
      }

      let notifierDefinition;
      for (const n in Notifier.notifierTypes) {
        if (Notifier.notifierTypes[n].type === notifier.type.toLowerCase()) {
          notifierDefinition = Notifier.notifierTypes[n];
        }
      }
      if (!notifierDefinition) {
        const msg = `Cannot find notifier definition (for ${notifier.type}), no alert can be issued`;
        console.log(msg);
        return continueProcess(msg);
      }

      const config = {};
      for (const field of notifierDefinition.fields) {
        for (const configuredField of notifier.fields) {
          if (configuredField.name === field.name && configuredField.value !== undefined) {
            config[field.name] = configuredField.value;
          }
        }

        // If a field is required and nothing was set, then we have an error
        if (field.required && config[field.name] === undefined) {
          const msg = `Cannot find notifier field value: ${field.name}, no alert can be issued`;
          if (ArkimeConfig.debug) { console.log(msg); }
          return continueProcess(msg);
        }
      }

      notifierDefinition.sendAlert(config, alertMessage, null, (response) => {
        let err;
        // there should only be one error here because only one notifier alert is sent at a time
        if (response.errors) {
          for (const e in response.errors) {
            err = response.errors[e];
          }
        }
        return continueProcess(err, notifier.name);
      });
    } catch (err) {
      console.log('ERROR - Cannot find notifier (%s):', ArkimeUtil.sanitizeStr(id), util.inspect(err, false, 50));
      return continueProcess('Cannot find notifier, no alert can be issued');
    }
  }

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  /**
   * A service that can be sent a notification.
   * @typedef Notifier
   * @type {object}
   * @property {string} name - The human readable name of the notifier. Must be unique.
   * @property {string} type - The type of notifier (e.g. email, slack, twilio).
   * @property {array} fields - The list of fields that need to be configured to use the notifier.
   * @property {number} created - The time the notifier was created. Format is seconds since Unix EPOCH.
   * @property {number} updated - The time the notifier was last updated. Format is seconds since Unix EPOCH.
   * @property {string} user - The ID of the user that created the notifier.
   * @property {Array} users - The list of userIds who have access to use this notifier.
   * @property {Array} roles - The list of roles who have access to use this notifier.
   */

  /**
   * GET - /api/notifiertypes
   *
   * Retrieves notifier types (admin only).
   * @name /notifiertypes
   * @returns {object} notifiers - The notifiers that Arkime knows about.
   */
  static apiGetNotifierTypes (req, res) {
    return res.send(Notifier.notifierTypes);
  }

  /**
   * GET - /api/notifiers
   *
   * Retrieves notifiers that have been configured.
   * @name /notifiers
   * @returns {Notifier[]} notifiers - The notifiers that have been created.
   */
  static async apiGetNotifiers (req, res) {
    try {
      const isAdmin = req.user.hasRole('arkimeAdmin') || req.user.hasRole('parliamentAdmin');

      let notifiers;
      if (isAdmin) {
        notifiers = await Notifier.searchNotifiers({ all: true });
      } else {
        const allRoles = await req.user.getRoles();
        const roles = [...allRoles.keys()];
        notifiers = await Notifier.searchNotifiers({ userId: req.user.userId, roles });
      }

      const results = notifiers.map((notifier) => {
        // client expects a string for users
        const users = notifier.users;
        notifier.users = '';
        if (users) {
          notifier.users = (Array.isArray(users) ? users : [users]).join(',') || '';
        }
        if (!isAdmin) {
          return { name: notifier.name, type: notifier.type, id: notifier.id };
        }
        return notifier;
      });

      return res.send(results);
    } catch (err) {
      return res.serverError(500, 'Error retrieving notifiers');
    }
  }

  /**
   * POST - /api/notifier
   *
   * Creates a new notifier (admin only).
   * @name /notifier
   * @param {string} name - The name of the new notifier.
   * @param {string} type - The type of notifier.
   * @param {array} fields - The fields to configure the notifier.
   * @returns {boolean} success - Whether the create notifier operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {Notifier} notifier - If successful, the notifier with name sanitized and created/user fields added.
   */
  static async apiCreateNotifier (req, res) {
    const errorMsg = Notifier.#validateNotifier(req.body);
    if (errorMsg) {
      return res.serverError(403, errorMsg);
    }

    // add user and created date
    req.body.user = req.user.userId;
    req.body.created = Math.floor(Date.now() / 1000);

    // comma/newline separated value -> array of values
    let users = ArkimeUtil.commaOrNewlineStringToArray(req.body.users || '');
    users = await User.validateUserIds(users);
    req.body.users = users.validUsers;

    try {
      const id = await Notifier.createNotifier(req.body);

      req.body.id = id;
      req.body.users = req.body.users.join(',');
      return res.json({
        success: true,
        notifier: req.body,
        text: 'Created notifier!',
        invalidUsers: users.invalidUsers
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/notifier (createNotifier)`, util.inspect(err, false, 50));
      return res.serverError(500, 'Error creating notifier');
    }
  }

  /**
   * PUT - /api/notifier/:id
   *
   * Updates an existing notifier (admin only).
   * @name /notifier/:id
   * @param {string} id - The id of the notifier to update.
   * @param {string} type - The new type of notifier.
   * @param {array} fields - The new field values to configure the notifier.
   * @returns {boolean} success - Whether the update notifier operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {Notifier} notifier - If successful, the updated notifier with name sanitized and updated field added/updated.
   */

  static async apiUpdateNotifier (req, res) {
    const errorMsg = Notifier.#validateNotifier(req.body);
    if (errorMsg) {
      return res.serverError(403, errorMsg);
    }

    try {
      const notifier = await Notifier.getNotifier(req.params.id);

      if (!notifier) {
        return res.serverError(404, 'Notifier not found');
      }

      // only on, name, fields, roles, users, alerts can change
      notifier.on = !!req.body.on;
      notifier.name = req.body.name;
      notifier.roles = req.body.roles;
      notifier.fields = req.body.fields;
      notifier.alerts = req.body.alerts ?? Notifier.#defaultAlerts;
      notifier.updated = Math.floor(Date.now() / 1000); // update/add updated time

      // comma/newline separated value -> array of values
      let users = ArkimeUtil.commaOrNewlineStringToArray(req.body.users || '');
      users = await User.validateUserIds(users);
      notifier.users = users.validUsers;

      await Notifier.setNotifier(req.params.id, notifier);
      notifier.id = req.params.id;
      notifier.users = notifier.users.join(',');
      res.json({
        notifier,
        success: true,
        invalidUsers: users.invalidUsers,
        text: 'Updated notifier successfully'
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/notifier/%s (updateNotifier)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Error updating notifier');
    }
  }

  /**
   * DELETE - /api/notifier/:id
   *
   * Deletes an existing notifier (admin only).
   * @name /notifier/:id
   * @returns {boolean} success - Whether the delete notifier operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async apiDeleteNotifier (req, res) {
    try {
      const notifier = await Notifier.getNotifier(req.params.id);

      if (!notifier) {
        return res.serverError(404, 'Notifier not found');
      }

      await Notifier.deleteNotifier(req.params.id);
      res.json({
        success: true,
        text: 'Deleted notifier successfully'
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/notifier/%s (deleteNotifier)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Error deleting notifier');
    }
  }

  /**
   * POST - /api/notifier/:id/test
   *
   * Tests an existing notifier (admin only).
   * @name /notifier/:id/test
   * @returns {boolean} success - Whether the test notifier operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static apiTestNotifier (req, res) {
    function continueProcess (err, notifierName) {
      if (err) {
        return res.serverError(500, `Error testing alert: ${err}`);
      }

      return res.json({
        success: true,
        text: `Successfully issued alert using the ${notifierName} notifier.`
      });
    }

    Notifier.issueAlert(req.params.id, `Test alert from ${req.user.userId}`, continueProcess);
  }
}

/******************************************************************************/
// ES Implementation
/******************************************************************************/
class NotifierESImplementation {
  client;
  notifiersIndex;

  constructor (options) {
    this.client = User.getClient();
    const prefix = ArkimeUtil.formatPrefix(options.prefix);
    this.notifiersIndex = `${prefix}notifiers`;
  }

  async getNotifier (id) {
    try {
      const { body: { _source: doc } } = await this.client.get({ index: this.notifiersIndex, id });
      if (!doc) { return null; }
      doc.id = id;
      return doc;
    } catch (err) {
      if (err.meta?.statusCode === 404) { return null; }
      throw err;
    }
  }

  async searchNotifiers (options = {}) {
    const query = {};

    if (options.name) {
      query.query = { bool: { must: { term: { name: options.name } } } };
    } else if (options.userId || options.roles) {
      const should = [];
      if (options.roles) { should.push({ terms: { roles: options.roles } }); }
      if (options.userId) { should.push({ term: { users: options.userId } }); }
      query.query = { bool: { filter: [{ bool: { should } }] } };
    }

    const { body: { hits: { hits } } } = await this.client.search({
      index: this.notifiersIndex, body: query, rest_total_hits_as_int: true, size: 1000
    });

    return hits.map(h => { const doc = h._source; doc.id = h._id; return doc; });
  }

  async createNotifier (doc) {
    const { body: { _id: id } } = await this.client.index({
      index: this.notifiersIndex, body: doc, refresh: 'wait_for', timeout: '10m'
    });
    return id;
  }

  async deleteNotifier (id) {
    await this.client.delete({ index: this.notifiersIndex, id, refresh: true });
  }

  async setNotifier (id, doc) {
    const docToSave = { ...doc };
    delete docToSave.id;
    await this.client.index({
      index: this.notifiersIndex, body: docToSave, id, refresh: true, timeout: '10m'
    });
  }

  async deleteAllNotifiers () {
    await this.client.indices.refresh({ index: this.notifiersIndex });
    await this.client.deleteByQuery({
      index: this.notifiersIndex, body: { query: { match_all: {} } }, conflicts: 'proceed', refresh: true
    });
  }
}

/******************************************************************************/
// LMDB Implementation
/******************************************************************************/
class NotifierLMDBImplementation {
  store;

  constructor (options) {
    const url = options.url ?? User.getDbUrl();
    this.store = ArkimeUtil.createLMDBStore(url, 'Notifier');
  }

  async getNotifier (id) {
    const json = this.store.get(id);
    if (!json) { return null; }
    const doc = JSON.parse(json);
    doc.id = id;
    return doc;
  }

  async searchNotifiers (options = {}) {
    const results = [];
    for (const { key, value } of this.store.getRange({})) {
      const doc = JSON.parse(value);
      doc.id = key;
      if (options.name && doc.name !== options.name) { continue; }
      if (!options.all && (options.userId || options.roles)) {
        const matchUser = options.userId && doc.users && doc.users.includes(options.userId);
        const matchRole = options.roles && doc.roles && doc.roles.some(r => options.roles.includes(r));
        if (!matchUser && !matchRole) { continue; }
      }
      results.push(doc);
    }
    return results;
  }

  async createNotifier (doc) {
    const id = cryptoLib.randomUUID();
    await this.store.put(id, JSON.stringify(doc));
    return id;
  }

  async deleteNotifier (id) {
    await this.store.remove(id);
  }

  async setNotifier (id, doc) {
    const docToSave = { ...doc };
    delete docToSave.id;
    await this.store.put(id, JSON.stringify(docToSave));
  }

  async deleteAllNotifiers () {
    for (const { key } of this.store.getRange({})) {
      this.store.remove(key);
    }
  }
}

/******************************************************************************/
// Redis Implementation
/******************************************************************************/
class NotifierRedisImplementation {
  client;
  prefix = 'notifier:';

  constructor (options) {
    this.client = ArkimeUtil.createRedisClient(options.url, 'Notifier');
  }

  async getNotifier (id) {
    const json = await this.client.get(this.prefix + id);
    if (!json) { return null; }
    const doc = JSON.parse(json);
    doc.id = id;
    return doc;
  }

  async #getAllNotifiers () {
    const keys = await this.client.keys(this.prefix + '*');
    if (keys.length === 0) { return []; }
    const results = [];
    for (const key of keys) {
      const json = await this.client.get(key);
      if (!json) { continue; }
      const doc = JSON.parse(json);
      doc.id = key.slice(this.prefix.length);
      results.push(doc);
    }
    return results;
  }

  async searchNotifiers (options = {}) {
    const all = await this.#getAllNotifiers();
    return all.filter(doc => {
      if (options.name && doc.name !== options.name) { return false; }
      if (!options.all && (options.userId || options.roles)) {
        const matchUser = options.userId && doc.users && doc.users.includes(options.userId);
        const matchRole = options.roles && doc.roles && doc.roles.some(r => options.roles.includes(r));
        if (!matchUser && !matchRole) { return false; }
      }
      return true;
    });
  }

  async createNotifier (doc) {
    const id = cryptoLib.randomUUID();
    await this.client.set(this.prefix + id, JSON.stringify(doc));
    return id;
  }

  async deleteNotifier (id) {
    await this.client.del(this.prefix + id);
  }

  async setNotifier (id, doc) {
    const docToSave = { ...doc };
    delete docToSave.id;
    await this.client.set(this.prefix + id, JSON.stringify(docToSave));
  }

  async deleteAllNotifiers () {
    const keys = await this.client.keys(this.prefix + '*');
    for (const key of keys) {
      await this.client.del(key);
    }
  }
}

/******************************************************************************/
// SQLite Implementation
/******************************************************************************/
class NotifierSQLiteImplementation {
  db;

  constructor (options) {
    this.db = ArkimeUtil.createSQLiteDB(options.url);
    this.db.exec(`CREATE TABLE IF NOT EXISTS notifiers (
      id TEXT PRIMARY KEY,
      json TEXT NOT NULL
    )`);
  }

  async getNotifier (id) {
    const row = this.db.prepare('SELECT json FROM notifiers WHERE id = ?').get(id);
    if (!row) { return null; }
    const doc = JSON.parse(row.json);
    doc.id = id;
    return doc;
  }

  async searchNotifiers (options = {}) {
    const rows = this.db.prepare('SELECT id, json FROM notifiers').all();
    const results = [];
    for (const row of rows) {
      const doc = JSON.parse(row.json);
      doc.id = row.id;
      if (options.name && doc.name !== options.name) { continue; }
      if (!options.all && (options.userId || options.roles)) {
        const matchUser = options.userId && doc.users && doc.users.includes(options.userId);
        const matchRole = options.roles && doc.roles && doc.roles.some(r => options.roles.includes(r));
        if (!matchUser && !matchRole) { continue; }
      }
      results.push(doc);
    }
    return results;
  }

  async createNotifier (doc) {
    const id = cryptoLib.randomUUID();
    this.db.prepare('INSERT INTO notifiers (id, json) VALUES (?, ?)').run(id, JSON.stringify(doc));
    return id;
  }

  async deleteNotifier (id) {
    this.db.prepare('DELETE FROM notifiers WHERE id = ?').run(id);
  }

  async setNotifier (id, doc) {
    const docToSave = { ...doc };
    delete docToSave.id;
    this.db.prepare('UPDATE notifiers SET json = ? WHERE id = ?').run(JSON.stringify(docToSave), id);
  }

  async deleteAllNotifiers () {
    this.db.prepare('DELETE FROM notifiers').run();
  }
}

module.exports = Notifier;
