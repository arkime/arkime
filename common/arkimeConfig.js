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
  static debug = 0;
  static regressionTests = false;
  static insecure = false;

  static exit = Symbol('ArkimeConfig - exit');
  static throw = Symbol('ArkimeConfig - throw');

  // ----------------------------------------------------------------------------

  static #override = new Map();
  static #debugged = new Map();
  static #config;
  static #configImpl;
  static #schemes = {};
  static #configFile;
  static #uri;
  static #dumpConfig;

  // ----------------------------------------------------------------------------
  /**
   * Initialize the ArkimeConfig subsystem
   * @param {string} options.defaultConfigFile what the default config file path is
   */
  static async initialize (options) {
    ArkimeConfig.#configFile ??= options.defaultConfigFile;

    // The config is actually hidden
    if (ArkimeConfig.#configFile.endsWith('.hiddenconfig')) {
      ArkimeConfig.#configFile = fs.readFileSync(ArkimeConfig.#configFile).toString().split('\n')[0].trim();
    }
    if (ArkimeConfig.#configFile.startsWith('urlinfile://')) {
      ArkimeConfig.#configFile = fs.readFileSync(ArkimeConfig.#configFile.substring(12)).toString().split('\n')[0].trim();
    }

    ArkimeConfig.#uri = ArkimeConfig.#configFile;

    const parts = ArkimeConfig.#uri.split('://');

    if (parts.length === 1) {
      try { // check if the file exists
        fs.accessSync(ArkimeConfig.#uri, fs.constants.F_OK);
      } catch (err) { // if the file doesn't exist, create it
        console.log(`WARNING - ${ArkimeConfig.#uri} doesn't exist`);
        ArkimeConfig.#config = {};

        if (ArkimeConfig.#dumpConfig) {
          console.error('OVERRIDE', ArkimeConfig.#override);
          console.error('CONFIG', ArkimeConfig.#config);
          if (ArkimeConfig.regressionTests) { process.exit(); }
        }
        return;
      }

      if (ArkimeConfig.#uri.endsWith('json')) {
        ArkimeConfig.#configImpl = ArkimeConfig.#schemes.json;
      } else {
        ArkimeConfig.#configImpl = ArkimeConfig.#schemes.ini;
      }
    } else {
      ArkimeConfig.#configImpl = ArkimeConfig.#schemes[parts[0]];
    }

    await ArkimeConfig.reload();
  }

  // ----------------------------------------------------------------------------
  static get configFile () {
    return ArkimeConfig.#configFile;
  }

  // ----------------------------------------------------------------------------
  /**
   * Reload the config file
   */
  static async reload () {
    ArkimeConfig.#config = await ArkimeConfig.#configImpl.load(ArkimeConfig.#uri);
    if (ArkimeConfig.#dumpConfig) {
      console.error('OVERRIDE', ArkimeConfig.#override);
      console.error('CONFIG', ArkimeConfig.#config);
      if (ArkimeConfig.regressionTests) { process.exit(); }
    }
  }

  // ----------------------------------------------------------------------------
  /**
   * Set a config override value
   * @param {string} key The section.key to override
   * @param {string} value The value for the key
   */
  static setOverride (key, value) {
    ArkimeConfig.#override.set(key, value);
  }

  // ----------------------------------------------------------------------------
  /**
   * Get a config value
   * @param {string[] | string} sections The sections the key lives in, can also be a string
   * @param {string} sectionKey The key in the section to get the value for
   * @param {string} d=undefined The default value to return if sectionKey isn't found
   */
  static get (sections, sectionKey, d) {
    if (!Array.isArray(sections)) { sections = [sections]; }

    let value;
    let key;

    for (const section of sections) {
      key = `${section}.${sectionKey}`;
      value = ArkimeConfig.#override.get(key) ?? ArkimeConfig.#config?.[section]?.[sectionKey];
      if (value !== undefined) { break; }
    }

    if (value === undefined) {
      if (d === ArkimeConfig.exit) {
        console.log(`ERROR - ${sectionKey} not found in sections: ${sections}`);
        process.exit();
      }
      if (d === ArkimeConfig.throw) {
        throw new Error(`${sectionKey} not found in sections: ${sections}`);
      }
      key = `defaultValue ${sectionKey}`;
      value = d;
    }

    if (ArkimeConfig.debug > 0 && !ArkimeConfig.#debugged.has(key)) {
      console.log(`CONFIG - ${key} is ${value}`);
      ArkimeConfig.#debugged.set(key, true);
    }

    if (value === 'false') { return false; }
    if (value === 'true') { return true; }

    return value;
  }

  // ----------------------------------------------------------------------------
  static registerScheme (str, parser) {
    ArkimeConfig.#schemes[str] = parser;
  }

  // ----------------------------------------------------------------------------
  static replace (config) {
    ArkimeConfig.#config = config;
  }

  // ----------------------------------------------------------------------------
  /**
   * Does the config implementation support saving
   */
  static canSave () {
    return ArkimeConfig.#configImpl.save !== undefined;
  }

  // ----------------------------------------------------------------------------
  /**
   * Save the config
   */
  static save (cb) {
    if (ArkimeConfig.#configImpl.save) {
      ArkimeConfig.#configImpl.save(ArkimeConfig.#uri, ArkimeConfig.#config, cb);
    } else {
      console.log('WARNING - Saving config is not supported by', ArkimeConfig.#uri);
    }
  }

  // ----------------------------------------------------------------------------
  /**
   * Load include files, currently these must be local in ini format
   */
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

  // ----------------------------------------------------------------------------
  static processArgs () {
    const args = [];
    for (let i = 0, ilen = process.argv.length; i < ilen; i++) {
      if (process.argv[i] === '-c' || process.argv[i] === '--config') {
        i++;
        ArkimeConfig.#configFile = process.argv[i];
      } else if (process.argv[i] === '--debug') {
        ArkimeConfig.debug++;
      } else if (process.argv[i] === '--insecure') {
        ArkimeConfig.insecure = true;
      } else if (process.argv[i] === '--regressionTests') {
        ArkimeConfig.regressionTests = true;
      } else if (process.argv[i] === '--dumpConfig') {
        ArkimeConfig.#dumpConfig = true;
      } else {
        args.push(process.argv[i]);
      }
    }
    process.argv = args;

    if (ArkimeConfig.debug > 0) {
      console.log('Debug Level', ArkimeConfig.debug);
    }
  }
}

ArkimeConfig.processArgs();

// ----------------------------------------------------------------------------
// Config Schemes - For each scheme supported implement a load/save function
// ----------------------------------------------------------------------------
class ConfigIni {
  static async load (uri) {
    return ini.parseSync(uri);
  }

  static save (uri, config, cb) {
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
ArkimeConfig.registerScheme('ini', ConfigIni);

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
ArkimeConfig.registerScheme('json', ConfigJson);

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
ArkimeConfig.registerScheme('redis', ConfigRedis);

// ----------------------------------------------------------------------------
// rediss://pass@host:port/db/key
ArkimeConfig.registerScheme('rediss', ConfigRedis);

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
ArkimeConfig.registerScheme('redis-sentinel', ConfigRedisSentinel);

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
ArkimeConfig.registerScheme('redis-cluster', ConfigRedisCluster);

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
ArkimeConfig.registerScheme('elasticsearch', ConfigElasticsearch);
ArkimeConfig.registerScheme('opensearch', ConfigElasticsearch);
ArkimeConfig.registerScheme('elasticsearchs', ConfigElasticsearch);
ArkimeConfig.registerScheme('opensearchs', ConfigElasticsearch);

// ----------------------------------------------------------------------------
class ConfigHttp {
  static async load (uri) {
    try {
      const response = await axios.get(uri);
      if (typeof response.data === 'object') {
        return response.data;
      } else if (uri.endsWith('.ini')) {
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
};
ArkimeConfig.registerScheme('http', ConfigHttp);
ArkimeConfig.registerScheme('https', ConfigHttp);

module.exports = ArkimeConfig;
