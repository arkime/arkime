/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class ThreatFoxIntegration extends Integration {
  name = 'ThreatFox';
  icon = 'integrations/threatfox/icon.png';
  order = 420;
  cacheTimeout = '1w';
  itypes = {
    ip: 'fetchIp',
    hash: 'fetchHash'
  };

  homePage = 'https://threatfox.abuse.ch/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  card = {
    title: 'ThreatFox for %{query}',
    fields: [
    ]
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetch (user, body) {
    try {
      const result = await axios.post('https://threatfox-api.abuse.ch/api/v1/', body, {
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      if (result.data.query_status.startsWith('no_result')) {
        return Integration.NoResult;
      }

      result.data._cont3xt = { count: 0 };
      if (result.data.query_status === 'ok' && result.data.data !== undefined) {
        result.data._cont3xt.count = result.data.data.length;
      }
      return result.data;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, body, err);
      return null;
    }
  }

  fetchIp (user, ip) {
    return this.fetch(user, `{ "query": "search_ioc", "search_term": "${ip}" }`);
  }

  fetchHash (user, hash) {
    return this.fetch(user, `{ "query": "search_hash", "hash": "${hash}" }`);
  }
}

// eslint-disable-next-line no-new
new ThreatFoxIntegration();
