/******************************************************************************/
/*
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const axios = require('axios');
const https = require('https');
const WISESource = require('./wiseSource.js');
const ArkimeUtil = require('../common/arkimeUtil');

// MISP attribute type -> [wise type, index into the '|' split value]
const TYPE_MAP = Object.assign(Object.create(null), {
  'ip-src': [['ip', 0]],
  'ip-dst': [['ip', 0]],
  'ip-src|port': [['ip', 0]],
  'ip-dst|port': [['ip', 0]],
  domain: [['domain', 0]],
  hostname: [['domain', 0]],
  'domain|ip': [['domain', 0], ['ip', 1]],
  'hostname|port': [['domain', 0]],
  email: [['email', 0]],
  'email-src': [['email', 0]],
  'email-dst': [['email', 0]],
  md5: [['md5', 0]],
  'filename|md5': [['md5', 1]],
  sha256: [['sha256', 0]],
  'filename|sha256': [['sha256', 1]],
  url: [['url', 0]],
  'ja3-fingerprint-md5': [['ja3', 0]]
});

const THREAT_LEVELS = new Map([['1', 'high'], ['2', 'medium'], ['3', 'low'], ['4', 'undefined']]);
const WISE_TYPES = ['ip', 'domain', 'email', 'md5', 'sha256', 'url', 'ja3'];

// wise type -> MISP attribute types that can hold it
const TYPES_BY_WISE = {};
WISE_TYPES.forEach((t) => {
  TYPES_BY_WISE[t] = Object.keys(TYPE_MAP).filter((m) => TYPE_MAP[m].some(([wtype]) => wtype === t));
});

class MISPSource extends WISESource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    // api mode lets the wise cache hold results, bulk mode holds everything itself
    super(api, section, { dontCache: api.getConfig(section, 'mode', 'bulk') !== 'api', tagsSetting: true });

    this.mode = api.getConfig(section, 'mode', 'bulk');
    if (this.mode !== 'bulk' && this.mode !== 'api') {
      console.log(this.section, `- ERROR not loading since unknown mode ${this.mode}, must be bulk or api`);
      return;
    }

    this.url = api.getConfig(section, 'url');
    this.key = api.getConfig(section, 'key');
    if (this.url === undefined || this.key === undefined) {
      console.log(this.section, '- ERROR not loading since missing url or key');
      return;
    }
    this.url = this.url.replace(/\/+$/, '');

    this.serverName = api.getConfig(section, 'name', section.substring(5));
    this.last = api.getConfig(section, 'last', this.mode === 'bulk' ? '30d' : undefined);
    this.toIds = api.getConfig(section, 'toIds', true);
    this.published = api.getConfig(section, 'published', true);
    this.enforceWarninglist = api.getConfig(section, 'enforceWarninglist', true);
    this.mispTags = api.getConfig(section, 'mispTags');

    if (api.getConfig(section, 'insecure', false)) {
      this.httpsAgent = new https.Agent({ rejectUnauthorized: false });
    }

    this.serverField = this.api.addField('field:misp.server;db:misp.server;kind:termfield;friendly:Server;help:MISP Server;count:true');
    this.eventIdField = this.api.addField('field:misp.event-id;db:misp.eventId;kind:termfield;friendly:Event Id;help:MISP Event Id;count:true');
    this.eventField = this.api.addField('field:misp.event;db:misp.event;kind:termfield;friendly:Event;help:MISP Event Info;count:true');
    this.threatLevelField = this.api.addField('field:misp.threat-level;db:misp.threatLevel;kind:lotermfield;friendly:Threat Level;help:MISP Event Threat Level;count:true');
    this.categoryField = this.api.addField('field:misp.category;db:misp.category;kind:termfield;friendly:Category;help:MISP Attribute Category;count:true');
    this.orgField = this.api.addField('field:misp.org;db:misp.org;kind:termfield;friendly:Org;help:MISP Event Creator Org;count:true');
    this.tagField = this.api.addField('field:misp.tag;db:misp.tag;kind:termfield;friendly:Tag;help:MISP Event and Attribute Tags;count:true');

    this.api.addView('misp',
      'if (session.misp)\n' +
      '  div.sessionDetailMeta.bold MISP\n' +
      '  dl.sessionDetailMeta\n' +
      "    +arrayList(session.misp, 'server', 'Server', 'misp.server')\n" +
      "    +arrayList(session.misp, 'eventId', 'Event Id', 'misp.event-id')\n" +
      "    +arrayList(session.misp, 'event', 'Event', 'misp.event')\n" +
      "    +arrayList(session.misp, 'threatLevel', 'Threat Level', 'misp.threat-level')\n" +
      "    +arrayList(session.misp, 'category', 'Category', 'misp.category')\n" +
      "    +arrayList(session.misp, 'org', 'Org', 'misp.org')\n" +
      "    +arrayList(session.misp, 'tag', 'Tag', 'misp.tag')\n"
    );

    this.api.addValueAction(`misp-${this.serverName}`, { name: `MISP ${this.serverName} Event`, url: `${this.url}/events/view/%TEXT%`, fields: 'misp.event-id' });

    this.maps = {};
    WISE_TYPES.forEach((t) => { this.maps[t] = new Map(); });

    if (this.mode === 'api') {
      this.maxResults = parseInt(api.getConfig(section, 'maxResults', 50), 10);
      if (!(this.maxResults > 0)) { this.maxResults = 50; }
      this.maxConns = parseInt(api.getConfig(section, 'maxConns', 20), 10);
      if (!(this.maxConns > 0)) { this.maxConns = 20; }
      this.inProgress = 0;
      this.lastErrorLog = 0;

      if (!this.onlyIPs && !api.getConfig(section, 'excludeIPs')) {
        console.log(this.section, '- WARNING api mode without onlyIPs or excludeIPs will query MISP for every ip seen');
      }
    } else {
      this.pageSize = parseInt(api.getConfig(section, 'pageSize', 10000), 10);
      if (!(this.pageSize > 0)) { this.pageSize = 10000; }
      this.maxAttributes = parseInt(api.getConfig(section, 'maxAttributes', 2000000), 10);
      if (!(this.maxAttributes > 0)) { this.maxAttributes = 2000000; }
      let reload = parseInt(api.getConfig(section, 'reload', 60), 10);
      if (Number.isNaN(reload)) { reload = 60; }

      setImmediate(this.load.bind(this));
      if (reload > 0) {
        setInterval(this.load.bind(this), Math.max(reload, 5) * 60 * 1000);
      }
    }

    this.api.addSource(section, this, WISE_TYPES);
  }

  // ----------------------------------------------------------------------------
  #searchBody (types) {
    const body = {
      returnFormat: 'json',
      type: types,
      includeEventTags: true,
      includeContext: true,
      enforceWarninglist: this.enforceWarninglist,
      deleted: false
    };
    if (this.last) { body.last = this.last; }
    if (this.toIds) { body.to_ids = true; }
    if (this.published) { body.published = true; }
    if (this.mispTags) { body.tags = this.mispTags.split(',').map(t => t.trim()); }
    return body;
  }

  // ----------------------------------------------------------------------------
  async #restSearch (body) {
    const result = await axios.post(`${this.url}/attributes/restSearch`, body, {
      headers: {
        Authorization: this.key,
        Accept: 'application/json',
        'Content-Type': 'application/json'
      },
      httpsAgent: this.httpsAgent,
      maxRedirects: 0,
      timeout: this.mode === 'api' ? 30 * 1000 : 5 * 60 * 1000
    });

    const attrs = result.data?.response?.Attribute;
    if (!Array.isArray(attrs)) {
      throw new Error('Unexpected response, no Attribute array');
    }
    return attrs;
  }

  // ----------------------------------------------------------------------------
  async load () {
    if (this.loading) { return; }
    this.loading = true;

    const body = this.#searchBody(Object.keys(TYPE_MAP));
    body.limit = this.pageSize;

    const items = {};
    WISE_TYPES.forEach((t) => { items[t] = new Map(); });

    try {
      let count = 0;
      let fetched = 0;
      for (let page = 1; ; page++) {
        if (fetched >= this.maxAttributes) {
          console.log(this.section, '- WARNING stopped loading at maxAttributes', this.maxAttributes);
          break;
        }
        body.page = page;
        const attrs = await this.#restSearch(body);

        fetched += attrs.length;
        attrs.forEach((a) => {
          if (!ArkimeUtil.isString(a?.value) || !ArkimeUtil.isString(a.type)) { return; }
          const mappings = TYPE_MAP[a.type];
          if (!mappings) { return; }
          const parts = a.type.includes('|') ? a.value.split('|') : [a.value];
          mappings.forEach(([wtype, idx]) => {
            const key = this.#normalize(wtype, parts[idx]);
            if (!key) { return; }
            let info = items[wtype].get(key);
            if (!info) {
              info = MISPSource.#newInfo();
              items[wtype].set(key, info);
            }
            MISPSource.#addInfo(info, a);
          });
          count++;
        });

        if (attrs.length < this.pageSize) { break; }
      }

      const maps = {};
      WISE_TYPES.forEach((t) => {
        maps[t] = new Map();
        items[t].forEach((info, key) => {
          maps[t].set(key, this.#encode(info));
        });
      });
      this.maps = maps;
      console.log(this.section, '- Done Loading', count, 'attributes', this.itemCount(), 'items');
    } catch (err) {
      console.log(this.section, '- Load failed, keeping previous data', err?.response?.status ?? '', err?.response?.data?.message ?? err.message);
    } finally {
      this.loading = false;
    }
  }

  // ----------------------------------------------------------------------------
  async #apiLookup (wtype, value, cb) {
    // MISP treats % as a wildcard and a leading ! as NOT, only do exact matches
    if (value.includes('%') || value.startsWith('!')) {
      return cb(null, WISESource.emptyResult);
    }

    if (this.inProgress >= this.maxConns) {
      return cb('dropped');
    }

    // capture sends urls as host/path, MISP stores them with a scheme
    const values = wtype === 'url' ? [value, `http://${value}`, `https://${value}`] : value;
    const body = this.#searchBody(TYPES_BY_WISE[wtype]);
    body.value = values;
    body.limit = this.maxResults;
    body.page = 1;

    this.inProgress++;
    try {
      const attrs = await this.#restSearch(body);
      const info = MISPSource.#newInfo();
      let found = false;
      attrs.forEach((a) => {
        if (a && typeof a === 'object') {
          MISPSource.#addInfo(info, a);
          found = true;
        }
      });
      cb(null, found ? this.#encode(info) : WISESource.emptyResult);
    } catch (err) {
      // don't flood the log when MISP is down, and don't cache the failure
      if (Date.now() - this.lastErrorLog > 60000) {
        this.lastErrorLog = Date.now();
        console.log(this.section, '- Lookup failed', err?.response?.status ?? '', err?.response?.data?.message ?? err.message);
      }
      cb('dropped');
    } finally {
      this.inProgress--;
    }
  }

  // ----------------------------------------------------------------------------
  #normalize (wtype, value) {
    if (!value) { return undefined; }
    value = value.trim();
    switch (wtype) {
    case 'domain':
    case 'email':
    case 'md5':
    case 'sha256':
    case 'ja3':
      return value.toLowerCase();
    case 'url':
      // capture looks up host/path, without the scheme or query string
      value = value.replace(/^https?:\/\//i, '').replace(/[?#].*$/, '');
      if (!value.includes('/')) { value += '/'; }
      return value;
    default:
      return value;
    }
  }

  // ----------------------------------------------------------------------------
  static #newInfo () {
    return { eventIds: new Set(), events: new Set(), threatLevels: new Set(), categories: new Set(), orgs: new Set(), tags: new Set() };
  }

  // ----------------------------------------------------------------------------
  static #addInfo (info, a) {
    const addStr = (set, v) => { if (ArkimeUtil.isString(v) && v !== '') { set.add(v); } };
    if (/^\d+$/.test('' + a.event_id)) { info.eventIds.add('' + a.event_id); }
    addStr(info.events, a.Event?.info);
    addStr(info.threatLevels, THREAT_LEVELS.get('' + a.Event?.threat_level_id));
    addStr(info.categories, a.category);
    addStr(info.orgs, a.Event?.Orgc?.name);
    if (Array.isArray(a.Tag)) { a.Tag.forEach((t) => addStr(info.tags, t?.name)); }
  }

  // ----------------------------------------------------------------------------
  #encode (info) {
    const args = [this.serverField, this.serverName];
    // result count is a single byte, leave room for the tags setting
    const push = (field, set) => set.forEach((v) => { if (args.length < 400) { args.push(field, v); } });
    push(this.eventIdField, info.eventIds);
    push(this.eventField, info.events);
    push(this.threatLevelField, info.threatLevels);
    push(this.categoryField, info.categories);
    push(this.orgField, info.orgs);
    push(this.tagField, info.tags);
    return WISESource.combineResults([WISESource.encodeResult.apply(null, args), this.tagsResult]);
  }

  // ----------------------------------------------------------------------------
  #get (wtype, value, cb) {
    if (this.mode === 'api') {
      return this.#apiLookup(wtype, value, cb);
    }
    const key = wtype === 'ip' || wtype === 'url' ? value : value.toLowerCase();
    cb(null, this.maps[wtype].get(key));
  }

  getIp (ip, cb) { this.#get('ip', ip, cb); }
  getDomain (domain, cb) { this.#get('domain', domain, cb); }
  getEmail (email, cb) { this.#get('email', email, cb); }
  getMd5 (md5, cb) { this.#get('md5', md5, cb); }
  getSha256 (sha256, cb) { this.#get('sha256', sha256, cb); }
  getURL (url, cb) { this.#get('url', url, cb); }
  getJa3 (ja3, cb) { this.#get('ja3', ja3, cb); }

  // ----------------------------------------------------------------------------
  itemCount () {
    return WISE_TYPES.reduce((sum, t) => sum + this.maps[t].size, 0);
  }

  // ----------------------------------------------------------------------------
  dump (res) {
    WISE_TYPES.forEach((t) => {
      res.write(`${t}:\n`);
      this.maps[t].forEach((value, key) => {
        res.write(`{"key": ${JSON.stringify(key)}, "ops":\n` + WISESource.result2JSON(value) + '},\n');
      });
    });
    res.end();
  }
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  const configDef = {
    singleton: false,
    name: 'misp',
    description: 'Look up attributes in a MISP server, either bulk loaded periodically or queried per item',
    link: 'https://arkime.com/wise#misp',
    types: WISE_TYPES,
    displayable: true,
    fields: [
      { name: 'url', required: true, help: 'The MISP server url' },
      { name: 'key', password: true, required: true, help: 'The MISP auth key' },
      { name: 'mode', required: false, help: 'bulk (default) periodically loads matching attributes into memory, api queries MISP for each item not in the wise cache and should be used with onlyIPs or excludeIPs' },
      { name: 'name', required: false, help: 'Name stored in misp.server, defaults to the section name after misp:' },
      { name: 'tags', required: false, help: 'Comma separated list of tags to set for matches', regex: '^[-a-z0-9,]+' },
      { name: 'last', required: false, help: 'Only use attributes from events published within this window, bulk mode default 30d, api mode default all' },
      { name: 'toIds', required: false, help: 'Only use attributes with the IDS flag set, default true' },
      { name: 'published', required: false, help: 'Only use attributes from published events, default true' },
      { name: 'enforceWarninglist', required: false, help: 'Skip attributes that match a MISP warninglist, default true' },
      { name: 'mispTags', required: false, help: 'Comma separated list of MISP tags to filter on' },
      { name: 'reload', required: false, help: 'bulk mode - Minutes between reloads, default 60' },
      { name: 'pageSize', required: false, help: 'bulk mode - Attributes per request, default 10000' },
      { name: 'maxAttributes', required: false, help: 'bulk mode - Stop loading after this many attributes, default 2000000' },
      { name: 'maxResults', required: false, help: 'api mode - Max attributes per lookup, default 50' },
      { name: 'maxConns', required: false, help: 'api mode - Max outstanding MISP requests, lookups past this are dropped, default 20' },
      { name: 'insecure', required: false, help: 'Accept self-signed certificates, default false' }
    ]
  };
  api.addSourceConfigDef('misp', configDef);
  // there is no type setting, so always show the exclude settings
  configDef.fields.forEach((f) => {
    if (f.ifField === 'type') {
      delete f.ifField;
      delete f.ifValue;
    }
  });

  const sections = api.getConfigSections().filter((e) => { return e.match(/^misp:/); });
  sections.forEach((section) => {
    return new MISPSource(api, section);
  });
};
