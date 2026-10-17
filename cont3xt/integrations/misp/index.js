/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

const Integration = require('../../integration.js');
const ArkimeConfig = require('../../../common/arkimeConfig');
const ArkimeUtil = require('../../../common/arkimeUtil');
const axios = require('axios');
const https = require('https');

ArkimeConfig.registerSettings({
  key: { secret: true }
});

const THREAT_LEVELS = new Map([['1', 'High'], ['2', 'Medium'], ['3', 'Low'], ['4', 'Undefined']]);

function str (v) {
  return ArkimeUtil.isString(v) ? v : undefined;
}

class MISPIntegration extends Integration {
  name;
  section;
  icon = 'integrations/misp/icon.png';
  order;
  itypes = {};

  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  card = {
    fields: [{
      label: 'Attributes',
      field: 'attributes',
      type: 'table',
      defaultSortField: 'timestamp',
      defaultSortDirection: 'desc',
      fields: [
        { label: 'Value', field: 'value', pivot: true },
        { label: 'Type', field: 'type' },
        { label: 'Category', field: 'category' },
        { label: 'IDS', field: 'to_ids' },
        { label: 'Event', field: 'eventInfo' },
        { label: 'Threat Level', field: 'threatLevel' },
        { label: 'Org', field: 'org' },
        { label: 'Tags', field: 'tags', type: 'array', join: ', ' },
        { label: 'Comment', field: 'comment' },
        { label: 'Updated', field: 'timestamp', type: 'seconds' },
        { label: 'Event Link', field: 'eventUrl', type: 'url' }
      ]
    }]
  };

  static #order = 51000;

  #url;
  #key;
  #maxResults;
  #enforceWarninglist;
  #toIds;
  #httpsAgent;

  // ----------------------------------------------------------------------------
  constructor (section) {
    super();

    this.section = section;
    this.name = ArkimeConfig.getFull(section, 'name', section);
    this.icon = ArkimeConfig.getFull(section, 'icon', this.icon);
    this.order = MISPIntegration.#order++;

    this.#url = ArkimeConfig.getFull(section, 'url', ArkimeConfig.exit).replace(/\/+$/, '');
    this.#key = ArkimeConfig.getFull(section, 'key', ArkimeConfig.exit);
    this.#maxResults = parseInt(ArkimeConfig.getFull(section, 'maxResults', 50), 10);
    if (!(this.#maxResults > 0)) { this.#maxResults = 50; }
    this.#enforceWarninglist = ArkimeConfig.getFullBool(section, 'enforceWarninglist', true);
    this.#toIds = ArkimeConfig.getFullBool(section, 'toIds', false);

    if (ArkimeConfig.getFullBool(section, 'insecure', false)) {
      this.#httpsAgent = new https.Agent({ rejectUnauthorized: false });
    }

    const itypes = ArkimeConfig.getFullArray(section, 'itypes', 'ip,domain,url,email,hash,phone');
    itypes.forEach(itype => {
      this.itypes[itype] = 'fetchItem';
    });

    this.card.title = `${this.name} for %{query}`;
    // the query lands in the path unencoded, so no urls
    this.card.searchUrls = [{
      url: `${this.#url}/attributes/index/searchvalue:%{query}`,
      itypes: itypes.filter(itype => itype !== 'url'),
      name: `Search ${this.name} for %{query}`
    }];

    Integration.register(this);
  }

  // ----------------------------------------------------------------------------
  async fetchItem (user, item) {
    // MISP treats % as a wildcard, a leading ! as NOT and splits on && and ||, only do exact matches
    if (item.includes('%') || item.startsWith('!') || item.includes('&&') || item.includes('||')) {
      return Integration.NoResult;
    }

    try {
      const body = {
        value: [item],
        returnFormat: 'json',
        limit: this.#maxResults,
        page: 1,
        includeEventTags: true,
        includeContext: true,
        enforceWarninglist: this.#enforceWarninglist
      };
      if (this.#toIds) { body.to_ids = true; }

      const result = await axios.post(`${this.#url}/attributes/restSearch`, body, {
        headers: {
          Authorization: this.#key,
          Accept: 'application/json',
          'Content-Type': 'application/json',
          'User-Agent': this.userAgent()
        },
        httpsAgent: this.#httpsAgent,
        maxRedirects: 0,
        timeout: 30 * 1000
      });

      const attrs = result.data?.response?.Attribute;
      if (!Array.isArray(attrs) || attrs.length === 0) {
        return Integration.NoResult;
      }

      const attributes = attrs.filter(a => a && typeof a === 'object').map(a => {
        const eventId = '' + (a.event_id ?? '');
        return {
          value: str(a.value),
          type: str(a.type),
          category: str(a.category),
          to_ids: a.to_ids === true || a.to_ids === '1',
          comment: str(a.comment),
          timestamp: parseInt(a.timestamp, 10) || undefined,
          eventId,
          eventInfo: str(a.Event?.info),
          eventDate: str(a.Event?.date),
          threatLevel: THREAT_LEVELS.get('' + a.Event?.threat_level_id),
          org: str(a.Event?.Orgc?.name),
          tags: Array.isArray(a.Tag) ? a.Tag.map(t => str(t?.name)).filter(t => t) : [],
          eventUrl: /^\d+$/.test(eventId) ? `${this.#url}/events/view/${eventId}` : undefined
        };
      });

      return {
        attributes,
        _cont3xt: {
          count: attributes.length
        }
      };
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, item, err?.response?.status ?? '', err?.response?.data?.message ?? err.message);
      return null;
    }
  }
}

// ----------------------------------------------------------------------------
const sections = ArkimeConfig.getSections().filter((e) => { return e.match(/^misp:/); });
sections.forEach((section) => {
  new MISPIntegration(section);
});
