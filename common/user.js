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
const util = require('util');
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
  webEnabled: 0,
  headerAuthEnabled: 0,
  emailSearch: 0,
  removeEnabled: 0,
  lastUsed: 0
};

const searchColumns = [
  'userId', 'userName', 'expression', 'enabled',
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
    if (options.debug > 1) {
      console.log('User.initialize', options);
    }
    User.debug = options.debug ?? 0;
    readOnly = options.readOnly ?? false;

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
   * Return a user checking cache first
   */
  static async getUserCache (userId, cb) {
    // If we have the cache just cb/return it
    if (User.usersCache[userId] && User.usersCache[userId]._timeStamp > Date.now() - User.userCacheTimeout) {
      if (cb) {
        return cb(null, User.usersCache[userId].user);
      } else {
        return User.usersCache[userId].user;
      }
    }

    // Promise version
    if (!cb) {
      return new Promise((resolve, reject) => {
        User.getUser(userId, (err, user) => {
          if (err) { return reject(err); }
          if (!user) { return resolve(user); }

          User.usersCache[userId] = { _timeStamp: Date.now(), user: user };
          return resolve(user);
        });
      });
    }

    // CB version
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
      user.expandRoles();
      if (readOnly) {
        user.roles = user.roles.filter(role => role === 'usersAdmin');
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
  static setUser (userId, user, cb) {
    delete user._allRoles;
    if (user.createEnabled && !user.roles.includes('usersAdmin')) {
      user.roles.push('usersAdmin');
    }
    delete user.createEnabled;
    delete User.usersCache[userId];
    User.implementation.setUser(userId, user, (err, boo) => {
      cb(err, boo);
    });
  };

  /**
   * Return all available roles using cache
   */
  static async allRolesCache () {
    if (User.rolesCache._timeStamp > Date.now() - User.userCacheTimeout) {
      return User.rolesCache.roles;
    }

    User.rolesCache._timeStamp = Date.now();
    const userAllRoles = await User.implementation.allRoles();
    User.rolesCache.roles = new Set([...Object.keys(systemRolesMapping), ...userAllRoles]);
    return User.rolesCache.roles;
  }

  /**
   * Web Api for listing available roles
   */
  static async apiRoles (req, res, next) {
    let roles = await User.allRolesCache();

    const userDefinedRoles = [];
    roles = [...roles].sort();

    // put user defined roles at the top
    roles = roles.filter((role) => {
      const startsWithRole = role.startsWith('role:');
      if (startsWithRole) { userDefinedRoles.push(role); }
      return !startsWithRole;
    });

    roles = userDefinedRoles.concat(roles);

    return res.send({ success: true, roles });
  }

  /**
   * Web Api for getting current user
   */
  static apiGetUser (req, res, next) {
    const userProps = [
      'emailSearch', 'enabled', 'removeEnabled',
      'headerAuthEnabled', 'settings', 'userId', 'userName', 'webEnabled',
      'packetSearch', 'hideStats', 'hideFiles', 'hidePcap',
      'disablePcapDownload', 'welcomeMsgNum', 'lastUsed', 'timeLimit',
      'roles'
    ];

    const clone = {};

    for (const prop of userProps) {
      if (req.user[prop]) {
        clone[prop] = req.user[prop];
      }
    }

    /* ALW - FIX LATER FOR internals
    clone.canUpload = internals.allowUploads;

    // If esAdminUser is set use that, other wise use createEnable privilege
    if (internals.esAdminUsersSet) {
      clone.esAdminUser = internals.esAdminUsers.includes(req.user.userId);
    } else {
      clone.esAdminUser = req.user.createEnabled && Config.get('multiES', false) === false;
    }

    // If no settings, use defaults
    if (clone.settings === undefined) { clone.settings = internals.settingDefaults; }

    // Use settingsDefaults for any settings that are missing
    for (const item in internals.settingDefaults) {
      if (clone.settings[item] === undefined) {
        clone.settings[item] = internals.settingDefaults[item];
      }
    }
    */

    return res.send(clone);
  };

  /**
   * POST - /api/users
   *
   * Retrieves a list of users (admin only).
   * @name /users
   * @returns {ArkimeUser[]} data - The list of users configured.
   * @returns {number} recordsTotal - The total number of users.
   * @returns {number} recordsFiltered - The number of users returned in this result.
   */
  static apiGetUsers (req, res, next) {
    const query = {
      from: +req.body.start || 0,
      size: +req.body.length || 10000
    };

    if (req.body.filter) {
      query.filter = req.body.filter;
    }

    query.sortField = req.body.sortField || 'userId';
    query.sortDescending = req.body.desc === true;

    Promise.all([
      User.searchUsers(query),
      User.numberOfUsers()
    ]).then(([users, total]) => {
      if (users.error) { throw users.error; }
      res.send({
        recordsTotal: total,
        recordsFiltered: users.total,
        data: users.users
      });
    }).catch((err) => {
      console.log(`ERROR - ${req.method} /api/users`, util.inspect(err, false, 50));
      return res.send({
        recordsTotal: 0, recordsFiltered: 0, data: []
      });
    });
  };

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
   * Save user, callback only
   */
  save (cb) {
    User.setUser(this.userId, this, cb);
  }

  /**
   * Generate set of all the roles this user has and store in _allRoles.
   */
  async expandRoles () {
    const allRoles = new Set();

    // The roles we need to process to see if any subroles
    const rolesQ = [...this.roles ?? []];

    while (rolesQ.length) {
      const r = rolesQ.pop();

      // Deal with system roles first, they are easy
      if (systemRolesMapping[r]) {
        allRoles.add(r);
        systemRolesMapping[r].forEach(allRoles.add, allRoles);
        continue;
      }

      // Already processed
      if (allRoles.has(r)) { continue; }

      // See if role actually exists
      const role = await User.getUserCache(r);
      if (!role) { continue; }
      allRoles.add(r);

      // schedule any sub roles
      if (!role.roles) { continue; }
      role.roles.forEach(r2 => {
        if (allRoles.has(r2)) { return; } // Already processed
        rolesQ.push(r2);
      });
    }

    this._allRoles = allRoles;
  }

  /**
   * Check if user has ANY of the roles in role2Check. The check can be against a single role or array of roles.
   */
  hasRole (role2Check) {
    if (!Array.isArray(role2Check)) {
      return this._allRoles.has(role2Check);
    }

    for (const r of role2Check) {
      if (this._allRoles.has(r)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Check if user has ALL of the roles in role2Check. The check can be against a single role or array of roles.
   */
  hasAllRole (role2Check) {
    if (!Array.isArray(role2Check)) {
      return this._allRoles.has(role2Check);
    }

    for (const r of role2Check) {
      if (!this._allRoles.has(r)) {
        return false;
      }
    }
    return true;
  }

  /**
   *
   */
  static checkRole (role) {
    return async (req, res, next) => {
      if (!req.user.hasAllRole(role)) {
        console.log(`Permission denied to ${req.user.userId} while requesting resource: ${req._parsedUrl.pathname}, using role ${role}`);
        return res.serverError(403, 'You do not have permission to access this resource');
      }
      next();
    };
  }

  /**
   *
   */
  static checkPermissions (permissions) {
    const inversePermissions = {
      hidePcap: true,
      hideFiles: true,
      hideStats: true,
      disablePcapDownload: true
    };

    return (req, res, next) => {
      for (const permission of permissions) {
        if ((!req.user[permission] && !inversePermissions[permission]) ||
          (req.user[permission] && inversePermissions[permission])) {
          console.log(`Permission denied to ${req.user.userId} while requesting resource: ${req._parsedUrl.pathname}, using permission ${permission}`);
          return res.serverError(403, 'You do not have permission to access this resource');
        }
      }
      next();
    };
  }

  /**
   * Return set of all roles for ourself
   */
  async getRoles () {
    if (this._allRoles === undefined) {
      await this.expandRoles();
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

  // By default give to all user stuff if never had roles
  if (user.roles === undefined) {
    user.roles = ['arkimeUser', 'cont3xtUser', 'parliamentUser', 'wiseUser'];
  }

  // Replace createEnabled
  if (user.createEnabled && !user.roles.includes('usersAdmin')) {
    // ALW should this write back to db?
    user.roles.push('usersAdmin');
    delete user.createEnabled;
  }
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
            { term: { userId: '_moloch_shared' } } // exclude shared user
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
      .filter(({ key, value }) => key !== '_moloch_shared')
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
    const keys = (await this.client.keys('*')).filter(key => key !== '_moloch_shared');
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
