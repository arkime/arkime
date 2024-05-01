/******************************************************************************/
/* user.js  -- common User interface and DB implementations
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';
const { Client } = require('@elastic/elasticsearch');
const fs = require('fs');
const util = require('util');
const cryptoLib = require('crypto');

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

const adminRoles = ['usersAdmin', 'arkimeAdmin', 'parliamentAdmin', 'wiseAdmin', 'cont3xtAdmin'];
const adminRolesWithSuper = ['superAdmin', 'usersAdmin', 'arkimeAdmin', 'parliamentAdmin', 'wiseAdmin', 'cont3xtAdmin'];

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
  'lastUsed', 'timeLimit', 'roles', 'roleAssigners'
];

let readOnly = false;
let getCurrentUserCB;

const userIdRegexp = /[^@\w.-]/;

/******************************************************************************/
// User class
/******************************************************************************/
class User {
  static lastUsedMinInterval = 60 * 1000;

  static #userCacheTimeout = 5 * 1000;
  static #usersCache = new Map();
  static #rolesCache = { _timeStamp: 0 };
  static #implementation;
  static #demoMode;

  /**
   * Initialize the User subsystem
   * @param {string} options.url The url that represents which DB implementation to use
   * @param {boolean} options.readOnly=false If true don't set the last used time
   * @param {function} options.getCurrentUserCB Optional function that can modify a user object when fetching
   * @param {boolean} options.noUsersCheck=false If true don't check if the users DB is empty
   *
   */
  static initialize (options) {
    if (ArkimeConfig.debug > 1) {
      console.log('User.initialize', options);
    }
    readOnly = options.readOnly ?? false;
    getCurrentUserCB = options.getCurrentUserCB;

    if (!options.url) {
      User.#implementation = new UserESImplementation(options);
    } else if (options.url.startsWith('lmdb')) {
      User.#implementation = new UserLMDBImplementation(options);
    } else if (options.url.startsWith('redis')) {
      User.#implementation = new UserRedisImplementation(options);
    } else {
      User.#implementation = new UserESImplementation(options);
    }

    if (!options.noUsersCheck && !Auth.isAnonymousMode()) {
      setImmediate(async () => {
        const count = await User.numberOfUsers();
        if (count === 0) {
          console.log('WARNING\nWARNING - No users are defined, use `/opt/arkime/bin/arkime_add_user.sh` to add one\nWARNING');
        }
      });
    }

    User.#demoMode = ArkimeConfig.get('demoMode', false);
    if (typeof User.#demoMode !== 'boolean') {
      User.#demoMode = new Set(ArkimeConfig.getArray('demoMode'));
    }
  }

  /**
   * Flush any in memory data
   */
  static flushCache () {
    User.#usersCache.clear();
    User.#rolesCache = { _timeStamp: 0 };
  }

  /**
   * Delete userId from cache
   * @param {string} userId to delete from cache
   */
  static deleteCache (userId) {
    User.#usersCache.delete(userId);
  }

  // Get the ES client for viewer, will remove someday
  // Used for shortcuts and views index
  static getClient () {
    if (User.#implementation.getClient()) {
      return User.#implementation.getClient();
    }
    return null;
  }

  /**
   * Return a user checking cache first. Supports both promise and cb.
   * @param userId the user to fetch
   * @param cb the callback to use in cb mode
   */
  static async getUserCache (userId, cb) {
    // If we have the cache just cb/return it
    const entry = User.#usersCache.get(userId);
    if (entry && entry._timeStamp > Date.now() - User.#userCacheTimeout) {
      if (cb) {
        return cb(null, entry.user);
      } else {
        return entry.user;
      }
    }

    // Promise version
    if (!cb) {
      return new Promise((resolve, reject) => {
        User.getUser(userId, (err, user) => {
          if (err) { return reject(err); }
          if (!user) { return resolve(user); }

          User.#usersCache.set(userId, { _timeStamp: Date.now(), user });
          return resolve(user);
        });
      });
    }

    // CB version
    User.getUser(userId, (err, user) => {
      if (err || !user) {
        return cb(err, user);
      }

      User.#usersCache.set(userId, { _timeStamp: Date.now(), user });
      cb(null, user);
    });
  };

  /******************************************************************************/
  // Static methods the Implementation must have
  /******************************************************************************/

  /**
   * Flush anything to disk and refresh any index
   */
  static flush (cluster) {
    if (User.#implementation.flush) {
      return User.#implementation.flush(cluster);
    }
  }

  /**
   * Close the DB if needed
   */
  static close () {
    if (User.#implementation.close) {
      return User.#implementation.close();
    }
  }

  /**
   * search against user index, promise only
   * @param query.from first index
   * @param query.size number of items
   * @param query.filter search userId userName for
   * @param query.sortField the field to sort on, default userId
   * @param query.sortDescending sort descending, default false
   * @param query.noRoles filters out users with ids starting with 'role:', default false
   * @param query.searchFields array of fields (with options "userName", "userId", & "roles") to be used in filter
   * @returns {number} total - The total number of matching users
   * @returns {Promise<{error: Error} | {users: ArkimeUser[], total: number}>} users - The users in the from->size section
   */
  static searchUsers (query) {
    if (query.size > 10000) {
      return { error: 'Max of 10000 users' };
    }
    return User.#implementation.searchUsers(query);
  }

  /**
   * @private
   * Create a user from thin air!
   */

  static #makeRegressionUser (userId) {
    if (!Auth.regressionTests) {
      process.exit();
    }

    if (userId.startsWith('sac-') || userId.startsWith('role:sac-')) {
      return undefined;
    }

    const user = Object.assign(new User(), {
      userId,
      enabled: true,
      webEnabled: true,
      headerAuthEnabled: false,
      emailSearch: true,
      removeEnabled: true,
      packetSearch: true,
      settings: {},
      welcomeMsgNum: 1
    });

    if (userId === 'superAdmin') {
      user.roles = ['superAdmin'];
    } else if (userId === 'anonymous') {
      user.roles = ['arkimeAdmin', 'cont3xtUser', 'parliamentUser', 'usersAdmin', 'wiseUser'];
    } else {
      user.roles = ['arkimeUser', 'cont3xtUser', 'parliamentUser', 'wiseUser'];
    }
    User.setUser(userId, user);
    return user;
  }

  /**
   * Return a user from DB, callback only
   */
  static getUser (userId, cb) {
    User.#implementation.getUser(userId, async (err, data) => {
      let user;
      if (err || !data) {
        if (Auth.regressionTests) {
          user = User.#makeRegressionUser(userId, cb);
          if (!user) {
            console.log(`Not making regression user ${userId}`);
            return cb(undefined, undefined);
          }
        } else {
          return cb(err, undefined);
        }
      } else {
        user = Object.assign(new User(), data);
      }

      // If passStore is using old form re-encrypt
      /* Remove for now
      if (data.passStore.split('.').length === 1) {
        data.passStore = Auth.ha12store(Auth.store2ha1(data.passStore));
        User.setUser(data.userId, data, (err, info) => {
          console.log('Upgraded passStore for', data.userId);
        });
      }
      */

      cleanUser(user);
      user.settings = user.settings ?? {};
      await user.expandFromRoles();
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
    return User.#implementation.numberOfUsers();
  };

  /**
   * Delete user
   */
  static deleteUser (userId) {
    User.#usersCache.delete(userId);
    return User.#implementation.deleteUser(userId);
  };

  /**
   * Set user, callback only
   */
  static setUser (userId, user, cb) {
    // Save with usersAdmin role if needed
    if (user.createEnabled) {
      if (user.roles === undefined) {
        user.roles = ['usersAdmin'];
      } else if (!user.roles.includes('usersAdmin')) {
        user.roles.push('usersAdmin');
      }
    }

    // Maintain compatibility for now
    if (user.roles !== undefined) {
      user.createEnabled = user.roles.includes('usersAdmin');
    }

    // Make sure views/notifiers are no longer part of user
    // db.pl should have moved already
    delete user.views;
    delete user.notifiers;

    User.#usersCache.delete(userId);
    User.#implementation.setUser(userId, user, (err, boo) => {
      if (cb) {
        cb(err, boo);
      }
    });
  };

  /**
   * Determines whether each user in the list of users is valid or invalid.
   * @param {Array} userIdList - Array of userIds
   * @param {boolean} anonymousMode - Whether the app is running in anonymous mode (no users)
   * @returns {Object} - An object containing two lists, one of valid users and one of invalid users.
   *                     A valid user can be found in the user's db based on userId
   */
  static async validateUserIds (userIdList) {
    // don't even bother searching for users if in anonymous mode
    if (Auth.isAnonymousMode()) {
      return { validUsers: [], invalidUsers: [] };
    }

    try {
      const users = await User.searchUsers({});
      let usersList = [];
      usersList = users.users.map((user) => {
        return user.userId;
      });

      const validUsers = [];
      const invalidUsers = [];
      for (const user of userIdList) {
        usersList.indexOf(user) > -1 ? validUsers.push(user) : invalidUsers.push(user);
      }

      return { validUsers, invalidUsers };
    } catch (err) {
      console.log('ERROR -', err);
      throw new Error('Unable to validate userIds');
    }
  }

  /**
   * Return all available roles using cache
   */
  static async allRolesCache () {
    if (User.#rolesCache._timeStamp > Date.now() - User.#userCacheTimeout) {
      return User.#rolesCache.roles;
    }

    User.#rolesCache._timeStamp = Date.now();
    const userAllRoles = await User.#implementation.allRoles();
    User.#rolesCache.roles = new Set([...Object.keys(systemRolesMapping), ...userAllRoles]);
    return User.#rolesCache.roles;
  }

  /**
   * An Arkime Role
   *
   * Roles are assigned to users to give them access to Arkime content<br>
   * Default roles include:<br>
   * arkimeAdmin - has administrative access to Arkime (can configure and update Arkime)<br>
   * arkimeUser - has access to Arkime<br>
   * cont3xtAdmin - has administrative access to Cont3xt (can configure and update Cont3xt)<br>
   * cont3xtUser - has access to Cont3xt<br>
   * parliamentAdmin - has administrative access to Parliament (can configure and update Parliament)<br>
   * parliamentUser - has access to Parliament (can view and interact with Parliament Issues)<br>
   * superAdmin - has access to all the applications and can configure anything<br>
   * usersAdmin - has access to configure users<br>
   * wiseAdmin - has administrative access to WISE (can configure and update WISE)<br>
   * wiseUser - has access to WISE
   * @typedef ArkimeRole
   * @type {string}
   */

  static async getRoles () {
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

    return roles;
  }

  /**
   * GET - /api/roles
   *
   * List all available Arkime roles
   * @name /roles
   * @returns {boolean} success - True if the request was successful, false otherwise
   * @returns {ArkimeRole[]} roles - The list of available Arkime roles
   */
  static async apiRoles (req, res, next) {
    const roles = await User.getRoles();
    return res.send({ success: true, roles });
  }

  /**
   * Web Api for getting current user
   */
  static async getCurrentUser (req) {
    const userProps = [
      'emailSearch', 'enabled', 'removeEnabled',
      'headerAuthEnabled', 'settings', 'userId', 'userName', 'webEnabled',
      'packetSearch', 'hideStats', 'hideFiles', 'hidePcap',
      'disablePcapDownload', 'welcomeMsgNum', 'lastUsed'
    ];

    const clone = {};

    for (const prop of userProps) {
      if (req.user[prop]) {
        clone[prop] = req.user[prop];
      }
    }

    clone.roles = [...req.user.#allRoles];

    const assignableRoles = await req.user.getAssignableRoles(req.user.userId);
    clone.assignableRoles = [...assignableRoles];
    clone.canAssignRoles = clone.assignableRoles.length > 0;
    clone.timeLimit = req.user.#allTimeLimit;

    if (getCurrentUserCB) {
      getCurrentUserCB(req.user, clone);
    }

    return clone;
  }

  /**
   * The Arkime user object.
   *
   * @typedef ArkimeUser
   * @type {object}
   * @param {string} userId - The ID of the user.
   * @param {string} userName - The name of the user (to be displayed in the UI).
   * @param {boolean} enabled=true - Whether the user is enabled (or disabled). Disabled users cannot access the UI or APIs.
   * @param {boolean} webEnabled=true - Can access the web interface. When off only APIs can be used.
   * @param {boolean} headerAuthEnabled=false - Can login using the web auth header. This setting doesn't disable the password so it should be scrambled.
   * @param {boolean} emailSearch=false - Can perform searches for fields relating to email.
   * @param {boolean} removeEnabled=false - Can delete tags or delete/scrub pcap data and other deletion operations.
   * @param {boolean} packetSearch=true - Can create a packet search job (hunt).
   * @param {boolean} hideStats=false - Hide the Stats page from this user.
   * @param {boolean} hideFiles=false - Hide the Files page from this user.
   * @param {boolean} hidePcap=false - Hide PCAP (and only show metadata/session detail) for this user when they open a Session.
   * @param {boolean} disablePcapDownload=false - Do not allow this user to download PCAP files.
   * @param {string} expression - An Arkime search expression that is silently added to all queries. Useful to limit what data a user can access (e.g. which nodes or IPs).
   * @param {ArkimeSettings} settings - The Arkime app settings.
   * @param {object} columnConfigs - A list of sessions table column configurations that a user has created.
   * @param {object} spiviewFieldConfigs - A list of SPIView page field configurations that a user has created.
   * @param {object} tableStates - A list of table states used to render Arkime tables as the user has configured them.
   * @param {number} welcomeMsgNum=0 - The message number that a user is on. Gets incremented when a user dismisses a message.
   * @param {number} lastUsed - The date that the user last used Arkime. Format is milliseconds since Unix EPOC.
   * @param {number} timeLimit - Limits the time range a user can query for.
   * @param {array} roles - The list of Arkime roles assigned to this user.
   * @param {array} roleAssigners - The list of userIds that can manage who has this (ROLE)
   */

  /**
   * GET - /api/user
   *
   * Fetches the currently logged in user
   * @name /user
   * @returns {ArkimeUser[]} user - The currently logged in user.
   */
  static async apiGetUser (req, res, next) {
    const clone = await User.getCurrentUser(req);
    return res.send(clone);
  };

  static #apiGetUsersCommon (req) {
    if (typeof req.body !== 'object') { return undefined; }
    if (Array.isArray(req.body.start) || Array.isArray(req.body.length)) {
      return undefined;
    }

    const query = {
      from: parseInt(req.body.start) || 0,
      size: parseInt(req.body.length) || 10000
    };

    if (ArkimeUtil.isString(req.body.filter)) {
      query.filter = req.body.filter;
    }
    query.noRoles = false;

    query.sortField = req.body.sortField || 'userId';
    query.sortDescending = req.body.desc === true;
    query.searchFields = ['userId', 'userName', 'roles'];

    return query;
  }

  /**
   * POST - /api/users
   *
   * Retrieves a list of users (admin only).
   * @name /users
   * @returns {boolean} success - True if the request was successful, false otherwise
   * @returns {ArkimeUser[]} data - The list of users configured.
   * @returns {number} recordsTotal - The total number of users.
   * @returns {number} recordsFiltered - The number of users returned in this result.
   */
  static apiGetUsers (req, res, next) {
    const query = User.#apiGetUsersCommon(req);
    if (query === undefined) {
      res.send({
        success: false,
        recordsTotal: 0,
        recordsFiltered: 0,
        data: []
      });
    }

    Promise.all([
      User.searchUsers(query),
      User.numberOfUsers()
    ]).then(([users, total]) => {
      if (users.error) { throw users.error; }
      res.send({
        success: true,
        recordsTotal: total,
        recordsFiltered: users.total,
        data: users.users
      });
    }).catch((err) => {
      console.log(`ERROR - ${req.method} /api/users`, util.inspect(err, false, 50));
      return res.send({
        success: false,
        recordsTotal: 0,
        recordsFiltered: 0,
        data: []
      });
    });
  };

  /**
   * POST - /api/users/csv
   *
   * Retrieves a list of users (admin only).
   * @name /users/csv
   */
  static apiGetUsersCSV (req, res, next) {
    ArkimeUtil.noCache(req, res, 'text/csv');

    const query = User.#apiGetUsersCommon(req);
    if (query === undefined) {
      res.send({
        success: false,
        recordsTotal: 0,
        recordsFiltered: 0,
        data: []
      });
    }

    Promise.all([
      User.searchUsers(query),
      User.numberOfUsers()
    ]).then(([users, total]) => {
      if (users.error) { throw users.error; }
      const columns = 'userId,userName,enabled,webEnabled,headerAuthEnabled,roles,emailSearch,removeEnabled,packetSearch,hideStats,hideFiles,hidePcap,disablePcapDownload,expression,timeLimit'.split(',');
      res.write(columns.join(', '));
      res.write('\r\n');
      users = users.users;
      for (let u = 0; u < users.length; u++) {
        const values = [];
        for (let c = 0; c < columns.length; c++) {
          let value = users[u][columns[c]];
          if (value === undefined) {
            value = '';
          } else if (Array.isArray(value)) {
            value = '"' + value.join(', ') + '"';
          } else if (typeof (value) === 'string' && value.includes(',')) {
            if (value.includes('"')) {
              value = value.replace(/"/g, '""');
            }
            value = '"' + value + '"';
          }
          values.push(value);
        }
        res.write(values.join(','));
        res.write('\r\n');
      }
      res.end();
    }).catch((err) => {
      console.log(`ERROR - ${req.method} /api/users`, util.inspect(err, false, 50));
      return res.send('Error');
    });
  };

  /**
   * The Arkime user-info object (information provided to roleAssigners or non-admin users).
   *
   * @typedef ArkimeUserInfo
   * @type {object}
   * @param {string} userId - The ID of the user.
   * @param {string} userName - The name of the user (to be displayed in the UI).
   * @param {boolean | undefined} hasRole - whether the user has the requested role
   *            (only if a role was provided & the requester is a roleAssigner for it)
   */

  /**
   * POST - /api/users/min
   *
   * Retrieves a list of users (non-admin usable [with role status returned only for roleAssigners]).
   * @name /users/min
   * @param {string} roleId - Optional roleId to match against
   * @returns {boolean} success - True if the request was successful, false otherwise
   * @returns {ArkimeUserInfo[]} data - The list of users configured.
   */
  static apiGetUsersMin (req, res, next) {
    const query = {
      from: 0,
      size: 10000,
      noRoles: true,
      sortField: 'userId',
      sortDescending: false
    };

    if (ArkimeUtil.isString(req.body.filter)) {
      query.filter = req.body.filter;
    }
    query.searchFields = ['userId', 'userName'];

    User.searchUsers(query).then((users) => {
      if (users.error) { throw users.error; }

      // since this is accessible to non-userAdmins (i.e. roleAssigners), minimal user-info is returned
      //   (only ID and whether they have the managed role)
      const userInfo = users.users.map(u => {
        const providedUserInfo = { userId: u.userId, userName: u.userName };
        if (ArkimeUtil.isString(req.body.roleId)) {
          providedUserInfo.hasRole = !!(u.roles?.includes(req.body.roleId));
        }
        return providedUserInfo;
      });

      res.send({
        success: true,
        data: userInfo
      });
    }).catch((err) => {
      console.log(`ERROR - ${req.method} /api/users/min`, util.inspect(err, false, 50));
      return res.send({
        success: false,
        data: []
      });
    });
  };

  /**
   * POST - /api/user
   *
   * Creates a new user (admin only).
   * @name /user
   * @returns {boolean} success - Whether the add user operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async apiCreateUser (req, res) {
    if (!req.body ||
      !ArkimeUtil.isString(req.body.userId) ||
      !ArkimeUtil.isString(req.body.userName)) {
      return res.serverError(403, 'Missing/Empty required fields');
    }

    if (req.body.userName.match(/^\s*$/)) {
      return res.serverError(403, 'Username can not be empty');
    }

    if (systemRolesMapping[req.body.userId]) {
      return res.serverError(403, 'User ID can\'t be a system role id');
    }

    let userIdTest = req.body.userId;
    const isRole = userIdTest.startsWith('role:');
    if (isRole) {
      userIdTest = userIdTest.slice(5);
      req.body.password = cryptoLib.randomBytes(48); // Reset role password to random
    } else if (!ArkimeUtil.isString(req.body.password, 3)) {
      return res.serverError(403, 'Password needs to be at least 3 characters');
    }

    if (userIdTest.match(userIdRegexp)) {
      return res.serverError(403, 'User ID must be word characters');
    }

    if (req.body.userId === '_moloch_shared') {
      return res.serverError(403, 'User ID cannot be the same as the shared user');
    }

    if (req.body.roles !== undefined && !ArkimeUtil.isStringArray(req.body.roles)) {
      return res.serverError(403, 'Roles field must be an array of strings');
    }

    req.body.roles ??= [];

    if (!User.validateRoles(req.body.roles)) {
      return res.serverError(403, 'User roles must be system roles or start with "role:"');
    }

    const rolesSet = await User.roles2ExpandedSet(req.body.roles);
    const iamSuperAdmin = req.user.hasRole('superAdmin');
    if (isRole && (rolesSet.has(req.body.userId) || req.body.roles.includes(req.body.userId))) {
      return res.serverError(403, 'Can\'t have circular role dependencies');
    }

    if (isRole && req.body.roles.includes('superAdmin')) {
      return res.serverError(403, 'User defined roles can\'t have superAdmin');
    }

    if (isRole && ArkimeUtil.arrayIncludes(req.body.roles, adminRoles)) {
      return res.serverError(403, 'User defined roles can\'t have a system Admin role');
    }

    if (req.body.roleAssigners && !ArkimeUtil.isStringArray(req.body.roleAssigners)) {
      return res.serverError(403, 'roleAssigners field must be an array of strings');
    }

    if (req.body.roles.includes('superAdmin') && !iamSuperAdmin) {
      return res.serverError(403, 'Can not create superAdmin unless you are superAdmin');
    }

    if (req.body.expression !== undefined && !ArkimeUtil.isString(req.body.expression, 0)) {
      return res.serverError(403, 'Expression must be a string when present');
    }

    if (!iamSuperAdmin && adminRoles.some(v => rolesSet.has(v))) {
      return res.serverError(403, 'Only superAdmin user can enable Admin roles on a user');
    }

    req.body.roleAssigners ??= [];

    User.getUser(req.body.userId, (err, user) => {
      if (user) {
        console.log('Trying to add duplicate user', util.inspect(err, false, 50), user);
        return res.serverError(403, 'User already exists');
      }

      const nuser = {
        userId: req.body.userId,
        userName: req.body.userName,
        expression: req.body.expression,
        passStore: Auth.pass2store(req.body.userId, req.body.password),
        enabled: req.body.enabled === true,
        webEnabled: req.body.webEnabled === true,
        emailSearch: req.body.emailSearch === true,
        headerAuthEnabled: req.body.headerAuthEnabled === true,
        removeEnabled: req.body.removeEnabled === true,
        packetSearch: req.body.packetSearch === true,
        timeLimit: req.body.timeLimit,
        hideStats: req.body.hideStats === true,
        hideFiles: req.body.hideFiles === true,
        hidePcap: req.body.hidePcap === true,
        disablePcapDownload: req.body.disablePcapDownload === true,
        roles: req.body.roles,
        welcomeMsgNum: 0,
        roleAssigners: req.body.roleAssigners
      };

      if (ArkimeConfig.debug) {
        console.log('Creating new user', nuser);
      }

      User.setUser(req.body.userId, nuser, (err, info) => {
        if (!err) {
          return res.send(JSON.stringify({
            success: true,
            text: `${isRole ? 'Role' : 'User'} created succesfully`
          }));
        } else {
          console.log(`ERROR - ${req.method} /api/user`, util.inspect(err, false, 50), info);
          return res.serverError(403, err);
        }
      });
    });
  };

  /**
   * DELETE - /api/user/:id
   *
   * Deletes a user (admin only).
   * @name /user/:id
   * @returns {boolean} success - Whether the delete user operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async apiDeleteUser (req, res) {
    const userId = ArkimeUtil.sanitizeStr(req.params.id);

    if (!ArkimeUtil.isString(userId)) {
      return res.serverError(403, 'Missing userId');
    }

    if (userId === req.user.userId) {
      return res.serverError(403, 'Can not delete yourself');
    }

    User.getUser(userId, async (err, user) => {
      if (err || !user) {
        console.log(`ERROR - ${req.method} /api/user/%s`, userId, util.inspect(err, false, 50), user);
        return res.serverError(404, 'User not found');
      }
      if (user.hasRole('superAdmin') && !req.user.hasRole('superAdmin')) {
        return res.serverError(403, 'Can not delete superAdmin unless you are superAdmin');
      }
      try {
        await User.deleteUser(userId);
        res.send({ success: true, text: 'User deleted successfully' });
      } catch (err) {
        console.log(`ERROR - ${req.method} /api/user/%s`, userId, util.inspect(err, false, 50));
        res.send({ success: false, text: 'User not deleted' });
      }
    });
  };

  /**
   * POST - /api/user/:id
   *
   * Updates a user (admin only).
   * @name /user/:id
   * @returns {boolean} success - Whether the update user operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async apiUpdateUser (req, res) {
    const userId = ArkimeUtil.sanitizeStr(req.params.id);

    delete req.body.userId; // Make sure nothing uses this

    if (!ArkimeUtil.isString(userId)) {
      return res.serverError(403, 'Missing userId');
    }

    const isRole = userId.startsWith('role:');

    if (userId === '_moloch_shared') {
      return res.serverError(403, "_moloch_shared is a shared user. This user's settings cannot be updated");
    }

    if (systemRolesMapping[userId]) {
      return res.serverError(403, 'User ID can\'t be a system role id');
    }

    if (req.body.roles !== undefined && !ArkimeUtil.isStringArray(req.body.roles)) {
      return res.serverError(403, 'Roles field must be an array of strings');
    }

    req.body.roles ??= [];

    if (!User.validateRoles(req.body.roles)) {
      return res.serverError(403, 'User roles must be system roles or start with "role:"');
    }

    const rolesSet = await User.roles2ExpandedSet(req.body.roles);
    const iamSuperAdmin = req.user.hasRole('superAdmin');
    if (isRole && (rolesSet.has(userId) || req.body.roles.includes(userId))) {
      return res.serverError(403, 'Can\'t have circular role dependencies');
    }

    if (isRole && req.body.roles.includes('superAdmin')) {
      return res.serverError(403, 'User defined roles can\'t have superAdmin');
    }

    if (isRole && ArkimeUtil.arrayIncludes(req.body.roles, adminRoles)) {
      return res.serverError(403, 'User defined roles can\'t have a system Admin role');
    }

    if (req.body.roleAssigners && !ArkimeUtil.isStringArray(req.body.roleAssigners)) {
      return res.serverError(403, 'roleAssigners field must be an array of strings');
    }

    if (req.body.roles.includes('superAdmin') && !iamSuperAdmin) {
      return res.serverError(403, 'Can not modify superAdmin unless you are superAdmin');
    }

    User.getUser(userId, async (err, user) => {
      if (err || !user) {
        console.log(`ERROR - ${req.method} /api/user/%s`, userId, util.inspect(err, false, 50), user);
        return res.serverError(403, 'User not found');
      }

      if (user.hasRole('superAdmin') && !req.body.roles.includes('superAdmin') && !iamSuperAdmin) {
        return res.serverError(403, 'Can not disable superAdmin unless you are superAdmin');
      }

      // userAdmin can disable Admin roles but can not enable new ones
      if (!iamSuperAdmin && adminRoles.some(v => rolesSet.has(v) && !user.hasRole(v))) {
        return res.serverError(403, 'Only superAdmin user can enable Admin roles on a user');
      }

      user.enabled = req.body.enabled === true;

      if (ArkimeUtil.isString(req.body.expression, 0)) {
        if (req.body.expression.match(/^\s*$/)) {
          delete user.expression;
        } else {
          user.expression = req.body.expression;
        }
      }

      if (ArkimeUtil.isString(req.body.userName)) {
        if (req.body.userName.match(/^\s*$/)) {
          return res.serverError(403, 'Username can not be empty');
        } else {
          user.userName = req.body.userName;
        }
      }

      user.webEnabled = req.body.webEnabled === true;
      user.emailSearch = req.body.emailSearch === true;
      user.headerAuthEnabled = req.body.headerAuthEnabled === true;
      user.removeEnabled = req.body.removeEnabled === true;
      user.packetSearch = req.body.packetSearch === true;
      user.hideStats = req.body.hideStats === true;
      user.hideFiles = req.body.hideFiles === true;
      user.hidePcap = req.body.hidePcap === true;
      user.disablePcapDownload = req.body.disablePcapDownload === true;
      user.timeLimit = req.body.timeLimit ? parseInt(req.body.timeLimit) : undefined;
      user.roles = req.body.roles;
      user.roleAssigners = req.body.roleAssigners ?? [];

      User.setUser(userId, user, (err, info) => {
        if (ArkimeConfig.debug) {
          console.log('setUser', user, err, info);
        }

        if (err) {
          console.log(`ERROR - ${req.method} /api/user/%s`, userId, util.inspect(err, false, 50), user, info);
          return res.serverError(500, 'Error updating user:' + err);
        }

        return res.send(JSON.stringify({
          success: true,
          text: `User ${userId} updated successfully`
        }));
      });
    });
  };

  /**
   * POST - /api/user/:id/assignment
   *
   * Updates whether a user has a certain role (admin & roleAssigners only).
   * @name /user/:id/assignment
   * @returns {boolean} success - Whether the update user operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static apiUpdateUserRole (req, res) {
    const userId = ArkimeUtil.sanitizeStr(req.params.id);
    const roleId = req.body.roleId;
    const newRoleState = req.body.newRoleState;

    if (!ArkimeUtil.isString(userId)) {
      return res.serverError(403, 'Missing userId');
    }

    if (!ArkimeUtil.isString(roleId)) {
      return res.serverError(403, 'Missing roleId');
    }

    if (typeof newRoleState !== 'boolean') {
      return res.serverError(403, 'Missing boolean newRoleState');
    }

    if (userId === '_moloch_shared') {
      return res.serverError(403, "_moloch_shared is a shared user. This user's settings cannot be updated");
    }

    User.getUser(userId, (err, user) => {
      if (err || !user) {
        console.log(`ERROR - ${req.method} /api/user/%s/assignment`, userId, util.inspect(err, false, 50), user);
        return res.serverError(403, 'User not found');
      }

      const roles = [...user.roles];
      const hasRole = roles.includes(roleId);
      if (newRoleState) {
        if (hasRole) {
          return res.serverError(403, 'Can not add a role that the user already has');
        }
        roles.push(roleId);
      } else {
        if (!hasRole) {
          return res.serverError(403, 'Can not remove a role that the user does not have');
        }
        roles.splice(roles.indexOf(roleId), 1);
      }

      user.roles = roles;

      User.setUser(userId, user, (err, info) => {
        if (ArkimeConfig.debug) {
          console.log('setUser', user, err, info);
        }

        if (err) {
          console.log(`ERROR - ${req.method} /api/user/%s/assignment`, userId, util.inspect(err, false, 50), user, info);
          return res.serverError(500, 'Error updating user role:' + err);
        }

        return res.send(JSON.stringify({
          success: true,
          text: `User ${userId}'s role ${roleId} updated successfully`
        }));
      });
    });
  };

  /**
   * POST - /api/user/password
   *
   * Update user password.
   * NOTE: currentPassword is not required so that a usersAdmin can update anyone user's password.
   * @name /user/password
   * @returns {boolean} success - Whether the update password operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static apiUpdateUserPassword (req, res) {
    if (!ArkimeUtil.isString(req.body.newPassword, 3)) {
      return res.serverError(403, 'New password needs to be at least 3 characters');
    }

    if (!req.user.hasRole('usersAdmin') && (Auth.store2ha1(req.user.passStore) !==
      Auth.store2ha1(Auth.pass2store(req.token.userId, req.body.currentPassword)) ||
      req.token.userId !== req.user.userId)) {
      return res.serverError(403, 'Password mismatch');
    }

    // Skip this check if we are a superAdmin
    if (!req.user.hasRole('superAdmin')) {
      // Only change the password if we have the same admin roles(s)
      for (const role of adminRolesWithSuper) {
        if (!req.user.hasRole(role) && req.settingUser.hasRole(role)) {
          return res.serverError(403, `Not allowed to change ${role} password`);
        }
      }
    }

    const user = req.settingUser;
    user.passStore = Auth.pass2store(user.userId, req.body.newPassword);

    User.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log(`ERROR - ${req.method} /api/user/password update error`, util.inspect(err, false, 50), info);
        return res.serverError(500, 'Password update failed');
      }

      return res.send(JSON.stringify({
        success: true,
        text: 'Changed password successfully'
      }));
    });
  };

  /******************************************************************************/
  // Regression Tests APIs
  /******************************************************************************/

  /**
   * Delete all the users
   */
  static apiDeleteAllUsers (req, res, next) {
    User.#implementation.deleteAllUsers();
    User.flushCache();
    return res.send({ success: true });
  }

  /**
   * Convert array of roles to set of fully expanded roles
   */
  static async roles2ExpandedSet (roles) {
    const allRoles = new Set();

    // The roles we need to process to see if any subroles
    const rolesQ = [...roles ?? []];

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
      if (!role || !role.enabled) { continue; }
      allRoles.add(r);

      // schedule any sub roles
      if (!role.roles) { continue; }
      role.roles.forEach(r2 => {
        if (allRoles.has(r2)) { return; } // Already processed
        rolesQ.push(r2);
      });
    }

    return allRoles;
  }

  /**
   * Determines whether the roles are valid.
   * Valid roles are system roles or roles that start with 'role:'
   * @returns {boolean} true if the roles are valid, false otherwise.
   */
  static validateRoles (roles) {
    for (const r of roles) {
      if (!systemRolesMapping[r] && !r.startsWith('role:')) {
        return false;
      }
    }

    return true;
  }

  /******************************************************************************/
  // Per User Methods
  /******************************************************************************/
  #allRoles;
  #allExpression;
  #allTimeLimit;
  /**
   * Save user, callback only
   */
  save (cb) {
    User.setUser(this.userId, this, cb);
  }

  /**
   * Create the combined variables from ourselves and enabled roles we use.
   */
  async expandFromRoles () {
    if (this.#allRoles !== undefined) { return; }
    const allRoles = new Set();

    // The roles we need to process to see if any subroles
    const rolesQ = [...this.roles ?? []];

    if (this.expression && this.expression.trim().length > 0) {
      this.#allExpression = '(' + this.expression.trim() + ')';
    }

    this.#allTimeLimit = this.timeLimit;

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
      if (!role || !role.enabled) { continue; }
      allRoles.add(r);

      if (role.expression && role.expression.trim().length > 0) {
        if (!this.#allExpression) {
          this.#allExpression = '(' + role.expression.trim() + ')';
        } else {
          this.#allExpression += ' && (' + role.expression.trim() + ')';
        }
      }

      if (role.timeLimit !== undefined) {
        if (this.#allTimeLimit === undefined) {
          this.#allTimeLimit = role.timeLimit;
        } else {
          this.#allTimeLimit = Math.min(this.timeLimit, role.timeLimit);
        }
      }

      // schedule any sub roles
      if (!role.roles) { continue; }
      role.roles.forEach(r2 => {
        if (allRoles.has(r2)) { return; } // Already processed
        rolesQ.push(r2);
      });
    }

    this.#allRoles = allRoles;
  }

  /**
   * Check if user has ANY of the roles in role2Check. The check can be against a single role or array of roles.
   */
  hasRole (role2Check) {
    if (!Array.isArray(role2Check)) {
      return this.#allRoles.has(role2Check);
    }

    for (const r of role2Check) {
      if (this.#allRoles.has(r)) {
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
      return this.#allRoles.has(role2Check);
    }

    for (const r of role2Check) {
      if (!this.#allRoles.has(r)) {
        return false;
      }
    }
    return true;
  }

  /*
   *
   */
  isDemoMode () {
    if (this.hasRole(Auth.appAdminRole)) { return false; }

    if (typeof User.#demoMode === 'boolean') {
      return User.#demoMode;
    }

    return User.#demoMode.has(this.userId);
  }

  /**
   * Denies access if the requesting user lacks the required role
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
   * Fails request if a roleId is given in the body, but the user is not a roleAssigner for the given role
   */
  static async checkAssignableRole (req, res, next) {
    const role = req.body.roleId;
    if (role != null && !(await req.user.getAssignableRoles(req.user.userId)).includes(role)) {
      console.log(`Permission denied to ${req.user.userId} while requesting resource: ${req._parsedUrl.pathname}, for assignment-access to role ${role}`);
      return res.serverError(403, 'You do not have permission to access this resource');
    }
    next();
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
   * Transfer ownership of a resource by updating the owner property
   * Only if the resource creator has changed
   * AND the user is an admin or the creator
   * AND the new creator is a valid user
   * If none of the above, sends a server error response
   * @param {Object} req The request object
   * @param {Object} res The response object
   * @param {Object} resource The resource from the client
   * @param {Object} dbResource The resource from the database
   * @param {String} creatorProperty The property name of the creator
   * @returns {boolean} true if the owner was set, false otherwise
   */
  static async setOwner (req, res, resource, dbResource, creatorProperty) {
    if (!resource[creatorProperty]) { // keep same owner
      if (dbResource[creatorProperty]) {
        resource[creatorProperty] = dbResource[creatorProperty];
      }
      return true;
    }

    if ( // if the resource has a new creator
      resource[creatorProperty] !== dbResource[creatorProperty] &&
      ArkimeUtil.isString(resource[creatorProperty])) {
      const settingUser = req.settingUser || req.user;

      if ( // and the user is an admin or the creator of the resource
        settingUser.userId !== dbResource[creatorProperty] &&
        !settingUser.hasRole(Auth.appAdminRole)
      ) {
        res.serverError(403, 'Permission denied');
        return false;
      }

      // check if user is valid before updating it
      // comma/newline separated value -> array of values
      let user = ArkimeUtil.commaOrNewlineStringToArray(resource[creatorProperty]);
      user = await User.validateUserIds(user);

      // set the valid user as the new owner (there should only be one valid user)
      if (user.validUsers?.length) {
        resource[creatorProperty] = user.validUsers[0];
        return true;
      }

      res.serverError(404, 'User not found');
      return false;
    }

    return true;
  }

  /**
   * Return set of all roles expanded for ourself
   */
  async getRoles () {
    if (this.#allRoles === undefined) {
      await this.expandFromRoles();
    }

    return this.#allRoles;
  }

  getExpression () {
    return this.#allExpression;
  }

  /**
   * Returns all roles that the currently this user has assignment access to
   */
  async getAssignableRoles (userId) {
    return await User.#implementation.getAssignableRoles(userId);
  }

  // Set last used info for user, should only be used by Auth
  async setLastUsed () {
    if (!readOnly) {
      try {
        const now = Date.now();
        if (!this.lastUsed || (now - this.lastUsed) > User.lastUsedMinInterval) {
          this.lastUsed = now;
          await User.#implementation.setLastUsed(this.userId, now);
        }
      } catch (err) {
        if (ArkimeConfig.debug) {
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

  // Convert createEnable to usersAdmin role
  if (user.createEnabled && !user.roles.includes('usersAdmin')) {
    user.roles.push('usersAdmin');
  }
  delete user.createEnabled;
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
function filterUsers (users, filter, searchFields, noRoles) {
  const validSearchFields = searchFields
    ?.filter(field => field === 'userId' || field === 'userName' || field === 'roles') || [];
  const usingFilter = filter && validSearchFields.length;
  if (!noRoles && !usingFilter) {
    return users; // nothing to filter on
  }
  const re = ArkimeUtil.wildcardToRegexp(`*${filter}*`);

  return users.filter(user => {
    // exclude roles
    if (noRoles && user.userId.startsWith('role:')) { return false; }

    // filter searched fields
    return (!usingFilter || validSearchFields.some(field => user[field].match(re)));
  });
}

/******************************************************************************/
// OpenSearch/Elasticsearch Implementation of Users DB
/******************************************************************************/
class UserESImplementation {
  prefix;
  client;

  constructor (options) {
    this.prefix = ArkimeUtil.formatPrefix(options.prefix);

    const esSSLOptions = { rejectUnauthorized: !options.insecure };
    if (options.caTrustFile) { esSSLOptions.ca = ArkimeUtil.certificateFileToArray(options.caTrustFile); };
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
      basicAuth = ArkimeUtil.splitRemain(basicAuth, ':', 1);
      esOptions.auth = {
        username: basicAuth[0],
        password: basicAuth[1]
      };
    }

    this.client = new Client(esOptions);
    if (!Auth.isAnonymousMode()) {
      process.nextTick(async () => {
        try {
          await this.client.indices.stats({ index: this.prefix + 'users' });
        } catch (err) {
          ArkimeUtil.searchErrorMsg(err, options.node, `Issue with OpenSearch/Elasticsearch '${this.prefix + 'users'}' index`);
          process.exit(1);
        }
      });
    }
  }

  getClient () {
    return this.client;
  }

  async flush (cluster) {
    this.client.indices.flush({ index: this.prefix + 'users', cluster });
    this.client.indices.refresh({ index: this.prefix + 'users', cluster });
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

    if (query.noRoles) {
      esQuery.query.bool.must_not.push({
        wildcard: { userId: 'role:*' } // exclude roles
      });
    }

    if (query.filter && query.searchFields.length) {
      const searchFilters = query.searchFields
        .filter(field => field === 'userId' || field === 'userName' || field === 'roles')
        .map(validField => { return { wildcard: { [validField]: '*' + query.filter + '*' } }; });
      if (searchFilters.length) { esQuery.query.bool.should = searchFilters; }
    }

    if (query.sortField && query.sortField !== '__proto__') {
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
    User.deleteCache(userId); // Delete again after db says its done refreshing
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
      User.deleteCache(userId); // Delete again after db says its done refreshing
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
      body: {
        size: 1000,
        query: { prefix: { userId: 'role:' } }
      },
      rest_total_hits_as_int: true
    });

    return response.body.hits.hits.map(h => h._source.userId);
  };

  async getAssignableRoles (userId) {
    const query = {
      bool: {
        must: [
          { prefix: { userId: 'role:' } },
          { term: { roleAssigners: userId } }
        ]
      }
    };

    const response = await this.client.search({
      index: this.prefix + 'users',
      body: { query },
      rest_total_hits_as_int: true
    });

    return response.body.hits.hits.map(h => h._source.userId);
  };

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

    hits = filterUsers(hits, query.filter, query.searchFields, query.noRoles);
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
    User.deleteCache(userId); // Delete again after db says its done refreshing
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

  async getAssignableRoles (userId) {
    const hits = [];
    this.store.getRange({})
      .filter(({ key, value }) => { return key.startsWith('role:') && value.roleAssigners?.includes(userId); })
      .forEach(({ key }) => {
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

    hits = filterUsers(hits, query.filter, query.searchFields, query.noRoles);
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
    User.deleteCache(userId); // Delete again after db says its done refreshing
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
        User.deleteCache(userId);
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

  async getAssignableRoles (userId) {
    const keys = await this.allRoles();
    return keys.filter(async key => this.client.get(key).roleAssigners?.includes(userId));
  }

  async deleteAllUsers () {
    await this.client.flushdb();
  }

  async close () {
    this.client.disconnect();
  }
}

module.exports = User;

const Auth = require('../common/auth');
const ArkimeUtil = require('../common/arkimeUtil');
const ArkimeConfig = require('../common/arkimeConfig');
