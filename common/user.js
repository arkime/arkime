/******************************************************************************/
/* userDB.js  -- User Database interface
 *
 * Copyright Yahoo Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
'use strict';
const { Client } = require('@elastic/elasticsearch');
const fs = require('fs');
const ArkimeUtil = require('../common/arkimeUtil');

const systemRolesMapping = {
  superAdmin: ['usersAdmin', 'arkimeAdmin', 'arkimeUser', 'parliamentAdmin', 'parliamentUser', 'wiseAdmin', 'wiseUser', 'cont3xtAdmin', 'cont3xtUser'],
  usersAdmin: [],
  arkimeAdmin: ['arkimeUser'],
  arkimeUser: [],
  parliamentAdmin: ['parliamentUser'],
  parliamentUser: [],
  wiseAdmin: ['wiseUser'],
  wiseUser: [],
  cont3xtAdmin: ['cont3xtUser'],
  cont3xtUser: []
};

const usersMissing = {
  userId: '',
  userName: '',
  expression: '',
  enabled: 0,
  createEnabled: 0,
  webEnabled: 0,
  headerAuthEnabled: 0,
  emailSearch: 0,
  removeEnabled: 0,
  lastUsed: 0
};

const searchColumns = [
  'userId', 'userName', 'expression', 'enabled', 'createEnabled',
  'webEnabled', 'headerAuthEnabled', 'emailSearch', 'removeEnabled', 'packetSearch',
  'hideStats', 'hideFiles', 'hidePcap', 'disablePcapDownload', 'welcomeMsgNum',
  'lastUsed', 'timeLimit', 'roles'
];

let readOnly = false;

/******************************************************************************/
// User class
/******************************************************************************/
class User {
  static lastUsedMinInterval = 60 * 1000;
  static userCacheTimeout = 5 * 1000;
  static usersCache = {};
  static rolesCache = { _timeStamp: 0 };
  static debug = false;

  /**
   * Initialize the User subsystem
   */
  static initialize (options) {
    User.debug = options.debug ?? 0;
    readOnly = options.readOnly ?? false;

    // options.url = 'lmdb://./lmdb-users';
    // options.url = 'redis://localhost:6379/1';
    if (!options.url) {
      User.implementation = new UserESImplementation(options);
    } else if (options.url.startsWith('lmdb')) {
      User.implementation = new UserLMDBImplementation(options);
    } else if (options.url.startsWith('redis')) {
      User.implementation = new UserRedisImplementation(options);
    } else {
      User.implementation = new UserESImplementation(options);
    }
  }

  /**
   * Flush any in memory data
   */
  static flushCache () {
    User.usersCache = {};
    User.rolesCache = { _timeStamp: 0 };
  }

  // Get the ES client for viewer, will remove someday
  static getClient () {
    if (User.implementation.getClient()) {
      return User.implementation.getClient();
    }
    return null;
  }

  /**
   * Return a user checking cache first or go to DB, callback only
   */
  static getUserCache (userId, cb) {
    if (User.usersCache[userId] && User.usersCache[userId]._timeStamp > Date.now() - User.userCacheTimeout) {
      return cb(null, User.usersCache[userId].user);
    }

    User.getUser(userId, (err, user) => {
      if (err || !user) {
        return cb(err, user);
      }

      User.usersCache[userId] = {
        _timeStamp: Date.now(),
        user: user
      };
      cb(null, user);
    });
  };

  /******************************************************************************/
  // Static methods the Implmentation must have
  /******************************************************************************/

  /**
   * Flush anything to disk and refresh any index
   */
  static flush () {
    if (User.implementation.flush) {
      return User.implementation.flush();
    }
  }

  /**
   * Close the DB if needed
   */
  static close () {
    if (User.implementation.close) {
      return User.implementation.close();
    }
  }

  /**
   * search against user index, promise only
   * @param query.from first index
   * @param query.size number of items
   * @param query.filter search userId userName for
   * @param query.sortField the field to sort on, default userId
   * @param query.sortDescending sort descending, default false
   * @returns {total: number matching, users: the users in the from-size section}
   */
  static searchUsers (query) {
    if (query.size > 10000) {
      return { error: 'Max of 10000 users' };
    }
    return User.implementation.searchUsers(query);
  }

  /**
   * Return a user from DB, callback only
   */
  static getUser (userId, cb) {
    User.implementation.getUser(userId, async (err, data) => {
      if (err || !data) { return cb(err, null); }

      const user = Object.assign(new User(), data);
      cleanUser(user);
      user.settings = user.settings ?? {};
      if (readOnly) {
        user.createEnabled = false;
      }
      return cb(null, user);
    });
  }

  /**
   * Number of users we know about
   */
  static numberOfUsers () {
    return User.implementation.numberOfUsers();
  };

  /**
   * Delete user
   */
  static deleteUser (userId) {
    delete User.usersCache[userId];
    return User.implementation.deleteUser(userId);
  };

  /**
   * Set user, callback only
   */
  static setUser (userId, doc, cb) {
    delete doc._allRoles;
    delete User.usersCache[userId];
    User.implementation.setUser(userId, doc, (err, boo) => {
      cb(err, boo);
    });
  };

  /**
   * Return all available roles using cache
   */
  static allRolesCache () {
    if (User.rolesCache._timeStamp > Date.now() - User.userCacheTimeout) {
      return User.rolesCache.roles;
    }

    User.rolesCache._timeStamp = Date.now();
    User.rolesCache.roles = new Set([...Object.keys(systemRolesMapping), ...User.implementation.allRoles()]);
    return User.rolesCache.roles;
  }

  /**
   * Api for listing available roles
   */
  static async apiRoles (req, res, next) {
    const roles = await User.allRolesCache();
    return res.send({ success: true, roles: [...roles].sort() });
  }

  /******************************************************************************/
  // Regression Tests APIs
  /******************************************************************************/

  /**
   * Delete all the users
   */
  static apiDeleteAllUsers (req, res, next) {
    User.implementation.deleteAllUsers();
    User.flushCache();
    return res.send({ success: true });
  }

  /******************************************************************************/
  // Per User Methods
  /******************************************************************************/

  /**
   * Generate set of all the roles this user has
   */
  expandRoles () {
    const roles = new Set(this.roles ?? []);
    for (const r of this.roles ?? []) {
      if (systemRolesMapping[r]) {
        systemRolesMapping[r].forEach(roles.add, roles);
      }
    }
    this._allRoles = roles;
  }

  /**
   * Check if user has role. The check can be against a single role or array of roles.
   */
  hasRole (role) {
    if (this._allRoles === undefined) {
      this.expandRoles();
    }

    if (!Array.isArray(role)) {
      return this._allRoles.has(role);
    }

    for (const r of role) {
      if (this._allRoles.has(r)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Return set of all roles for ourself
   */
  getRoles () {
    if (this._allRoles === undefined) {
      this.expandRoles();
    }

    return this._allRoles;
  }

  // Set last used info for user, should only be used by Auth
  async setLastUsed () {
    if (!readOnly) {
      try {
        const now = Date.now();
        if (!this.lastUsed || (now - this.lastUsed) > User.lastUsedMinInterval) {
          this.lastUsed = now;
          await User.implementation.setLastUsed(this.userId, now);
        }
      } catch (err) {
        if (User.debug) {
          console.log('DEBUG - user lastUsed update error', err);
        }
      }
    }
  }
}

/******************************************************************************/
// Clean User
/******************************************************************************/
function cleanUser (user) {
  user.expression = user.expression || '';
  user.headerAuthEnabled = user.headerAuthEnabled || false;
  user.emailSearch = user.emailSearch || false;
  user.removeEnabled = user.removeEnabled || false;
  user.userName = ArkimeUtil.safeStr(user.userName || '');
  user.packetSearch = user.packetSearch || false;
  user.timeLimit = user.timeLimit || undefined;
  user.lastUsed = user.lastUsed || 0;
}

/******************************************************************************/
// Clean Search User
/******************************************************************************/
function cleanSearchUser (iuser) {
  const ouser = {};
  for (const col of searchColumns) {
    ouser[col] = iuser[col];
  }
  cleanUser(ouser);
  return ouser;
}

/******************************************************************************/
// Sort Users
/******************************************************************************/
function sortUsers (users, sortField, sortDescending) {
  if (users.length <= 1) { return; }

  if (typeof users[0][sortField] === 'string') {
    if (sortDescending) {
      users.sort((b, a) => { return a[sortField].localeCompare(b[sortField]); });
    } else {
      users.sort((a, b) => { return a[sortField].localeCompare(b[sortField]); });
    }
  } else {
    if (sortDescending) {
      users.sort((b, a) => { return a[sortField] - b[sortField]; });
    } else {
      users.sort((a, b) => { return a[sortField] - b[sortField]; });
    }
  }
}
/******************************************************************************/
// Filter Users
/******************************************************************************/
function filterUsers (users, filter) {
  const results = [];
  const re = ArkimeUtil.wildcardToRegexp(`*${filter}*`);
  for (let i = 0; i < users.length; i++) {
    if (users[i].userId.match(re) || users[i].userName.match(re)) {
      results.push(users[i]);
    }
  }
  return results;
}

/******************************************************************************/
// ES Implementation of Users DB
/******************************************************************************/
class UserESImplementation {
  prefix;
  client;

  constructor (options) {
    if (options.prefix === undefined || options.prefix === '') {
      this.prefix = '';
    } else if (options.prefix.endsWith('_')) {
      this.prefix = options.prefix;
    } else {
      this.prefix = options.prefix + '_';
    }

    const esSSLOptions = { rejectUnauthorized: !options.insecure, ca: options.ca };
    if (options.clientKey) {
      esSSLOptions.key = fs.readFileSync(options.clientKey);
      esSSLOptions.cert = fs.readFileSync(options.clientCert);
      if (options.clientKeyPass) {
        esSSLOptions.passphrase = options.clientKeyPass;
      }
    }

    const esOptions = {
      node: options.node,
      maxRetries: 2,
      requestTimeout: (parseInt(options.requestTimeout) + 30) * 1000 || 330000,
      ssl: esSSLOptions
    };

    if (options.apiKey) {
      esOptions.auth = {
        apiKey: options.apiKey
      };
    } else if (options.basicAuth) {
      let basicAuth = options.basicAuth;
      if (!basicAuth.includes(':')) {
        basicAuth = Buffer.from(basicAuth, 'base64').toString();
      }
      basicAuth = basicAuth.split(':');
      esOptions.auth = {
        username: basicAuth[0],
        password: basicAuth[1]
      };
    }

    this.client = new Client(esOptions);
  }

  getClient () {
    return this.client;
  }

  async flush () {
    this.client.indices.flush({ index: this.prefix + 'users' });
    this.client.indices.refresh({ index: this.prefix + 'users' });
  }

  // search against user index, promise only
  async searchUsers (query) {
    const esQuery = {
      _source: searchColumns,
      from: query.from ?? 0,
      size: query.size ?? 10000,
      query: {
        bool: {
          must_not: [
            { term: { userId: '_moloch_shared' } }, // exclude shared ues
            { prefix: { userId: 'role:' } } // exclude roles
          ]
        }
      }
    };

    if (query.filter) {
      esQuery.query.bool.should = [
        { wildcard: { userName: '*' + query.filter + '*' } },
        { wildcard: { userId: '*' + query.filter + '*' } }
      ];
    }

    if (query.sortField) {
      esQuery.sort = {};
      esQuery.sort[query.sortField] = { order: query.sortDescending === true ? 'desc' : 'asc' };
      esQuery.sort[query.sortField].missing = usersMissing[query.sortField];
    }

    const { body: users } = await this.client.search({
      index: this.prefix + 'users',
      body: esQuery,
      rest_total_hits_as_int: true
    });

    if (users.error) {
      return { error: users.error };
    }

    const hits = [];
    for (const user of users.hits.hits) {
      const fields = user._source || user.fields;
      fields.id = user._id;
      cleanUser(fields);
      hits.push(Object.assign(new User(), fields));
    }
    return { users: hits, total: users.hits.total };
  }

  // Return a user from DB, callback only
  getUser (userId, cb) {
    this.client.get({ index: this.prefix + 'users', id: userId }, (err, result) => {
      if (result?.body?.found === false) {
        return cb(null, null);
      } else if (err) {
        return cb(err, null);
      }
      return cb(null, result.body._source);
    });
  }

  async numberOfUsers () {
    const { body: count } = await this.client.count({
      index: this.prefix + 'users',
      ignoreUnavailable: true,
      body: {
        query: { // exclude the shared user from results
          bool: { must_not: { term: { userId: '_moloch_shared' } } }
        }
      }
    });
    return count.count;
  };

  // Delete user, promise only
  async deleteUser (userId) {
    await this.client.delete({
      index: this.prefix + 'users',
      id: userId,
      refresh: true
    });
    delete User.usersCache[userId]; // Delete again after db says its done refreshing
  };

  // Set user, callback only
  setUser (userId, doc, cb) {
    const createOnly = !!doc._createOnly;
    delete doc._createOnly;
    this.client.index({
      index: this.prefix + 'users',
      body: doc,
      id: userId,
      refresh: true,
      timeout: '10m',
      op_type: createOnly ? 'create' : 'index'
    }, (err) => {
      delete User.usersCache[userId]; // Delete again after db says its done refreshing
      cb(err);
    });
  };

  setLastUsed (userId, now) {
    const params = {
      index: this.prefix + 'users',
      body: { doc: { lastUsed: now } },
      id: userId,
      retry_on_conflict: 3
    };

    return this.client.update(params);
  };

  async allRoles () {
    const response = await this.client.search({
      index: this.prefix + 'users',
      body: { query: { prefix: { userId: 'role:' } } },
      rest_total_hits_as_int: true
    });

    return response.body.hits.hits.map(h => h._source.userId);
  }

  async deleteAllUsers () {
    await this.client.delete_by_query({
      index: this.prefix + 'users',
      body: { query: { match_all: { } } }
    });
  }
}

/******************************************************************************/
// LMDB Implementation of Users DB
/******************************************************************************/
class UserLMDBImplementation {
  store;

  constructor (options) {
    this.store = ArkimeUtil.createLMDBStore(options.url, 'User');
  }

  // search against user index, promise only
  async searchUsers (query) {
    let hits = [];
    this.store.getRange({})
      .filter(({ key, value }) => key !== '_moloch_shared' && !key.startsWith('role:'))
      .forEach(({ key, value }) => {
        value = cleanSearchUser(value);
        value.id = key;
        hits.push(Object.assign(new User(), value));
      });

    if (query.filter) {
      hits = filterUsers(hits, query.filter);
    }
    sortUsers(hits, query.sortField, query.sortDescending);

    return {
      total: hits.length,
      users: hits.slice(query.from, query.from + query.size)
    };
  }

  // Return a user from DB, callback only
  getUser (userId, cb) {
    try {
      return cb(null, this.store.get(userId));
    } catch (err) {
      return cb(err);
    }
  }

  async numberOfUsers () {
    return await new Promise((resolve, reject) => {
      try {
        let count = 0;
        for (const key of this.store.getKeys({})) {
          if (key !== '_moloch_shared' && !key.startsWith('role:')) {
            count++;
          }
        }
        resolve(count);
      } catch (err) {
        reject(err);
      }
    });
  };

  // Delete user, promise only
  async deleteUser (userId) {
    await this.store.remove(userId);
    delete User.usersCache[userId]; // Delete again after db says its done refreshing
  };

  // Set user, callback only
  async setUser (userId, doc, cb) {
    const createOnly = !!doc._createOnly;
    delete doc._createOnly;
    try {
      if (createOnly) {
        const user = this.store.get(userId);
        if (!user) {
          await this.store.put(userId, doc);
          cb(null);
        } else {
          cb({ meta: { body: { error: { type: 'version_conflict_engine_exception' } } } });
        }
      } else {
        await this.store.put(userId, doc);
        cb(null);
      }
    } catch (err) {
      cb(err);
    }
  };

  async setLastUsed (userId, now) {
    const user = this.store.get(userId);
    user.lastUsed = now;
    await this.store.put(userId, user);
  };

  async allRoles () {
    const hits = [];
    this.store.getKeys({})
      .filter(key => { return key.startsWith('role:'); })
      .forEach(key => {
        hits.push(key);
      });

    return hits;
  }

  async deleteAllUsers () {
    this.store.getKeys({})
      .forEach(key => {
        this.store.remove(key);
      });
  }
}

/******************************************************************************/
// Redis Implementation of Users DB
/******************************************************************************/
class UserRedisImplementation {
  client;

  constructor (options) {
    this.client = ArkimeUtil.createRedisClient(options.url, 'User');
  }

  // search against user index, promise only
  async searchUsers (query) {
    const keys = (await this.client.keys('*')).filter(key => key !== '_moloch_shared' && !key.startsWith('role:'));
    let hits = [];
    for (const key of keys) {
      const data = await this.client.get(key);
      if (!data) { continue; }
      let user = JSON.parse(data);
      user = cleanSearchUser(user);
      user.id = key;
      hits.push(Object.assign(new User(), user));
    }

    if (query.filter) {
      hits = filterUsers(hits, query.filter);
    }
    sortUsers(hits, query.sortField, query.sortDescending);

    return {
      total: hits.length,
      users: hits.slice(query.from, query.from + query.size)
    };
  }

  // Return a user from DB, callback only
  getUser (userId, cb) {
    this.client.get(userId, (err, user) => {
      if (err || !user) { return cb(err, user); }
      user = JSON.parse(user);
      return cb(err, user);
    });
  }

  async numberOfUsers () {
    const keys = (await this.client.keys('*')).filter(key => key !== '_moloch_shared' && !key.startsWith('role:'));
    return keys.length;
  };

  // Delete user, promise only
  async deleteUser (userId) {
    await this.client.del(userId);
    delete User.usersCache[userId]; // Delete again after db says its done refreshing
  };

  // Set user, callback only
  async setUser (userId, doc, cb) {
    const createOnly = !!doc._createOnly;
    delete doc._createOnly;
    doc = JSON.stringify(doc);
    try {
      if (createOnly) {
        this.client.setnx(userId, doc, cb);
        // cb({ meta: { body: { error: { type: 'version_conflict_engine_exception' } } } });
      } else {
        this.client.set(userId, doc, cb);
        delete User.usersCache[userId];
      }
    } catch (err) {
      cb(err);
    }
  };

  async setLastUsed (userId, now) {
    let user = await this.client.get(userId);
    if (!user) { return; }
    user = JSON.parse(user);
    user.lastUsed = now;
    user = JSON.stringify(user);
    await this.client.set(userId, user);
  };

  async allRoles () {
    const keys = await this.client.keys('role:*');
    return keys;
  }

  async deleteAllUsers () {
    await this.client.flushdb();
  }

  async close () {
    this.client.disconnect();
  }
}

module.exports = User;
