/******************************************************************************/
/* config.js -- Code dealing with the config file, command line arguments,
 *              and dropping privileges
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

/// ///////////////////////////////////////////////////////////////////////////////
/// / Command Line Parsing
/// ///////////////////////////////////////////////////////////////////////////////
const os = require('os');
const version = require('../common/version');
const Auth = require('../common/auth');
const ArkimeConfig = require('../common/arkimeConfig');

const internals = {
  hostName: os.hostname(),
  fields: [],
  fieldsMap: {},
  categories: {},
  options: new Map(),
  debugged: new Map()
};

class Config {
  static get debug () {
    return ArkimeConfig.debug;
  }

  static esProfile = false;

  static #keyFileLocation;
  static #certFileLocation;
  static keyFileData;
  static certFileData;

  // ----------------------------------------------------------------------------
  static processArgs () {
    const args = [];
    const doverrides = [];
    for (let i = 0, ilen = process.argv.length; i < ilen; i++) {
      if (process.argv[i] === '--host') {
        i++;
        internals.hostName = process.argv[i];
      } else if (process.argv[i] === '-n' || process.argv[i] === '--node') {
        i++;
        internals.nodeName = process.argv[i];
      } else if (process.argv[i] === '-o' || process.argv[i] === '--option') {
        i++;
        const equal = process.argv[i].indexOf('=');
        if (equal === -1) {
          console.log('Missing equal sign in', process.argv[i]);
          process.exit(1);
        }

        const key = process.argv[i].slice(0, equal);
        const value = process.argv[i].slice(equal + 1);
        if (key.includes('.')) {
          ArkimeConfig.setOverride(key, value);
        } else {
          // Need to do later since nodeName can change
          doverrides.push({ key, value });
        }
      } else if (process.argv[i] === '--esprofile') {
        Config.esProfile = true;
      } else {
        args.push(process.argv[i]);
      }
    }
    process.argv = args;

    if (!internals.nodeName) {
      internals.nodeName = internals.hostName.split('.')[0];
    }

    for (const obj of doverrides) {
      ArkimeConfig.setOverride(`default.${obj.key}`, obj.value);
      ArkimeConfig.setOverride(`${internals.nodeName}.${obj.key}`, obj.value);
    }
  }

  /// ///////////////////////////////////////////////////////////////////////////////
  // Config File & Dropping Privileges
  /// ///////////////////////////////////////////////////////////////////////////////

  // ----------------------------------------------------------------------------
  static sectionGet = ArkimeConfig.get;

  // ----------------------------------------------------------------------------
  static getFull (node, key, defaultValue) {
    return ArkimeConfig.get([node, 'default'], key, defaultValue);
  }

  // ----------------------------------------------------------------------------
  static get (key, defaultValue) {
    return ArkimeConfig.get([internals.nodeName, 'default'], key, defaultValue);
  };

  // ----------------------------------------------------------------------------
  static getBoolFull (node, key, defaultValue) {
    const value = Config.getFull(node, key);
    if (value !== undefined) {
      if (value === 'true' || value === '1') {
        return true;
      } else if (value === 'false' || value === '0') {
        return false;
      } else {
        console.log('ERROR - invalid value for ', key);
      }
    }
    return defaultValue;
  };

  // ----------------------------------------------------------------------------
  static getBool (key, defaultValue) {
    return Config.getBoolFull(internals.nodeName, key, defaultValue);
  };

  // ----------------------------------------------------------------------------
  // Return an array split on separator, remove leading/trailing spaces, remove empty elements
  static getArray (key, separator, defaultValue) {
    const value = Config.get(key, defaultValue);
    if (typeof value === 'string') {
      return value.split(separator).map(s => s.trim()).filter(s => s.match(/^\S+$/));
    } else {
      return value;
    }
  };

  // ----------------------------------------------------------------------------
  static getObj (key, defaultValue) {
    const full = Config.getFull(internals.nodeName, key, defaultValue);
    if (!full) {
      return null;
    }

    const obj = {};
    full.split(';').forEach((element) => {
      const parts = element.split('=');
      if (parts && parts.length === 2) {
        if (parts[1] === 'true') {
          parts[1] = true;
        } else if (parts[1] === 'false') {
          parts[1] = false;
        }
        obj[parts[0]] = parts[1];
      }
    });
    return obj;
  };

  // ----------------------------------------------------------------------------
  static isHTTPS (node) {
    return Config.getFull(node ?? internals.nodeName, 'keyFile') &&
           Config.getFull(node ?? internals.nodeName, 'certFile');
  };

  // ----------------------------------------------------------------------------
  static basePath (node) {
    return Config.getFull(node ?? internals.nodeName, 'webBasePath', '/');
  };

  // ----------------------------------------------------------------------------
  static nodeName () {
    return internals.nodeName;
  };

  // ----------------------------------------------------------------------------
  static hostName () {
    return internals.hostName;
  };

  // ----------------------------------------------------------------------------
  static arkimeWebURL () {
    let webUrl = Config.get('arkimeWebURL', `${Config.hostName()}${Config.basePath()}`);
    if (!webUrl.startsWith('http')) {
      webUrl = Config.isHTTPS() ? `https://${webUrl}` : `http://${webUrl}`;
    }
    return webUrl;
  };

  // ----------------------------------------------------------------------------
  static keys (section) {
    return Object.keys(ArkimeConfig.getSection(section) ?? {});
  };

  // ----------------------------------------------------------------------------
  static headers (section) {
    const csection = ArkimeConfig.getSection(section);
    if (csection === undefined) { return []; }
    const keys = Object.keys(csection);
    if (!keys) { return []; }
    const headers = Object.keys(csection).map((key) => {
      const obj = { name: key };
      csection[key].split(';').forEach((element) => {
        const i = element.indexOf(':');
        if (i === -1) {
          return;
        }

        const parts = [element.slice(0, i), element.slice(i + 1)];
        if (parts[1] === 'true') {
          parts[1] = true;
        } else if (parts[1] === 'false') {
          parts[1] = false;
        }
        obj[parts[0]] = parts[1];
      });
      return obj;
    });

    return headers;
  };

  // ----------------------------------------------------------------------------
  static configMap (section, dSection, d) {
    const data = ArkimeConfig.getSection(section) ?? ArkimeConfig.getSection(dSection) ?? d;
    if (data === undefined) { return {}; }
    const keys = Object.keys(data);
    if (!keys) { return {}; }
    const map = {};
    keys.forEach((key) => {
      const obj = {};
      data[key].split(';').forEach((element) => {
        const i = element.indexOf(':');
        if (i === -1) {
          return;
        }

        const parts = [element.slice(0, i), element.slice(i + 1)];
        if (parts[1] === 'true') {
          parts[1] = true;
        } else if (parts[1] === 'false') {
          parts[1] = false;
        }
        obj[parts[0]] = parts[1];
      });
      map[key] = obj;
    });

    return map;
  };

  /// ///////////////////////////////////////////////////////////////////////////////
  // Fields
  /// ///////////////////////////////////////////////////////////////////////////////
  static getFields () {
    return internals.fields;
  };

  // ----------------------------------------------------------------------------
  static getFieldsMap () {
    return internals.fieldsMap;
  };

  // ----------------------------------------------------------------------------
  static getDBFieldsMap () {
    return internals.dbFieldsMap;
  };

  // ----------------------------------------------------------------------------
  static getDBField (field, property) {
    if (internals.dbFieldsMap[field] === undefined) { return undefined; }
    if (property === undefined) { return internals.dbFieldsMap[field]; }
    return internals.dbFieldsMap[field][property];
  };

  // ----------------------------------------------------------------------------
  static getCategories () {
    return internals.categories;
  };

  // ----------------------------------------------------------------------------
  static loadFields (data) {
    internals.fields = [];
    internals.fieldsMap = {};
    internals.dbFieldsMap = {};
    internals.categories = {};
    data.forEach((field) => {
      const source = field._source;
      source.exp = field._id;

      // Add some transforms
      if (!source.transform) {
        if (source.exp === 'http.uri' || source.exp === 'http.uri.tokens') {
          source.transform = 'removeProtocol';
        }
        if (source.exp === 'host' || source.exp.startsWith('host.')) {
          source.transform = 'removeProtocolAndURI';
        }
      }

      internals.fieldsMap[field._id] = source;
      internals.dbFieldsMap[source.dbField] = source;
      if (source.dbField2 !== undefined) {
        internals.dbFieldsMap[source.dbField2] = source;
      }
      if (source.fieldECS !== undefined) {
        internals.dbFieldsMap[source.fieldECS] = source;
        internals.fieldsMap[source.fieldECS] = source;
      }
      internals.fields.push(source);
      if (!internals.categories[source.group]) {
        internals.categories[source.group] = [];
      }
      internals.categories[source.group].push(source);
      (source.aliases || []).forEach((alias) => {
        internals.fieldsMap[alias] = source;
      });
    });

    function sortFunc (a, b) {
      return a.exp.localeCompare(b.exp);
    }

    for (const cat in internals.categories) {
      internals.categories[cat] = internals.categories[cat].sort(sortFunc);
    }
  };

  // ----------------------------------------------------------------------------
  static #loadedCbs = [];
  static loaded (cb) {
    if (Config.#loadedCbs === undefined) {
      cb(); // Loaded already, call right away
    } else {
      Config.#loadedCbs.push(cb);
    }
  }

  /// ///////////////////////////////////////////////////////////////////////////////
  // Initialize Auth
  /// ///////////////////////////////////////////////////////////////////////////////

  static async initialize () {
    await ArkimeConfig.initialize({ defaultConfigFile: `${version.config_prefix}/etc/wiseService.ini` });

    // Make sure for everything put cont3xt we have a default section
    if (internals.nodeName !== 'cont3xt' && ArkimeConfig.getSection('default') === undefined) {
      console.log('ERROR - [default] section missing from', ArkimeConfig.configFile);
      process.exit(1);
    }

    ArkimeConfig.loadIncludes(Config.get('includes'));

    const loadedCbs = Config.#loadedCbs;
    Config.#loadedCbs = undefined; // Mark as loaded
    for (const cb of loadedCbs) {
      cb();
    }

    if (Config.debug === 0) {
      ArkimeConfig.debug = parseInt(Config.get('debug', 0));
      if (Config.debug) {
        console.log('Debug Level', Config.debug);
      }
    }

    Auth.initialize({
      mode: Config.get('authMode'),
      userNameHeader: Config.get('userNameHeader'),
      debug: Config.debug,
      basePath: Config.basePath(),
      passwordSecret: Config.getFull(internals.nodeName === 'cont3xt' ? 'cont3xt' : 'default', 'passwordSecret', 'password'),
      passwordSecretSection: internals.nodeName === 'cont3xt' ? 'cont3xt' : 'default',
      serverSecret: Config.getFull('default', 'serverSecret'),
      requiredAuthHeader: Config.get('requiredAuthHeader'),
      requiredAuthHeaderVal: Config.get('requiredAuthHeaderVal'),
      userAutoCreateTmpl: Config.get('userAutoCreateTmpl'),
      userAuthIps: Config.get('userAuthIps'),
      s2s: true,
      s2sRegressionTests: !!Config.get('s2sRegressionTests'),
      caTrustFile: Config.getFull(internals.nodeName, 'caTrustFile'),
      authConfig: {
        httpRealm: Config.get('httpRealm', 'Moloch'),
        userIdField: Config.get('authUserIdField'),
        discoverURL: Config.get('authDiscoverURL'),
        clientId: Config.get('authClientId'),
        clientSecret: Config.get('authClientSecret'),
        redirectURIs: Config.get('authRedirectURIs'),
        trustProxy: Config.get('authTrustProxy')
      }
    });
  }
}

Config.processArgs();

module.exports = Config;
