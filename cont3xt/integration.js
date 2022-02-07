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
    options.integrationsPath = options.integrationsPath ?? path.join(__dirname, '/integrations/');

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

    integration.cacheable = integration.cacheable ?? true;
    integration.noStats = integration.noStats ?? false;
    integration.order = integration.order ?? 10000;

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
      return 'url';
    }

    if (str.match(/^[A-Fa-f0-9]{32}$/) || str.match(/^[A-Fa-f0-9]{40}$/) || str.match(/^[A-Fa-f0-9]{64}$/)) {
      return 'hash';
    }

    // https://www.oreilly.com/library/view/regular-expressions-cookbook/9781449327453/ch08s15.html
    if (str.match(/^([a-z0-9]+(-[a-z0-9]+)*\.)+[a-z]{2,}$/)) {
      return 'domain';
    }

    // Not sure
    return 'text';
  }

  /**
   * List out all the integrations. Integrations without any itypes are skipped.
   * doable - as a user will this integration execute
   * cacheTimeout - how long results will be cached, -1 not cached
   * icon - relative url to icon
   * card - information on how to display the card
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
        doable: doable,
        cacheTimeout: integration.cacheable ? integration.cacheTimeout : -1,
        cachePolicy: integration.cachePolicy,
        icon: integration.icon,
        card: card,
        order: order
      };
    }

    return res.send({ success: true, integrations: results });
  }

  static async runIntegrationsList (shared, query, itype, integrations) {
    shared.total += integrations.length;
    if (Integration.debug > 0) {
      console.log('RUNNING', itype, query, integrations.map(integration => integration.name));
    }

    const writeOne = (integration, response) => {
      if (integration.addMoreIntegrations) {
        integration.addMoreIntegrations(itype, response, (moreQuery, moreIType) => {
          Integration.runIntegrationsList(shared, moreQuery, moreIType, Integration.integrations[moreIType]);
        });
      }

      if (response === Integration.NoResult) {
        shared.res.write(JSON.stringify({ sent: shared.sent, total: shared.total, name: integration.name, itype: itype, query: query, data: { _cont3xt: { createTime: Date.now() } } }));
      } else {
        shared.res.write(JSON.stringify({ sent: shared.sent, total: shared.total, name: integration.name, itype: itype, query: query, data: response }));
      }
      shared.res.write(',\n');
    };

    const writeDone = () => {
      if (shared.sent === shared.total) {
        shared.res.write(JSON.stringify({ finished: true }));
        shared.res.end(']\n');
      }
    };

    const keys = shared.user.getCont3xtKeys();

    let normalizedQuery = query;
    if (itype === 'ip') {
      normalizedQuery = ipaddr.parse(query).toNormalizedString();
      // I'm sure there is some function to do this with ipv6 but I couldn't find it
      if (normalizedQuery.includes(':')) {
        normalizedQuery = normalizedQuery.split(':').map(x => x.padStart(4, '0')).join(':');
      }
    }

    for (const integration of integrations) {
      // Can disable a integration per user
      const disabled = keys?.[integration.name]?.disabled;
      if (disabled === true || disabled === 'true') {
        shared.total--;
        if (Integration.debug > 1) {
          console.log('DISABLED', integration.name);
        }
        writeDone();
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
        writeDone();
        continue;
      }

      stats.total++;
      istats.total++;

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
            writeOne(integration, response);
            writeDone();
            continue;
          }
        }
      }

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
            response._cont3xt.createTime = Date.now();
            writeOne(integration, response);
            if (Integration.cache && integration.cacheable) {
              Integration.cache.set(cacheKey, response);
            }
          } else {
            // console.log('ALW null', integration.name, cacheKey);
          }
          writeDone();
        })
        .catch(err => {
          console.log(integration.name, itype, query, err);
          shared.sent++;
          stats.directError++;
          istats.directError++;
          shared.res.write(JSON.stringify({ sent: shared.sent, total: shared.total, name: integration.name, itype: itype, query: query, failed: true }));
          shared.res.write(',\n');
        });
    }
  }

  /**
   * The search api to go against integrations
   *
   * body.query String to actually query
   * body.doIntegrations Array of integration names to skip
   * body.skipCache Don't use the cache
   */
  static async apiSearch (req, res, next) {
    if (!req.body.query) {
      return res.send({ success: false, text: 'Missing query' });
    }

    if (req.body.doIntegrations && !Array.isArray(req.body.doIntegrations)) {
      return res.send({ success: false, text: 'doIntegrations bad format' });
    }

    const query = req.body.query.trim();

    const itype = Integration.classify(query);

    const integrations = Integration.integrations[itype];
    const shared = {
      skipCache: !!req.body.skipCache,
      doIntegrations: req.body.doIntegrations,
      user: req.user,
      res: res,
      sent: 0,
      total: 0 // runIntegrationsList will fix
    };
    res.write('[\n');
    res.write(JSON.stringify({ success: true, itype: itype, sent: shared.sent, total: integrations.length, text: 'more to follow' }));
    res.write(',\n');

    Integration.runIntegrationsList(shared, query, itype, integrations);
    if (itype === 'email') {
      const dquery = query.slice(query.indexOf('@') + 1);
      Integration.runIntegrationsList(shared, dquery, 'domain', Integration.integrations.domain);
    } else if (itype === 'url') {
      const url = new URL(query);
      if (Integration.classify(url.hostname) === 'ip') {
        Integration.runIntegrationsList(shared, url.hostname, 'ip', Integration.integrations.ip);
      } else {
        const equery = extractDomain(query, { tld: true });
        Integration.runIntegrationsList(shared, equery, 'domain', Integration.integrations.domain);
      }
    }
  }

  /**
   * The search api to go against single itype/integration
   */
  static async apiSingleSearch (req, res, next) {
    if (!req.body.query) {
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
   * Return all the settings and current values that a user can set about
   * Intergrations so a Setting UI can be built on the fly.
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
        values: values,
        globalConfiged: globalConfiged,
        homePage: integration.homePage
      };
    }
    res.send({ success: true, settings: result });
  }

  // TODO ECR cont3xt_views
  static async apiGetViews (req, res, next) {
    res.send({ success: true, views: [] });
  }

  // TODO ECR
  static async apiCreateView (req, res, next) {
    res.send({ success: true });
  }

  // TODO ECR
  static async apiUpdateView (req, res, next) {
    res.send({ success: true });
  }

  // TODO ECR
  static async apiDeleteView (req, res, next) {
    res.send({ success: true });
  }

  /**
   * Return all the settings and current values that a user can set about
   * Intergrations so a Setting UI can be built on the fly.
   */
  static async apiPutSettings (req, res, next) {
    if (!req.body?.settings) {
      res.send({ success: false, text: 'Missing settings' });
    }
    req.user.setCont3xtKeys(req.body.settings);
    res.send({ success: true, text: 'Saved' });
  }

  /**
   * Get stats about integrations
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

  /**
   * Return a config value by first check the user, then the interation name section.
   */
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
