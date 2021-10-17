/******************************************************************************/
/* userDB.js  -- User Database interface
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
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

class User {
  static debug;
  static readOnly;
  static lastUsedMinInterval = 60 * 1000;
  static userCacheTimeout = 5 * 1000;
  static prefix;
  static client;
  static usersCache = {};

  static initialize (options) {
    User.debug = options.debug ?? 0;
    User.readOnly = options.readOnly ?? false;

    if (options.prefix === undefined || options.prefix === '') {
      User.prefix = '';
    } else if (options.prefix.endsWith('_')) {
      User.prefix = options.prefix;
    } else {
      User.prefix = options.prefix + '_';
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

    User.client = new Client(esOptions);
  }

  static getClient () {
    return User.client;
  }

  static flushCache () {
    User.usersCache = {};
  }

  static async flush () {
    User.client.indices.flush({ index: User.prefix + 'users' });
  }

  static async refresh () {
    User.client.indices.refresh({ index: User.prefix + 'users' });
  }

  // search against user index, promise only
  static async searchUsers (query) {
    const { body: users } = await User.client.search({
      index: User.prefix + 'users',
      body: query,
      rest_total_hits_as_int: true
    });

    if (users.error) {
      return users;
    }

    const hits = [];
    for (const user of users.hits.hits) {
      const fields = user._source || user.fields;
      fields.id = user._id;
      fields.expression = fields.expression || '';
      fields.headerAuthEnabled = fields.headerAuthEnabled || false;
      fields.emailSearch = fields.emailSearch || false;
      fields.removeEnabled = fields.removeEnabled || false;
      fields.userName = ArkimeUtil.safeStr(fields.userName || '');
      fields.packetSearch = fields.packetSearch || false;
      fields.timeLimit = fields.timeLimit || undefined;
      hits.push(Object.assign(new User(), fields));
    }
    users.hits.hits = hits;
    return users;
  }

  // Return a user from DB, callback only
  static getUser (userId, cb) {
    User.client.get({ index: User.prefix + 'users', id: userId }, (err, result) => {
      if (result?.body?.found === false) {
        return cb(null, null);
      } else if (err) {
        return cb(err, null);
      }

      const user = Object.assign(new User(), result.body._source);
      user.settings = user.settings ?? {};
      user.emailSearch = user.emailSearch ?? false;
      user.removeEnabled = user.removeEnabled ?? false;
      if (User.readOnly) {
        user.createEnabled = false;
      } else {
        try {
          const now = Date.now();
          if (!user.lastUsed || (now - user.lastUsed) > User.lastUsedMinInterval) {
            user.setLastUsed(now);
          }
        } catch (err) {
          if (User.debug) {
            console.log('DEBUG - user lastUsed update error', err);
          }
        }
      }

      return cb(null, user);
    });
  }

  // Return a user from cache, callback only
  static getUserCache (userId, cb) {
    if (User.usersCache[userId] && User.usersCache[userId]._timeStamp > Date.now() - User.userCacheTimeout) {
      return cb(null, User.usersCache[userId].user);
    }

    User.getUser(userId, (err, user) => {
      if (err) {
        return cb(err, user);
      }

      User.usersCache[userId] = {
        _timeStamp: Date.now(),
        user: user
      };
      cb(null, user);
    });
  };

  static async numberOfUsers () {
    const { body: count } = await User.client.count({
      index: User.prefix + 'users',
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
  static async deleteUser (userId) {
    delete User.usersCache[userId];
    await User.client.delete({
      index: User.prefix + 'users',
      id: userId,
      refresh: true
    });
    delete User.usersCache[userId]; // Delete again after db says its done refreshing
  };

  // Set user, callback only
  static setUser (userId, doc, cb) {
    delete User.usersCache[userId];
    const createOnly = !!doc._createOnly;
    delete doc._createOnly;
    User.client.index({
      index: User.prefix + 'users',
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

  // set the lastUsed on ourself
  setLastUsed (now) {
    this.lastUsed = now;
    const params = {
      index: User.prefix + 'users',
      body: { doc: { lastUsed: now } },
      id: this.userId,
      retry_on_conflict: 3
    };

    return User.client.update(params);
  };
}

module.exports = User;
