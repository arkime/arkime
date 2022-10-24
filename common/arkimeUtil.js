/* arkimeUtil.js  -- Shared util functions
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

const Redis = require('ioredis');
const memjs = require('memjs');
const Auth = require('./auth');

class ArkimeUtil {
  static debug = 0;
  // ----------------------------------------------------------------------------
  static safeStr (str) {
    return str.replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;')
      .replace(/\//g, '&#47;');
  };

  // ----------------------------------------------------------------------------
  static sanitizeStr (str) {
    if (!str) { return str; }
    // eslint-disable-next-line no-control-regex
    return str.replace(/\u001b/g, '*ESC*');
  }

  // ----------------------------------------------------------------------------
  static noCacheJson (req, res, next) {
    res.header('Cache-Control', 'no-cache, private, no-store, must-revalidate, max-stale=0, post-check=0, pre-check=0');
    res.header('Content-Type', 'application/json');
    res.header('X-Content-Type-Options', 'nosniff');
    return next();
  }

  // ----------------------------------------------------------------------------
  /**
   * Create a redis client from the provided url
   * @params {string} url - The redis url to connect to.
   * @params {string} section - The section this redis client is being created for
   */
  static createRedisClient (url, section) {
    url = ArkimeUtil.sanitizeStr(url);
    // redis://[:pass]@host:port/db
    if (url.startsWith('redis://') || url.startsWith('rediss://')) {
      const match = url.match(/(rediss?):\/\/(:[^@]+@)?([^:/]+)(:[0-9]+)?\/([0-9]+)/);
      if (!match) {
        console.log(`${section} - ERROR - can't parse redis url '%s' should be of form //[:pass@]redishost[:redisport]/redisDbNum`, url);
        process.exit(1);
      }

      if (ArkimeUtil.debug > 1) {
        console.log('REDIS:', url);
      }
      return new Redis(url);
    }

    // redis-sentinel://sentinelPassword:redisPassword@host:port[,hostN;portN]/name/db
    if (url.startsWith('redis-sentinel://')) {
      const match = url.match(/(redis-sentinel):\/\/(([^:]+)?:([^@]+)?@)?([^/]+)\/([^/]+)\/([0-9]+)(\/.+)?/);
      if (!match) {
        console.log(`${section} - ERROR - can't parse redis-sentinel url '%s' should be of form //[sentinelPassword:redisPassword@]sentinelHost[:sentinelPort][,sentinelPortN[:sentinelPortN]]/redisName/redisDbNum`, url);
        process.exit(1);
      }

      const options = { sentinels: [], name: match[6], db: parseInt(match[7]) };
      match[5].split(',').forEach((hp) => {
        const hostport = hp.split(':');
        options.sentinels.push({ host: hostport[0], port: hostport[1] || 26379 });
      });

      if (match[3] && match[3] !== '') {
        options.sentinelPassword = match[3];
      }
      if (match[4] && match[4] !== '') {
        options.password = match[4];
      }

      if (ArkimeUtil.debug > 1) {
        console.log('REDIS-SENTINEL:', options);
      }
      return new Redis(options);
    }

    // redis-cluster://[:pass]@host:port/db
    if (url.startsWith('redis-cluster://')) {
      const match = url.match(/(redis-cluster):\/\/(:([^@]+)@)?([^/]+)\/([0-9]+)(\/.+)?/);
      if (!match) {
        console.log(`${section} - ERROR - can't parse redis-cluster url '%s' should be of form //[:redisPassword@]redisHost[:redisPort][,redisHostN[:redisPortN]]/redisDbNum`, url);
        process.exit(1);
      }

      const hosts = [];
      match[4].split(',').forEach((hp) => {
        const hostport = hp.split(':');
        hosts.push({ host: hostport[0], port: hostport[1] || 6379 });
      });

      const options = { db: parseInt(match[5]) };
      if (match[3] && match[3] !== '') {
        options.password = match[3];
      }

      if (ArkimeUtil.debug > 0) {
        console.log('REDIS-CLUSTER: hosts', hosts, 'options', { redisOptions: options });
      }
      return new Redis.Cluster(hosts, { redisOptions: options });
    }

    console.log(`${section} - Unknown redis url '%s'`, url);
    process.exit(1);
  }

  /**
   * Create a memcached client from the provided url
   * @params {string} url - The memcached url to connect to.
   * @params {string} section - The section this memcached client is being created for
   */
  static createMemcachedClient (url, section) {
    url = ArkimeUtil.sanitizeStr(url);
    // memcached://[user:pass@]server1[:11211],[user:pass@]server2[:11211],...
    if (url.startsWith('memcached://')) {
      if (ArkimeUtil.debug > 0) {
        console.log('MEMCACHED:', section, url);
      }
      return memjs.Client.create(url.substring(12));
    }

    console.log(`${section} - Unknown memcached url '%s'`, url);
    process.exit(1);
  }

  /**
   * Create a LMDB store from the provided url
   * @params {string} url - The LMDB url to connect to.
   * @params {string} section - The section this LMDB client is being created for
   */
  static createLMDBStore (url, section) {
    // eslint-disable-next-line no-shadow
    const { open } = require('lmdb-store');

    try {
      const store = open({
        path: url.slice(7),
        compression: true
      });
      return store;
    } catch (err) {
      console.log('ERROR -', err);
      process.exit(1);
    }
  }

  static wildcardToRegexp (wildcard) {
    // https://stackoverflow.com/revisions/57527468/5
    wildcard = wildcard.replace(/[.+^${}()|[\]\\]/g, '\\$&');
    return new RegExp(`^${wildcard.replace(/\*/g, '.*').replace(/\?/g, '.')}$`, 'i');
  }

  static parseTimeStr (time) {
    if (typeof time !== 'string') {
      return time;
    }

    switch (time[time.length - 1]) {
    case 'w':
      return parseInt(time.slice(0, -1)) * 60 * 60 * 24 * 7;
    case 'd':
      return parseInt(time.slice(0, -1)) * 60 * 60 * 24;
    case 'h':
      return parseInt(time.slice(0, -1)) * 60 * 60;
    case 'm':
      return parseInt(time.slice(0, -1)) * 60;
    case 's':
      return parseInt(time.slice(0, -1));
    default:
      return parseInt(time);
    }
  }

  /**
   * Sends an error from the server by:
   * 1. setting the http content-type header to json
   * 2. setting the response status code (403 default)
   * 3. sending a false success with message text (default "Server Error!")
   * @param {Number} [resStatus=403] - The response status code (optional)
   * @param {String} [text="Server Error!"] - The response text (optional)
   * @returns {Object} res - The Express.js response object
   */
  static serverError (resStatus, text) {
    this.status(resStatus || 403);
    this.setHeader('Content-Type', 'application/json');
    return this.send(
      { success: false, text: text || 'Server Error!' }
    );
  }

  /**
   * Missing resource error handler for static file endpoints.
   * Sends a missing resource message to the client by:
   * 1. setting the response status code to 404 (not found)
   * 2. sending a message that the resource cannot be found
   * This is so the client recieves an understandable message instead of the client index.html
   * @returns {Object} res - The Express.js response object
   */
  static missingResource (err, req, res, next) {
    res.status(404);
    console.log('Cannot locate resource requsted from', ArkimeUtil.sanitizeStr(req.path));
    return res.send('Cannot locate resource');
  }

  /**
   * express error handler
   */
  static expressErrorHandler (err, req, res, next) {
    console.error('Error', ArkimeUtil.sanitizeStr(err.stack));
    res.status(500).send(err.toString());
    next();
  }

  // express middleware to set req.settingUser to who to work on, depending if admin or not
  // This returns fresh from db
  static getSettingUserDb (req, res, next) {
    let userId;

    if (req.query.userId === undefined || req.query.userId === req.user.userId) {
      if (Auth.regressionTests) {
        req.settingUser = req.user;
        return next();
      }

      userId = req.user.userId;
    } else if (!req.user.hasRole('usersAdmin')) {
      // user is trying to get another user's settings without admin privilege
      return res.serverError(403, 'Need admin privileges');
    } else {
      userId = req.query.userId;
    }

    User.getUser(userId, function (err, user) {
      if (err || !user) {
        if (!Auth.passwordSecret) {
          req.settingUser = JSON.parse(JSON.stringify(req.user));
          delete req.settingUser.found;
        } else {
          return res.serverError(403, 'Unknown user');
        }

        return next();
      }

      req.settingUser = user;
      return next();
    });
  }

  /**
   * Breaks a comma or newline separated string of values into an array of values
   * @param {string} string - The comma or newline separated string of values
   * @returns {Array} The array of values parsed from the string
   */
  static commaOrNewlineStringToArray (string) {
    // split string on commas and newlines
    let values = string.split(/[,\n]+/g);

    // remove any empty values
    values = values.filter(function (val) {
      return val !== '';
    });

    return values;
  }
}

module.exports = ArkimeUtil;

const User = require('./user');
