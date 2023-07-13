/* arkimeConfig.js  -- Shared config
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

const ArkimeUtil = require('./arkimeUtil');
const fs = require('fs');
const axios = require('axios');
const ini = require('iniparser');

class ArkimeConfig {
  static #debug = 0;
  static #override = new Map();
  static #debugged = new Map();
  static #config;
  static #configImpl;
  static #parsers = {};
  static #uri;

  static async initialize (options) {
    ArkimeConfig.#debug = options.debug ?? 0;

    // The config is actually hidden
    if (options.configFile.endsWith('.hiddenconfig')) {
      options.configFile = fs.readFileSync(options.configFile).toString().split('\n')[0].trim();
    }
    if (options.configFile.startsWith('urlinfile://')) {
      options.configFile = fs.readFileSync(options.configFile.substring(12)).toString().split('\n')[0].trim();
    }

    ArkimeConfig.#uri = options.configFile;

    const parts = ArkimeConfig.#uri.split('://');

    if (parts.length === 1) {
      try { // check if the file exists
        fs.accessSync(ArkimeConfig.#uri, fs.constants.F_OK);
      } catch (err) { // if the file doesn't exist, create it
        console.log(`WARNING - ${ArkimeConfig.#uri} doesn't exist`);
        ArkimeConfig.#config = {};
        return;
      }

      if (ArkimeConfig.#uri.endsWith('json')) {
        ArkimeConfig.#configImpl = ArkimeConfig.#parsers.json;
      } else {
        ArkimeConfig.#configImpl = ArkimeConfig.#parsers.ini;
      }
    } else {
      ArkimeConfig.#configImpl = ArkimeConfig.#parsers[parts[0]];
    }

    ArkimeConfig.#config = await ArkimeConfig.#configImpl.load(ArkimeConfig.#uri);
  }

  static async reload () {
    ArkimeConfig.#config = await ArkimeConfig.#configImpl.load(ArkimeConfig.#uri);
  }

  static setOverride (key, value) {
    ArkimeConfig.#override.set(key, value);
  }

  static setDebug (debug) {
    ArkimeConfig.#debug = debug;
  }

  static get (section, sectionKey, d) {
    const key = `${section}.${sectionKey}`;
    const value = ArkimeConfig.#override.get(key) ?? ArkimeConfig.#config?.[section]?.[sectionKey] ?? d;

    if (ArkimeConfig.#debug > 0 && !ArkimeConfig.#debugged.has(key)) {
      ArkimeConfig.#debugged.set(key, true);
    }

    if (value === 'false') { return false; }
    if (value === 'true') { return true; }

    return value;
  }

  static registerParser (str, parser) {
    ArkimeConfig.#parsers[str] = parser;
  }

  static replace (config) {
    ArkimeConfig.#config = config;
  }

  static save (cb) {
    ArkimeConfig.#configImpl.save(ArkimeConfig.#uri, ArkimeConfig.#config, cb);
  }

  static loadIncludes (includes) {
    if (!includes) {
      return;
    }
    includes.split(';').forEach((file) => {
      const ignoreError = file[0] === '-';
      if (ignoreError) {
        file = file.substring(1);
      }

      if (!fs.existsSync(file)) {
        if (ignoreError) {
          return;
        }
        console.log("ERROR - Couldn't open config includes file '%s'", file);
        process.exit(1);
      }
      const config = ini.parseSync(file);
      for (const group in config) {
        if (!ArkimeConfig.#config[group]) {
          ArkimeConfig.#config[group] = config[group];
        } else {
          for (const key in config[group]) {
            ArkimeConfig.#config[group][key] = config[group][key];
          }
        }
      }
    });
  }

  // ----------------------------------------------------------------------------
  /**
   * Get a list of all the sections in the config file
   *
   * @returns {string|Array} - A list of all the sections in the config file
   */
  static getSections () {
    return Object.keys(ArkimeConfig.#config);
  }

  // ----------------------------------------------------------------------------
  /**
   * Get the full config for a section
   *
   * @param {string} section - The section of the config file to return
   * @returns {object} - A list of all the sections in the config file
   */
  static getSection (section) {
    return ArkimeConfig.#config[section];
  }
}

// ----------------------------------------------------------------------------
// Config Schemes - For each scheme supported implement a load/save function
// ----------------------------------------------------------------------------
class ConfigIni {
  static async load (uri) {
    return ini.parseSync(uri);
  }

  static save (config, uri, cb) {
    function encode (str) {
      return typeof (str) === 'string' ? str.replace(/[\n\r]/g, '\\n') : str;
    }
    let output = '';
    Object.keys(config).forEach((section) => {
      output += `[${encode(section)}]\n`;
      Object.keys(config[section]).forEach((key) => {
        output += `${key}=${encode(config[section][key])}\n`;
      });
    });

    try {
      fs.writeFileSync(uri, output);
      cb(null);
    } catch (e) {
      cb(e.message);
    }
  }
}
ArkimeConfig.registerParser('ini', ConfigIni);

// ----------------------------------------------------------------------------

class ConfigJson {
  static async load (uri) {
    return JSON.parse(fs.readFileSync(uri, 'utf8'));
  }

  static save (uri, config, cb) {
    try {
      fs.writeFileSync(uri, JSON.stringify(config, null, 1));
      cb();
    } catch (e) {
      cb(e.message);
    }
  }
}
ArkimeConfig.registerParser('json', ConfigJson);

// ----------------------------------------------------------------------------
// redis://[:pass]@host:port/db/key
class ConfigRedis {
  static #redisKey;
  static #redis;
  static async load (uri) {
    const redisParts = uri.split('/');
    if (redisParts.length !== 5) {
      throw new Error(`Invalid redis url - ${redisParts[0]}//[:pass@]redishost[:redisport]/redisDbNum/key`);
    }
    ConfigRedis.#redisKey = redisParts.pop();
    ConfigRedis.#redis = ArkimeUtil.createRedisClient(redisParts.join('/'), 'config');

    const result = await ConfigRedis.#redis.get(ConfigRedis.#redisKey);
    if (result === null) {
      return {};
    } else {
      return JSON.parse(result);
    }
  }

  save (uri, config, cb) {
    ConfigRedis.#redis.set(ConfigRedis.#redisKey, JSON.stringify(config), function (err, result) {
      cb(err);
    });
  }
};
ArkimeConfig.registerParser('redis', ConfigRedis);

// ----------------------------------------------------------------------------
// rediss://pass@host:port/db/key
ArkimeConfig.registerParser('rediss', ConfigRedis);

// redis-sentinel://sentinelPassword:redisPassword@host:port/name/db/key
class ConfigRedisSentinel {
  static #redisKey;
  static #redis;
  static async load (uri) {
    const redisParts = uri.split('/');
    redisParts[1] = 'stoperror';
    if (redisParts.length !== 6 || redisParts.some(p => p === '')) {
      throw new Error(`Invalid redis-sentinel url - ${redisParts[0]}//[sentinelPassword:redisPassword@]sentinelHost[:sentinelPort][,sentinelPortN:sentinelPortN]/redisName/redisDbNum`);
    }
    ConfigRedisSentinel.#redisKey = redisParts[5];
    ConfigRedisSentinel.#redis = ArkimeUtil.createRedisClient(uri, 'config');

    const result = await ConfigRedisSentinel.#redis.get(ConfigRedisSentinel.#redisKey);
    if (result === null) {
      return {};
    } else {
      return JSON.parse(result);
    }
  }

  save (uri, config, cb) {
    ConfigRedisSentinel.#redis.set(ConfigRedisSentinel.#redisKey, JSON.stringify(config), function (err, result) {
      cb(err);
    });
  }
};
ArkimeConfig.registerParser('redis-sentinel', ConfigRedisSentinel);

// ----------------------------------------------------------------------------
// redis-cluster://[:pass]@host:port/db/key
class ConfigRedisCluster {
  static #redisKey;
  static #redis;
  static async load (uri) {
    const redisParts = uri.split('/');
    redisParts[1] = 'stoperror';
    if (redisParts.length !== 5 || redisParts.some(p => p === '')) {
      throw new Error(`Invalid redis-cluster url - ${redisParts[0]}//[:redisPassword@]redishost[:redisport]/redisDbNum/key`);
    }
    ConfigRedisCluster.#redisKey = redisParts[4];
    ConfigRedisCluster.#redis = ArkimeUtil.createRedisClient(uri, 'config');

    const result = await ConfigRedisCluster.#redis.get(ConfigRedisCluster.#redisKey);
    if (result === null) {
      return {};
    } else {
      return JSON.parse(result);
    }
  }

  save (uri, config, cb) {
    ConfigRedisCluster.#redis.set(ConfigRedisCluster.#redisKey, JSON.stringify(config), function (err, result) {
      cb(err);
    });
  }
};
ArkimeConfig.registerParser('redis-cluster', ConfigRedisCluster);

// ----------------------------------------------------------------------------
class ConfigElasticsearch {
  static async load (uri) {
    const url = uri.replace('elasticsearch', 'http').replace('opensearch', 'http');
    if (!url.includes('/_doc/')) {
      throw new Error('Missing _doc in url, should be format elasticsearch://user:pass@host:port/INDEX/_doc/DOC');
    }

    try {
      const response = await axios.get(url);
      return response.data._source;
    } catch (error) {
      if (error.response && error.response.status === 404) {
        return {};
      }
      throw error;
    }
  }

  static save (uri, config, cb) {
    const url = uri.replace('elasticsearchs', 'https').replace('opensearchs', 'https').replace('elasticsearch', 'http').replace('opensearch', 'http');

    axios.post(url, JSON.stringify(config), { headers: { 'Content-Type': 'application/json' } })
      .then((response) => {
        cb(null);
      })
      .catch((error) => {
        cb(error);
      });
  }
};
ArkimeConfig.registerParser('elasticsearch', ConfigElasticsearch);
ArkimeConfig.registerParser('opensearch', ConfigElasticsearch);
ArkimeConfig.registerParser('elasticsearchs', ConfigElasticsearch);
ArkimeConfig.registerParser('opensearchs', ConfigElasticsearch);

// ----------------------------------------------------------------------------
class ConfigHttp {
  static async load (uri) {
    try {
      const response = await axios.get(uri);
      console.log('response.data', response.data);
      if (uri.endsWith('.ini')) {
        return ini.parseString(response.data);
      } else {
        return JSON.parse(response.data);
      }
    } catch (error) {
      if (error.response && error.response.status === 404) {
        return {};
      }
      throw error;
    }
  }

  static save (uri, config, cb) {
    console.log('ERROR - Can not SAVE to', uri);
  }
};
ArkimeConfig.registerParser('http', ConfigHttp);
ArkimeConfig.registerParser('https', ConfigHttp);

module.exports = ArkimeConfig;
