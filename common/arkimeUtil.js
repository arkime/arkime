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
const util = require('util');
const fs = require('fs');
const bodyParser = require('body-parser');
const sjson = require('secure-json-parse');
const http = require('http');
const https = require('https');
const path = require('path');
// eslint-disable-next-line no-shadow
const crypto = require('crypto');
const logger = require('morgan');
const express = require('express');

class ArkimeUtil {
  static adminRole;

  // ----------------------------------------------------------------------------
  /**
   * A json body parser that doesn't allow anything that looks like "__proto__": or "constructor":
   */
  static jsonParser = bodyParser.json({
    verify: function (req, res, buf, encoding) {
      sjson.parse(buf);
    }
  });

  // ---------------------------------------------------------------------------
  /**
   * For both arrays and single values escape entities
   */
  static safeStr (str) {
    if (Array.isArray(str)) { return str.map(x => ArkimeUtil.safeStr(x)); }

    return str.replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;')
      .replace(/\//g, '&#47;');
  };

  // ----------------------------------------------------------------------------
  /**
   * Replace ESC character with ESC. This should be used when console.log of
   * any user input to stop ESC 52 issues
   */
  static sanitizeStr (str) {
    if (!str) { return str; }
    if (typeof str === 'object') { str = util.inspect(str); }
    // eslint-disable-next-line no-control-regex
    return str.replace(/\u001b/g, '*ESC*');
  }

  // ----------------------------------------------------------------------------
  /**
   * Remove any special characters except ('-', '_', ':', and ' ')
   */
  static removeSpecialChars (str) {
    if (!str) { return str; }
    return str.replace(/[^-a-zA-Z0-9_: ]/g, '');
  }

  // ----------------------------------------------------------------------------
  /**
   * Express middleware to set some common headers for json responses
   */
  static noCacheJson (req, res, next) {
    res.header('Cache-Control', 'no-cache, private, no-store, must-revalidate, max-stale=0, post-check=0, pre-check=0');
    res.header('Content-Type', 'application/json');
    res.header('X-Content-Type-Options', 'nosniff');
    return next();
  }

  // ----------------------------------------------------------------------------
  static noCache (req, res, ct) {
    res.header('Cache-Control', 'no-cache, private, no-store, must-revalidate, max-stale=0, post-check=0, pre-check=0');
    if (ct) {
      res.setHeader('Content-Type', ct);
      res.header('X-Content-Type-Options', 'nosniff');
    }
  };

  // ----------------------------------------------------------------------------
  /**
   * Is there any intersection between the two arrays
   */
  static arrayIncludes (arr1, arr2) {
    return arr2.some(v => arr1.includes(v));
  }

  /**
   * Only split up to limit items and add the remaining as one element at the end of array
   */
  // https://coderwall.com/p/pq0usg/javascript-string-split-that-ll-return-the-remainder
  static splitRemain (str, separator, limit) {
    str = str.split(separator);
    if (str.length <= limit) { return str; }

    const ret = str.splice(0, limit);
    ret.push(str.join(separator));

    return ret;
  };

  // ----------------------------------------------------------------------------
  /**
   * Is str a string and a length of at least len
   */
  static isString (str, minLen = 1) {
    return typeof str === 'string' && str.length >= minLen;
  }

  // ----------------------------------------------------------------------------
  /**
   * Is arr an array of strings, with each string being at least minLen
   */
  static isStringArray (arr, minLen = 1) {
    if (!Array.isArray(arr)) {
      return false;
    }

    for (const str of arr) {
      if (typeof str !== 'string' || str.length < minLen) {
        return false;
      }
    }
    return true;
  }

  // ----------------------------------------------------------------------------
  /**
   * Is obj an object
   */
  static isObject (obj) {
    return typeof obj === 'object' && obj !== null;
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

      if (ArkimeConfig.debug > 1) {
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

      if (ArkimeConfig.debug > 1) {
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

      if (ArkimeConfig.debug > 1) {
        console.log('REDIS-CLUSTER: hosts', hosts, 'options', { redisOptions: options });
      }
      return new Redis.Cluster(hosts, { redisOptions: options });
    }

    console.log(`${section} - Unknown redis url '%s'`, url);
    process.exit(1);
  }

  // ----------------------------------------------------------------------------
  /**
   * Create a memcached client from the provided url
   * @params {string} url - The memcached url to connect to.
   * @params {string} section - The section this memcached client is being created for
   */
  static createMemcachedClient (url, section) {
    url = ArkimeUtil.sanitizeStr(url);
    // memcached://[user:pass@]server1[:11211],[user:pass@]server2[:11211],...
    if (url.startsWith('memcached://')) {
      if (ArkimeConfig.debug > 0) {
        console.log('MEMCACHED:', section, url);
      }
      return memjs.Client.create(url.substring(12));
    }

    console.log(`${section} - Unknown memcached url '%s'`, url);
    process.exit(1);
  }

  // ----------------------------------------------------------------------------
  /**
   * Create a LMDB store from the provided url
   * @params {string} url - The LMDB url to connect to.
   * @params {string} section - The section this LMDB client is being created for
   */
  static createLMDBStore (url, section) {
    // eslint-disable-next-line no-shadow
    const { open } = require('lmdb');

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

  // ----------------------------------------------------------------------------
  static wildcardToRegexp (wildcard) {
    // https://stackoverflow.com/revisions/57527468/5
    wildcard = wildcard.replace(/[.+^${}()|[\]\\]/g, '\\$&');
    return new RegExp(`^${wildcard.replace(/\*/g, '.*').replace(/\?/g, '.')}$`, 'i');
  }

  // ----------------------------------------------------------------------------
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

  // ----------------------------------------------------------------------------
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

  // ----------------------------------------------------------------------------
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

  // ----------------------------------------------------------------------------
  /**
   * express error handler
   */
  static expressErrorHandler (err, req, res, next) {
    console.error('Error', ArkimeUtil.sanitizeStr(err.stack));
    res.status(500).send(err.toString());
    next();
  }

  // ----------------------------------------------------------------------------
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

  // ----------------------------------------------------------------------------
  /**
   * Breaks file of certificates into an array of separate certificates
   * @param {string} string - The file containing certificates
   * @returns {Array} The array of values parsed from the string
   */
  static certificateFileToArray (certificateFile) {
    if (certificateFile && certificateFile.length > 0) {
      const certs = [];
      const certificateFileLines = fs.readFileSync(certificateFile, 'utf8').split('\n');

      let foundCert = [];

      for (let i = 0, ilen = certificateFileLines.length; i < ilen; i++) {
        const line = certificateFileLines[i];
        if (line.length === 0) {
          continue;
        }
        foundCert.push(line);
        if (line.match(/-END CERTIFICATE-/)) {
          certs.push(foundCert.join('\n'));
          foundCert = [];
        }
      }

      if (certs.length > 0) {
        return certs;
      }
    }

    return undefined;
  };

  // ----------------------------------------------------------------------------
  /**
   * Check the Arkime Schema Version
   */
  static async checkArkimeSchemaVersion (esClient, prefix, minVersion) {
    if (prefix.length > 0 && !prefix.endsWith('_')) {
      prefix += '_';
    }

    try {
      const { body: doc } = await esClient.indices.getTemplate({
        name: `${prefix}sessions3_template`,
        filter_path: '**._meta'
      });

      try {
        const molochDbVersion = doc[`${prefix}sessions3_template`].mappings._meta.molochDbVersion;

        if (molochDbVersion < minVersion) {
          console.log(`ERROR - Current database version (${molochDbVersion}) is less then required version (${minVersion}) use 'db/db.pl <eshost:esport> upgrade' to upgrade`);
          if (doc._node) {
            console.log(`On node ${doc._node}`);
          }
          process.exit(1);
        }
      } catch (e) {
        console.log("ERROR - Couldn't find database version.  Have you run ./db.pl host:port upgrade?", e);
        process.exit(0);
      }
    } catch (err) {
      console.log("ERROR - Couldn't retrieve database version, is OpenSearch/Elasticsearch running?  Have you run ./db.pl host:port init?", err);
      process.exit(0);
    }
  }

  // ----------------------------------------------------------------------------
  /**
   * Callback when of the cert files change
   */
  static #fsWait;
  static #httpsServer;
  static #watchHttpsFile (e, filename) {
    if (ArkimeUtil.#fsWait) { clearTimeout(ArkimeUtil.#fsWait); };

    // We wait 10s from last event incase there are more events
    ArkimeUtil.#fsWait = setTimeout(() => {
      ArkimeUtil.#fsWait = null;
      try { // try to get the new cert files
        const keyFileData = fs.readFileSync(ArkimeConfig.get('keyFile'));
        const certFileData = fs.readFileSync(ArkimeConfig.get('certFile'));

        console.log('Reloading cert...');

        const options = { // set new server cert options
          key: keyFileData,
          cert: certFileData,
          secureOptions: crypto.constants.SSL_OP_NO_TLSv1
        };

        try {
          ArkimeUtil.#httpsServer.setSecureContext(options);
        } catch (err) {
          console.log('ERROR cert not reloaded: ', err.toString());
        }
      } catch (err) { // don't continue if we can't read them
        console.log('Missing cert or key files. Cannot reload cert.');
        return;
      }
    }, 10000);
  }

  // ----------------------------------------------------------------------------
  /**
   * Create HTTP/HTTPS Server, load cert if HTTPS, listen, drop priv
   */
  static createHttpServer (app, host, port, listenCb) {
    let server;

    if (ArkimeConfig.get('keyFile') && ArkimeConfig.get('certFile')) {
      const keyFileData = fs.readFileSync(ArkimeConfig.get('keyFile'));
      const certFileData = fs.readFileSync(ArkimeConfig.get('certFile'));

      // watch the cert and key files
      fs.watch(ArkimeConfig.get('keyFile'), { persistent: false }, ArkimeUtil.#watchHttpsFile);
      fs.watch(ArkimeConfig.get('certFile'), { persistent: false }, ArkimeUtil.#watchHttpsFile);

      if (ArkimeConfig.debug > 1) {
        console.log('Watching cert and key files. If either is changed, the server will be updated with the new files.');
      }

      server = ArkimeUtil.#httpsServer = https.createServer({
        key: keyFileData,
        cert: certFileData,
        secureOptions: crypto.constants.SSL_OP_NO_TLSv1
      }, app);
    } else {
      server = http.createServer(app);
    }

    server
      .on('error', (e) => {
        console.log("ERROR - couldn't listen on host %s port %d is %s already running?", host, port, path.basename(process.argv[1]));
        process.exit(1);
      })
      .on('listening', (e) => {
        console.log('Express server listening on host %s port %d in %s mode', server.address().address, server.address().port, app.settings.env);
      })
      .listen({ port, host }, listenCb);

    // If root drop priv when dropGroup or dropUser set
    if (process.getuid() === 0) {
      const group = ArkimeConfig.get('dropGroup', null);
      if (group !== null) {
        process.setgid(group);
      }

      const user = ArkimeConfig.get('dropUser', null);
      if (user !== null) {
        process.setuid(user);
      }
    }

    return server;
  }

  // ----------------------------------------------------------------------------
  /**
   * Foramt the prefix
   */
  static formatPrefix (prefix) {
    if (prefix === undefined) {
      return 'arkime_';
    }

    if (prefix === '') {
      return '';
    }

    if (prefix.endsWith('_')) {
      return prefix;
    }

    return prefix + '_';
  }

  // ----------------------------------------------------------------------------
  /**
   * Setup logger
   */
  static logger (app) {
    const loggerApp = express.Router();
    app.use(loggerApp);
    ArkimeConfig.loaded(() => {
      // send req to access log file or stdout
      let stream = process.stdout;
      const accessLogFile = ArkimeConfig.get('accessLogFile');
      if (accessLogFile) {
        stream = fs.createWriteStream(accessLogFile, { flags: 'a' });
      }

      const accessLogFormat = decodeURIComponent(ArkimeConfig.get(
        'accessLogFormat',
        ':date :username %1b[1m:method%1b[0m %1b[33m:url%1b[0m :status :res[content-length] bytes :response-time ms'
      ));

      const accessLogSuppressPaths = ArkimeConfig.getArray('accessLogSuppressPaths', '');

      loggerApp.use(logger(accessLogFormat, {
        stream,
        skip: (req, res) => { return accessLogSuppressPaths.includes(req.path); }
      }));

      logger.token('username', (req, res) => {
        return req.user ? req.user.userId : '-';
      });
    });
  }

  // ----------------------------------------------------------------------------
  /*
   * make sure a string ip is valid by expanding with 0's at the end
   */
  static expandIp (ip) {
    if (ip.includes(':')) {
      if (ip.includes('::')) {
        return ip;
      } else {
        const parts = ip.split(':');
        if (parts.length === 8) {
          return ip;
        }
        return ip + '::0';
      }
    } else {
      const parts = ip.split('.');
      while (parts.length < 4) {
        parts.push('0');
      }
      return parts.join('.');
    }
  }
}

module.exports = ArkimeUtil;

// At end because of circular require
const ArkimeConfig = require('./arkimeConfig');
