/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class URLHausIntegration extends Integration {
  name = 'URLHaus';
  icon = 'integrations/urlhaus/icon.png';
  order = 460;
  cacheTimeout = '1w';
  itypes = {
    domain: 'fetch',
    ip: 'fetch'
  };

  homePage = 'https://urlhaus.abuse.ch/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  card = {
    title: 'URLHaus for %{query}',
    fields: [
    ]
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetch (user, query) {
    try {
      const result = await axios.post('https://urlhaus-api.abuse.ch/v1/host/', `host=${query}`, {
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      if (result.data.query_status.startsWith('no_result')) {
        return Integration.NoResult;
      }

      result.data._cont3xt = { count: 0 };
      if (result.data.query_status === 'ok' && result.data.urls !== undefined) {
        result.data._cont3xt.count = result.data.urls.length;
      }
      return result.data;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, query, err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new URLHausIntegration();
