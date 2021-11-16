/* arkimeUtil.js  -- Shared util functions
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

const Redis = require('ioredis');
const memjs = require('memjs');

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
    // redis://[:pass]@host:port/db
    if (url.startsWith('redis://') || url.startsWith('rediss://')) {
      const match = url.match(/(rediss?):\/\/(:[^@]+@)?([^:/]+)(:[0-9]+)?\/([0-9]+)/);
      if (!match) {
        console.log(`${section} - ERROR - can't parse redis url '${url}' should be of form //[:pass@]redishost[:redisport]/redisDbNum`);
        process.exit(1);
      }

      if (ArkimeUtil.debug > 0) {
        console.log('REDIS:', url);
      }
      return new Redis(url);
    }

    // redis-sentinel://sentinelPassword:redisPassword@host:port[,hostN;portN]/name/db
    if (url.startsWith('redis-sentinel://')) {
      const match = url.match(/(redis-sentinel):\/\/(([^:]+)?:([^@]+)?@)?([^/]+)\/([^/]+)\/([0-9]+)(\/.+)?/);
      if (!match) {
        console.log(`${section} - ERROR - can't parse redis-sentinel url '${url}' should be of form //[sentinelPassword:redisPassword@]sentinelHost[:sentinelPort][,sentinelPortN[:sentinelPortN]]/redisName/redisDbNum`);
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

      if (ArkimeUtil.debug > 0) {
        console.log('REDIS-SENTINEL:', options);
      }
      return new Redis(options);
    }

    // redis-cluster://[:pass]@host:port/db
    if (url.startsWith('redis-cluster://')) {
      const match = url.match(/(redis-cluster):\/\/(:([^@]+)@)?([^/]+)\/([0-9]+)(\/.+)?/);
      if (!match) {
        console.log(`${section} - ERROR - can't parse redis-cluster url '${url}' should be of form //[:redisPassword@]redisHost[:redisPort][,redisHostN[:redisPortN]]/redisDbNum`);
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

    console.log(`${section} - Unknown redis url '${url}'`);
    process.exit(1);
  }

  /**
   * Create a memcached client from the provided url
   * @params {string} url - The memcached url to connect to.
   * @params {string} section - The section this memcached client is being created for
   */
  static createMemcachedClient (url, section) {
    // memcached://[user:pass@]server1[:11211],[user:pass@]server2[:11211],...
    if (url.startsWith('memcached://')) {
      if (ArkimeUtil.debug > 0) {
        console.log('MEMCACHED:', section, url);
      }
      return memjs.Client.create(url.substring(12));
    }

    console.log(`${section} - Unknown memcached url '${url}'`);
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
}

module.exports = ArkimeUtil;
