/******************************************************************************/
/* integration.js  -- Integrations wrapper
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

const ArkimeUtil = require('../common/arkimeUtil');
const glob = require('glob');
const path = require('path');
const extractDomain = require('extract-domain');
const ipaddr = require('ipaddr.js');
const Audit = require('./audit');

const itypeStats = {};

class Integration {
  static NoResult = Symbol('NoResult');

  static debug = 0;
  static cache;
  static getConfig;
  static cont3xtStartTime = Date.now();
  static integrationsByName = {};
  static integrations = {
    all: [],
    ip: [],
    domain: [],
    phone: [],
    email: [],
    hash: [],
    url: [],
    text: []
  };

  static initialize (options) {
    Integration.debug = options.debug ?? 0;
    Integration.cache = options.cache;
    Integration.getConfig = options.getConfig;
    options.integrationsPath ??= path.join(__dirname, '/integrations/');

    glob(options.integrationsPath + '*/index.js', (err, files) => {
      files.forEach((file) => {
        require(file);
      });
    });

    if (Integration.debug > 1) {
      setTimeout(() => {
        const sorted = Integration.integrations.all.sort((a, b) => { return a.order - b.order; });
        console.log('ORDER:');
        for (const integration of sorted) {
          if (integration.card) {
            console.log(`${integration.name}: ${integration.order}`);
          }
        }
      }, 1000);
    }
  }

  static register (integration) {
    if (typeof (integration.name) !== 'string') {
      console.log('Missing .name', integration);
      return;
    }

    if (typeof (integration.itypes) !== 'object') {
      console.log('Missing .itypes object', integration);
      return;
    }

    // Can disable a integration globally
    const disabled = integration.getConfig('disabled', false);
    if (disabled === true || disabled === 'true') {
      console.log(integration.name, 'disabled');
      return;
    }

    integration.cacheable ??= true;
    integration.noStats ??= false;
    integration.order ??= 10000;

    integration.stats = {
      total: 0,
      cacheLookup: 0,
      cacheFound: 0,
      cacheGood: 0,
      cacheRecentAvgMS: 0.0,
      directLookup: 0,
      directFound: 0,
      directGood: 0,
      directError: 0,
      directRecentAvgMS: 0.0
    };

    // cacheTime order we check:
    //   cacheTimeout in integration section
    //   cacheTimeout in cont3xt section
    //   cacheTimeout in integration code
    //   60 minutes
    integration.cacheTimeout = ArkimeUtil.parseTimeStr(integration.getConfig('cacheTimeout', Integration.getConfig('cont3xt', 'cacheTimeout', integration.cacheTimeout ?? '1h'))) * 1000;

    // cachePolicy
    integration.cachePolicy = integration.getConfig('cachePolicy', Integration.getConfig('cont3xt', 'cachePolicy', integration.cachePolicy ?? 'shared'));
    switch (integration.cachePolicy) {
    case 'none':
      integration.cacheable = false;
      integration.sharedCache = false;
      break;
    case 'user':
      integration.sharedCache = false;
      break;
    case 'shared':
      integration.sharedCache = true;
      break;
    default:
      console.log('Unknown cache policy', integration);
      return;
    }

    if (Integration.debug > 0) {
      console.log(`REGISTER ${integration.name} cacheTimeout:${integration.cacheTimeout / 1000}s cacheable:${integration.cacheable} sharedCache:${integration.sharedCache} order:${integration.order} itypes:${Object.keys(integration.itypes)}`);
    }

    integration.normalizeCard();
    integration.normalizeTidbits();
    // console.log(integration.name, JSON.stringify(integration.card, false, 2));

    Integration.integrationsByName[integration.name] = integration;
    Integration.integrations.all.push(integration);
    for (const itype in integration.itypes) {
      Integration.integrations[itype].push(integration);
    }
  }

  static classify (str) {
    if (str.match(/^(\d{3})[-. ]?(\d{3})[-. ]?(\d{4})$/)) {
      return 'phone';
    }

    // https://www.oreilly.com/library/view/regular-expressions-cookbook/9780596802837/ch07s16.html
    if (str.match(/^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$/)) {
      return 'ip'; // v4
    }

    // https://stackoverflow.com/questions/53497/regular-expression-that-matches-valid-ipv6-addresses
    if (str.match(/^(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:)|fe80:(:[0-9a-fA-F]{0,4}){0,4}%[0-9a-zA-Z]{1,}|::(ffff(:0{1,4}){0,1}:){0,1}((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])|([0-9a-fA-F]{1,4}:){1,4}:((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9]))$/)) {
      return 'ip'; // v6
    }

    // https://emailregex.com/
    // eslint-disable-next-line no-useless-escape
    if (str.match(/^(([^<>()\[\]\\.,;:\s@"]+(\.[^<>()\[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/)) {
      return 'email';
    }

    // https://urlregex.com/
    // eslint-disable-next-line no-useless-escape
    if (str.match(/https?:\/\//) && str.match(/((([A-Za-z]{3,9}:(?:\/\/)?)(?:[\-;:&=\+\$,\w]+@)?[A-Za-z0-9\.\-]+|(?:www\.|[\-;:&=\+\$,\w]+@)[A-Za-z0-9\.\-]+)((?:\/[\+~%\/\.\w\-_]*)?\??(?:[\-\+=&;%@\.\w_]*)#?(?:[\.\!\/\\\w]*))?)/)) {
      try {
        // Make sure we can construct a proper URL-object using this string
        // eslint-disable-next-line no-unused-vars
        const url = new URL(str);
        return 'url';
      } catch (e) {
        // This looked like a URL but could not be parsed as such. Continue testing below.
      }
    }

    if (str.match(/^[A-Fa-f0-9]{32}$/) || str.match(/^[A-Fa-f0-9]{40}$/) || str.match(/^[A-Fa-f0-9]{64}$/)) {
      return 'hash';
    }

    // https://www.oreilly.com/library/view/regular-expressions-cookbook/9781449327453/ch08s15.html + uppercase
    if (str.match(/^([a-zA-Z0-9]+(-[a-zA-Z0-9]+)*\.)+[a-zA-Z]{2,}$/)) {
      return 'domain';
    }

    // Not sure
    return 'text';
  }

  /**
   * An Integration field definition object
   *
   * The specifics on how to display a field
   * @typedef IntegrationFieldDef
   * @type {object}
   * @param {string} label - The field label to display to a user
   * @param {string} path - The path (it can have dots) to the data for field, if not set the field is the same as name. For table type this will be the path to the array.
   * @param {IntegrationField[]} fields - Used with table data types, the list of fields to display in the table
   * @param {boolean} defang - When true defang the string, change http to hXXp and change . to [.]
   * @param {boolean} pivot - When set this field should be added to action menu for table entry that you can replace query with
   * @param {string} join - Used with array data types, display with value as the separator on one line (example single: ', ')
   * @param {string} defaultSortField - Used with table data types, sorts the table by this field initially
   * @param {string} defaultSortDirection="desc" - Used with table data types if defaultSortField is also set, sorts the table in this direction ('asc' or 'desc')
   */

  /**
    * An Integration field type string
    *
    * The data type of the field data
    * @typedef IntegrationFieldType
    * @type {string}
    * @param {string} type="string" - The type of data displayed in the field
    *                                 string - obvious
    *                                 url - a url that should be made clickable
    *                                 table - there will be a fields element
    *                                 array - the field var will point to an array, display 1 per line unless join set
    *                                 date - a date value
    *                                 ms - a ms time value
    *                                 seconds - a second time value
    *                                 json - just display raw json, call in JSON.stringify(blah, false, 2)
    */

  /**
   * An Integration tidbits object
   *
   * Information for creating and ordering tidbits
   * @typedef IntegrationTidbitContainer
   * @type {object}
   * @param {number|undefined} order - a default order to apply to all contained tidbits
   * @param {IntegrationTidbit[]} fields - the objects that define individual tidbit displays
   */

  /**
   * An Integration tidbit object
   *
   * Information about how to display a field from an Integration's data to the primary indicator-tree display.
   * @typedef IntegrationTidbit
   * @type {object}
   * @param {string|undefined} label - The name of the field. If given, tidbit is displayed as key-value pair at bottom
   * @param {IntegrationFieldType} type - The type of data displayed in the field, default 'string'
   * @param {IntegrationFieldDef} field - path to data
   * @param {IntegrationFieldDef|undefined} fieldRoot - path to element data from data root
   * @param {string} display - how to display value in UI, default 'badge'
   * @param {string|undefined} template - pseudo template-string applied to value before postProcess
   * @param {string[]|string|undefined} postProcess - named filter(s) to pass value into
   * @param {string|undefined} tooltip - value used as tooltip
   * @param {string|undefined} tooltipTemplate - pseudo template-string filled with value & data for use in tooltip
   * @param {number} order - number by which tidbits are sorted (ascending order), default 0
   * @param {number|undefined} precedence - the higher, the more preferred among those with the same purpose
   * @param {string|undefined} purpose - when multiple valid tidbits have the same purpose,
   *                                     only the one with the highest precedence will be kept
   */

  /**
   * An Integration field object
   *
   * Information about how to display a field within an Integration's data.
   * @typedef IntegrationField
   * @type {object}
   * @param {string} name - The name of the field
   * @param {IntegrationFieldType} type - The type of data displayed in the field
   * @param {IntegrationFieldDef} field - If not "name" and "type" it's an object describing the data
   */

  /**
   * An Integration card object
   *
   * Information about how to display the integration's data.
   * @typedef IntegrationCard
   * @type {object}
   * @param {string} title - The title of the card to display in the UI
   * @param {IntegrationField[]} fields - An array of field objects to outline how to display data for each field within the integration's data
   */

  /**
   * An Integration object
   *
   * Integrations are the configured data sources for Cont3xt.
   * @typedef Integration
   * @type {object}
   * @param {string} cachePolicy - Who can access the cached results of this integration's data ("shared")
   * @param {number} cacheTimeout - How long results will be cached, -1 not cached
   * @param {boolean} doable - Whether the user has access to execute this integration
   * @param {string} icon - The relative url to the integrations icon
   * @param {number} order - The order in which this integration displays in the UI
   * @param {IntegrationCard} card - Information on how to display the integration's data
   * @param {IntegrationTidbitContainer} tidbits - Information on how to pull specialized fields into indicator-tree UI
   */

  /**
   * GET - /api/integration
   *
   * List out all the integrations. Integrations without any itypes are skipped.
   * @name /integration
   * @returns {Integrations[]} integrations - A map of integrations that the logged in user has configured
   * @returns {boolean} success - True if the request was successful, false otherwise
   */
  static async apiList (req, res, next) {
    const results = {};
    const integrations = Integration.integrations.all;

    const keys = req.user.getCont3xtKeys();

    for (const integration of integrations) {
      if (Object.keys(integration.itypes).length === 0) { continue; }

      let doable = true;

      // First check if user has disabled integration
      const disabled = keys?.[integration.name]?.disabled;
      if (disabled === true || disabled === 'true') {
        doable = false;
      }

      // If still doable check to see if all settings set
      if (doable && integration.settings) {
        for (const setting in integration.settings) {
          if (integration.settings[setting].required && !integration.getUserConfig(req.user, integration.name, setting)) {
            doable = false;
            break;
          }
        }
      }

      // Gather settings to be made accessible from the UI
      const uiSettings = Object.fromEntries(
        Object.entries(keys?.[integration.name] ?? {})
          .filter(([key, _]) => integration?.settings?.[key]?.uiSetting)
      );

      // User can override card display
      let card = integration.card;
      const cardstr = integration.getUserConfig(req.user, integration.name, 'card');
      if (cardstr) {
        card = JSON.parse(cardstr);
        // Should normalize here
      }

      // User can override order
      const order = integration.getUserConfig(req.user, integration.name, 'order', integration.order);

      results[integration.name] = {
        doable,
        cacheTimeout: integration.cacheable ? integration.cacheTimeout : -1,
        cachePolicy: integration.cachePolicy,
        icon: integration.icon,
        card,
        order,
        tidbits: integration.tidbits?.fields || [],
        uiSettings: uiSettings || {}
      };
    }

    return res.send({ success: true, integrations: results });
  }

  static async runIntegrationsList (shared, query, itype, integrations, onFinish) {
    shared.total += integrations.length;

    if (Integration.debug > 0) {
      console.log('RUNNING', itype, query, integrations.map(integration => integration.name));
    }

    const writeOne = (integration, response) => {
      if (integration.addMoreIntegrations) {
        integration.addMoreIntegrations(itype, response, (moreQuery, moreIType) => {
          Integration.runIntegrationsList(shared, moreQuery, moreIType, Integration.integrations[moreIType], onFinish);
        });
      }

      if (response === Integration.NoResult) {
        shared.res.write(JSON.stringify({ sent: shared.sent, total: shared.total, name: integration.name, itype, query, data: { _cont3xt: { createTime: Date.now() } } }));
      } else {
        shared.res.write(JSON.stringify({ sent: shared.sent, total: shared.total, name: integration.name, itype, query, data: response }));
      }
      shared.res.write(',\n');
    };

    const checkWriteDone = () => {
      // setImmediate to ensure that any pending integration lists have a chance to start (and contribute to total)
      setImmediate(() => {
        if (shared.sent === shared.total && shared.finished !== true) {
          shared.finished = true;
          onFinish();
        }
      });
    };

    const keys = shared.user.getCont3xtKeys();

    let normalizedQuery = query;
    if (itype === 'ip') {
      try {
        normalizedQuery = ipaddr.parse(query).toNormalizedString();
      } catch (e) {
        console.log(`WARNING - ${query} is not really an ip`);
        shared.total -= integrations.length;
        return;
      }
      // I'm sure there is some function to do this with ipv6 but I couldn't find it
      if (normalizedQuery.includes(':')) {
        normalizedQuery = normalizedQuery.split(':').map(x => x.padStart(4, '0')).join(':');
      }
    }

    // must finish in case of no possible integrations (text)
    if (integrations.length === 0) {
      checkWriteDone();
    }

    for (const integration of integrations) {
      // Can disable a integration per user
      const disabled = keys?.[integration.name]?.disabled;
      if (disabled === true || disabled === 'true') {
        shared.total--;
        if (Integration.debug > 1) {
          console.log('DISABLED', integration.name);
        }
        checkWriteDone();
        continue;
      }

      const cacheKey = `${integration.sharedCache ? 'shared' : shared.user.userId}-${integration.name}-${itype}-${normalizedQuery}`;
      const stats = integration.stats;
      if (itypeStats[itype] === undefined) {
        makeITypeStats(itype);
      }
      const istats = itypeStats[itype];

      if (shared.doIntegrations && !shared.doIntegrations.includes(integration.name)) {
        shared.total--;
        checkWriteDone();
        continue;
      }

      stats.total++;
      istats.total++;

      // if available, first try cache
      if (!shared.skipCache && Integration.cache && integration.cacheable) {
        stats.cacheLookup++;
        istats.cacheLookup++;
        const cStartTime = Date.now();
        const response = await Integration.cache.get(cacheKey);
        updateTime(stats, istats, Date.now() - cStartTime, 'cache');
        if (response) {
          stats.cacheFound++;
          istats.cacheFound++;
          if (!response._cont3xt) { response._cont3xt = {}; }
          if (Date.now() - response._cont3xt.createTime < integration.cacheTimeout) {
            stats.cacheGood++;
            istats.cacheGood++;
            shared.sent++;
            shared.resultCount += response._cont3xt.count ?? 0;
            writeOne(integration, response);
            checkWriteDone();
            continue;
          }
        }
      }

      // if cache fails, fetch
      stats.directLookup++;
      istats.directLookup++;
      const dStartTime = Date.now();
      integration[integration.itypes[itype]](shared.user, normalizedQuery)
        .then(response => {
          updateTime(stats, istats, Date.now() - dStartTime, 'direct');
          stats.directFound++;
          istats.directFound++;
          shared.sent++;
          if (response === Integration.NoResult) {
            writeOne(integration, response);
          } else if (response) {
            stats.directGood++;
            istats.directGood++;
            if (!response._cont3xt) { response._cont3xt = {}; }
            shared.resultCount += response._cont3xt.count ?? 0;
            response._cont3xt.createTime = Date.now();
            writeOne(integration, response);
            if (Integration.cache && integration.cacheable) {
              Integration.cache.set(cacheKey, response);
            }
          } else {
            // console.log('ALW null', integration.name, cacheKey);
          }
          checkWriteDone();
        })
        .catch(err => {
          console.log(integration.name, itype, query, err);
          shared.sent++;
          stats.directError++;
          istats.directError++;
          shared.res.write(JSON.stringify({ sent: shared.sent, total: shared.total, name: integration.name, itype, query, failed: true }));
          shared.res.write(',\n');
        });
    }
  }

  /**
   * The classification of the search string
   *
   * @typedef Itype
   * @type {string}
   * @param {string} itype="text" - The type of the search
   *                                ip, domain, url, email, phone, hashes, or text
   */

  /**
   * Integration Data Chunk object
   *
   * An chunk of data returned from searching integrations
   * @typedef IntegrationChunk
   * @type {object}
   * @param {boolean} success - Whether the search was successful (sent in first chunk only)
   * @param {Itype} itype - The type of the search
   * @param {number} total - The total number of integrations to query
   * @param {number} sent - The number of integration results that have completed and been sent to the client
   * @param {string} name - The name of the integration result within the chunk
   * @param {string} query - The query that was run against the integration to retrieve data
   * @param {object} data - The data from the integration query. This varies based upon the integration. The <a href="#integrationcard-type">IntegrationCard</a> describes how to present this data to the user.
   */

  /**
   * POST - /api/integration/search
   *
   * Fetches integration data
   * @name /integration/search
   * @param {string} query - The string to query integrations
   * @param {string[]} doIntegrations - A list of integration names to query
   * @param {boolean} skipCache - Ignore any cached data and query all integrations again
   * @param {string[]} tags - Tags applied at the time of search
   * @param {string | undefined} viewId - The ID of the view at the time of search (if any)
   * @returns {IntegrationChunk[]} results - An array data chunks with the data
   */
  static async apiSearch (req, res, next) {
    if (typeof req.body.query !== 'string') {
      return res.send({ success: false, text: 'Missing query' });
    }

    if (req.body.tags !== undefined) {
      if (!Array.isArray(req.body.tags)) {
        return res.send({ success: false, text: 'tags must be an array when present' });
      }
      if (req.body.tags.some(t => typeof t !== 'string')) {
        return res.send({ success: false, text: 'every tag must be a string' });
      }
    }

    if (req.body.doIntegrations) {
      if (!Array.isArray(req.body.doIntegrations)) {
        return res.send({ success: false, text: 'doIntegrations must be an array when present' });
      }
      if (req.body.doIntegrations.some(i => typeof i !== 'string')) {
        return res.send({ success: false, text: 'every doIntegration must be a string' });
      }
    }

    if (req.body.viewId !== undefined && typeof req.body.viewId === 'string') {
      return res.send({ success: false, text: 'viewId must be a string when present' });
    }

    const query = req.body.query.trim();

    const itype = Integration.classify(query);

    const integrations = Integration.integrations[itype];
    const shared = {
      skipCache: !!req.body.skipCache,
      doIntegrations: req.body.doIntegrations,
      user: req.user,
      res,
      sent: 0,
      total: 0, // runIntegrationsList will fix
      resultCount: 0 // sum of _cont3xt.count from results
    };
    res.write('[\n');
    res.write(JSON.stringify({ success: true, itype, query, sent: shared.sent, total: integrations.length, text: 'more to follow' }));
    res.write(',\n');

    const issuedAt = Date.now();
    // end data write and create audit log when finished
    const finishWrite = () => {
      res.write(JSON.stringify({ finished: true, resultCount: shared.resultCount }));
      res.end(']\n');

      Audit.create({
        userId: req.user.userId,
        indicator: query,
        iType: itype,
        tags: req.body.tags ?? [],
        viewId: req.body.viewId,
        issuedAt,
        took: Date.now() - issuedAt,
        resultCount: shared.resultCount
      }).catch((err) => {
        console.log('ERROR - creating audit log.', err);
      });
    };

    Integration.runIntegrationsList(shared, query, itype, integrations, finishWrite);
    if (itype === 'email') {
      const dquery = query.slice(query.indexOf('@') + 1);
      Integration.runIntegrationsList(shared, dquery, 'domain', Integration.integrations.domain, finishWrite);
    } else if (itype === 'url') {
      const url = new URL(query);
      if (Integration.classify(url.hostname) === 'ip') {
        Integration.runIntegrationsList(shared, url.hostname, 'ip', Integration.integrations.ip, finishWrite);
      } else {
        const equery = extractDomain(query, { tld: true });
        Integration.runIntegrationsList(shared, equery, 'domain', Integration.integrations.domain, finishWrite);
      }
    }
  }

  /**
   * POST - /api/integration/:itype/:integration/search
   *
   * Fetches integration data about a single itype/integration
   * @name /integration/:itype/:integration/search
   * @param {string} query - The string to query the integration
   * @returns {boolean} success - True if the request was successful, false otherwise
   * @returns {string} _query - The query that was run against the integration to retrieve data
   * @returns {object} data - The data from the integration query. This varies based upon the integration. The IntegrationCard describes how to present this data to the user.
   */
  static async apiSingleSearch (req, res, next) {
    if (typeof req.body.query !== 'string') {
      return res.send({ success: false, text: 'Missing query' });
    }

    const itype = req.params.itype;
    const query = req.body.query.trim();

    const integration = Integration.integrationsByName[req.params.integration];

    if (integration === undefined || integration.itypes[itype] === undefined) {
      return res.send({ success: false, text: `integration ${itype} ${req.params.integration} not found` });
    }

    const keys = req.user.getCont3xtKeys();
    const disabled = keys?.[integration.name]?.disabled;
    if (disabled === true || disabled === 'true') {
      return res.send({ success: false, text: `integration ${itype} ${req.params.integration} disabled` });
    }

    const stats = integration.stats;
    if (itypeStats[itype] === undefined) {
      makeITypeStats(itype);
    }
    const istats = itypeStats[itype];
    stats.total++;
    istats.total++;

    stats.directLookup++;
    istats.directLookup++;
    const dStartTime = Date.now();
    integration[integration.itypes[itype]](req.user, query)
      .then(response => {
        updateTime(stats, istats, Date.now() - dStartTime, 'direct');
        stats.directFound++;
        istats.directFound++;
        if (response === Integration.NoResult) {
          res.send({ success: true, data: { _cont3xt: { createTime: Date.now() } }, _query: query });
        } else if (response) {
          stats.directGood++;
          istats.directGood++;
          if (!response._cont3xt) { response._cont3xt = {}; }
          response._cont3xt.createTime = Date.now();
          res.send({ success: true, data: response, _query: query });
          if (response && Integration.cache && integration.cacheable) {
            const cacheKey = `${integration.sharedCache ? 'shared' : req.user.userId}-${integration.name}-${itype}-${query}`;
            Integration.cache.set(cacheKey, response);
          }
        }
      })
      .catch(err => {
        console.log(integration.name, itype, query, err);
        stats.directError++;
        istats.directError++;
        res.status(500).send({ success: false, _query: query });
      });
  }

  /**
   * Integration Settings object
   *
   * The settings for an integration for the logged in user
   * @typedef IntegrationSetting
   * @type {object}
   * @param {boolean} globalConfiged - Whether integration is configured globally across cont3xt users or by this user (if a user has changed the settings for an integration, this if false)
   * @param {string} homePage - The link to the home page for this integration so a user can learn more
   * @param {object} settings - The setting field definitions for this integration
   * @param {object} values - The values that map to the setting fields for this integration (empty object if not set)
   */

  /**
   * GET - /api/integration/settings
   *
   * Return all the integration settings and current values that a user can set
   * @name /integration/settings
   * @returns {boolean} success - True if the request was successful, false otherwise
   * @returns {IntegrationSetting[]} settings - The settings for each integration for the logged in user
   */
  static async apiGetSettings (req, res, next) {
    const result = {};
    const integrations = Integration.integrations.all;
    const keys = req.user.getCont3xtKeys();
    for (const integration of integrations) {
      if (!integration.settings) { continue; }

      // Any values saved for this user, we don't send global values
      const values = {};
      if (keys?.[integration.name]) {
        const ivalues = keys[integration.name];
        for (const setting in integration.settings) {
          if (ivalues[setting]) {
            values[setting] = ivalues[setting];
          }
        }
      }

      // All the required items are filled out in global config?
      let globalConfiged = true;
      let cnt = 0;
      for (const setting in integration.settings) {
        if (integration.settings[setting].required) {
          cnt++;
          if (Integration.getConfig(integration.name, setting) === undefined) {
            globalConfiged = false;
            break;
          }
        }
      }

      // If there are no required fields then don't say it is global configured
      if (cnt === 0) { globalConfiged = false; }

      result[integration.name] = {
        settings: integration.settings,
        values,
        globalConfiged,
        homePage: integration.homePage
      };
    }
    res.send({ success: true, settings: result });
  }

  /**
   * PUT - /api/integration/settings
   *
   * Updates the integration settings
   * @name /integration/settings
   * @returns {boolean} success - True if the request was successful, false otherwise
   * @returns {IntegrationSetting[]} settings - The integration settings to update for the logged in user
   */
  static async apiPutSettings (req, res, next) {
    if (typeof req.body.settings !== 'object') {
      res.send({ success: false, text: 'Missing settings' });
    }
    req.user.setCont3xtKeys(req.body.settings);
    res.send({ success: true, text: 'Saved' });
  }

  /**
   * Integration Stat object
   *
   * The statistic data for an integration
   * @typedef Stat
   * @type {object}
   * @param {number} cacheFound - The number of entries found in the cache for this integration
   * @param {number} cacheGood - The number of valid entries found in the cache for this integration
   * @param {number} cacheLookup - The number of entries looked up in the cache for this integration
   * @param {number} cacheRecentAvgMS - How long it takes to look up this integration from the cache
   * @param {number} directError - The number of entries queried directly from the integration that failed
   * @param {number} directFound - The number of entries found directly from the integration
   * @param {number} directGood - The number of valid entries queried directly from the integration
   * @param {number} directLookup - The number of entries queried directly from the integration
   * @param {number} directRecentAvgMS - How long it takes to look up directly from the integration
   * @param {number} name - The name of the integration
   * @param {number} total - The number of times the integration was asked for a result
   */

  /**
   * GET - /api/integration/stats
   *
   * Fetches stats about integrations
   * @name /integration/stats
   * @returns {boolean} success - True if the request was successful, false otherwise
   * @returns {number} startTime - The start time of the cont3xt server (the start of the stats data)
   * @returns {Stat[]} stats - The integration stat data
   * @returns {Stat[]} itypeStats - The itype stat data
   */
  static async apiStats (req, res, next) {
    const result = [];
    for (const integration of Integration.integrations.all) {
      if (integration.noStats) { continue; }
      result.push({ ...integration.stats, name: integration.name });
    }
    const iresult = [];
    for (const itype of Object.keys(itypeStats)) {
      iresult.push({ ...itypeStats[itype], name: itype });
    }
    res.send({ success: true, startTime: Integration.cont3xtStartTime, stats: result, itypeStats: iresult });
  }

  getConfig (k, d) {
    return Integration.getConfig(this.name, k, d);
  }

  // Return a config value by first check the user, then the interation name section.
  getUserConfig (user, section, key, d) {
    if (user.cont3xt?.keys) {
      const keys = user.getCont3xtKeys();
      if (keys[section]?.[key]) { return keys[section]?.[key]; }
    }

    return Integration.getConfig(section, key, d);
  }

  userAgent () {
    Integration.getConfig('cont3xt', 'userAgent', 'cont3xt');
  }

  normalizeCardFields (inFields) {
    const outFields = [];
    for (const f of inFields) {
      if (typeof f === 'string') {
        outFields.push({
          label: f,
          path: f.split('.'),
          type: 'string'
        });
        continue;
      }
      if (f.field === undefined) { f.field = f.label; }
      if (typeof f.field === 'string') { f.path = f.field.split('.'); }
      delete f.field;

      if (f.type === undefined) { f.type = 'string'; }

      if (f.type === 'table') {
        f.fields = this.normalizeCardFields(f.fields);
      }

      if (f.type === 'array' || f.type === 'table') { // array-like types
        if (f.filterEmpty === undefined) { f.filterEmpty = true; }

        if (f.fieldRoot !== undefined) {
          f.fieldRootPath = f.fieldRoot.split('.');
          delete f.fieldRoot;
        }
      }

      outFields.push(f);
    }

    return outFields;
  }

  normalizeCard () {
    const card = this.card;
    if (!card) { return; }

    if (!card.title) {
      this.card.title = `${this.name} for %{query}`;
    }
    if (card.fields === undefined) { card.fields = []; }
    card.fields = this.normalizeCardFields(card.fields);
  }

  normalizeTidbits () {
    if (!this.tidbits) { return; }
    if (Array.isArray(this.tidbits)) {
      // if tidbits is just an array, make that into fields.
      this.tidbits = { fields: this.tidbits };
    }
    this.tidbits.order ??= 0;
    this.tidbits.fields = this.tidbits.fields
      ?.map((tidbit, index) => this.normalizeTidbitField(tidbit, index, this.tidbits.order));
  }

  normalizeTidbitField (tidbit, index, containerOrder) {
    const {
      type,
      path: fieldPath,
      field,
      fieldRootPath,
      fieldRoot,
      order,
      tooltip,
      label,
      display,
      postProcess,
      template,
      tooltipTemplate,
      precedence,
      purpose
    } = tidbit;

    return {
      path: fieldPath ?? field?.split('.') ?? [],
      fieldRootPath: fieldRootPath ?? fieldRoot?.split('.') ?? [],
      order: order ?? containerOrder,
      tooltip,
      tooltipTemplate,
      label,
      type: type ?? 'string',
      display: display ?? 'badge',
      template,
      postProcess,
      precedence: precedence ?? Number.MIN_VALUE,
      purpose: purpose ?? '',
      definitionOrder: index
    };
  }
}

// Calculate rolling average over at most 100 items
function updateTime (stats, istats, diff, prefix) {
  const lookup = Math.min(stats[prefix + 'Lookup'], 100);
  stats[prefix + 'RecentAvgMS'] = (stats[prefix + 'RecentAvgMS'] * (lookup - 1) + diff) / lookup;

  const ilookup = Math.min(stats[prefix + 'Lookup'], 100);
  istats[prefix + 'RecentAvgMS'] = (istats[prefix + 'RecentAvgMS'] * (ilookup - 1) + diff) / ilookup;
};

function makeITypeStats (itype) {
  itypeStats[itype] = {
    total: 0,
    cacheLookup: 0,
    cacheFound: 0,
    cacheGood: 0,
    cacheRecentAvgMS: 0.0,
    directLookup: 0,
    directFound: 0,
    directGood: 0,
    directError: 0,
    directRecentAvgMS: 0.0
  };
};

module.exports = Integration;

// Must be at bottom to avoid circular dependency
// const Db = require('./db.js');
