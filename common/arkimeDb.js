/* arkimeDb.js  -- Database connection helpers, use through ArkimeUtil
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

class ArkimeDb {
  // ----------------------------------------------------------------------------
  /**
   * Create a redis client from the provided url
   * @param {string} url - The redis url to connect to.
   * @param {string} section - The section this redis client is being created for
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

      if (ArkimeConfig.debug > 1) {
        console.log('REDIS:', url);
      }
      const Redis = require('ioredis');
      return new Redis(url);
    }

    // redis-sentinel://sentinelPassword:redisPassword@host:port[,hostN:portN]/name/db
    if (url.startsWith('redis-sentinel://')) {
      const match = url.match(/(redis-sentinel):\/\/(([^:]+)?:([^@]+)?@)?([^/]+)\/([^/]+)\/([0-9]+)(\/.+)?/);
      if (!match) {
        console.log(`${section} - ERROR - can't parse redis-sentinel url '%s' should be of form //[sentinelPassword:redisPassword@]sentinelHost[:sentinelPort][,sentinelHostN[:sentinelPortN]]/redisName/redisDbNum`, url);
        process.exit(1);
      }

      const options = { sentinels: [], name: match[6], db: parseInt(match[7]) };
      for (const hp of match[5].split(',')) {
        const hostport = hp.split(':');
        options.sentinels.push({ host: hostport[0], port: hostport[1] || 26379 });
      }

      if (match[3]) {
        options.sentinelPassword = match[3];
      }
      if (match[4]) {
        options.password = match[4];
      }

      if (ArkimeConfig.debug > 1) {
        console.log('REDIS-SENTINEL:', options);
      }
      const Redis = require('ioredis');
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
      for (const hp of match[4].split(',')) {
        const hostport = hp.split(':');
        hosts.push({ host: hostport[0], port: hostport[1] || 6379 });
      }

      const options = { db: parseInt(match[5]) };
      if (match[3]) {
        options.password = match[3];
      }

      if (ArkimeConfig.debug > 1) {
        console.log('REDIS-CLUSTER: hosts', hosts, 'options', { redisOptions: options });
      }
      const Redis = require('ioredis');
      return new Redis.Cluster(hosts, { redisOptions: options });
    }

    console.log(`${section} - Unknown redis url '%s'`, url);
    process.exit(1);
  }

  // ----------------------------------------------------------------------------
  /**
   * Parse the elasticsearch url and return the url and possible auth object
   */
  static createElasticsearchInfo (url) {
    url = url.split(',')[0];
    url = url.replace(/^elasticsearch/, 'http').replace(/^opensearch/, 'http');
    let auth;
    if (url.includes('://usersElasticsearch')) {
      let es, basicAuth;

      if (ArkimeConfig.get('usersElasticsearch')) {
        es = ArkimeConfig.get('usersElasticsearch');
        basicAuth = ArkimeConfig.get('usersElasticsearchBasicAuth');
      }

      if (!es) {
        console.log(`ERROR - No usersElasticsearch defined but used in ${url}`);
        process.exit(1);
      }

      url = url.replace(/^.*:\/\/usersElasticsearch/, es);

      if (basicAuth) {
        if (!basicAuth.includes(':')) {
          basicAuth = Buffer.from(basicAuth, 'base64').toString();
        }
        basicAuth = ArkimeUtil.splitRemain(basicAuth, ':', 1);

        auth = {
          username: basicAuth[0],
          password: basicAuth[1]
        };
      }
    }

    return { url, auth };
  }

  // ----------------------------------------------------------------------------
  /**
   * Create a memcached client from the provided url
   * @param {string} url - The memcached url to connect to.
   * @param {string} section - The section this memcached client is being created for
   */
  static createMemcachedClient (url, section) {
    url = ArkimeUtil.sanitizeStr(url);
    // memcached://[user:pass@]server1[:11211],[user:pass@]server2[:11211],...
    if (url.startsWith('memcached://')) {
      if (ArkimeConfig.debug > 0) {
        console.log('MEMCACHED:', section, url);
      }
      const memjs = require('memjs');
      return memjs.Client.create(url.substring(12));
    }

    console.log(`${section} - Unknown memcached url '%s'`, url);
    process.exit(1);
  }

  // ----------------------------------------------------------------------------
  /**
   * Create a LMDB store from the provided url
   * @param {string} url - The LMDB url to connect to.
   * @param {string} section - The section this LMDB client is being created for
   */
  static createLMDBStore (url, section) {
    // eslint-disable-next-line no-shadow
    const { open } = require('lmdb');

    try {
      const root = open({
        path: url.slice(7),
        compression: true
      });
      if (section) {
        return root.openDB(section);
      }
      return root;
    } catch (err) {
      console.log('ERROR -', err);
      process.exit(1);
    }
  }

  // ----------------------------------------------------------------------------
  static createSQLiteDB (url) {
    const Database = require('better-sqlite3');
    let dbPath;
    if (url.startsWith('sqlite3://')) {
      dbPath = url.slice(10);
    } else if (url.startsWith('sqlite://')) {
      dbPath = url.slice(9);
    } else {
      dbPath = url;
    }

    const db = new Database(dbPath);
    db.pragma('journal_mode = WAL');
    db.pragma('synchronous = NORMAL');
    db.pragma('busy_timeout = 5000');
    return db;
  }
}

module.exports = ArkimeDb;

// At end because of circular require
const ArkimeUtil = require('./arkimeUtil');
const ArkimeConfig = require('./arkimeConfig');
