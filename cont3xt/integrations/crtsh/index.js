/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class CrtShIntegration extends Integration {
  name = 'crt.sh';
  icon = 'integrations/crtsh/icon.png';
  order = 140;
  itypes = {
    domain: 'fetchDomain'
  };

  homePage = 'https://crt.sh/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  card = {
    title: 'crt.sh Certificates for %{query}',
    searchUrls: [{
      url: 'https://crt.sh/?q=%{query}',
      itypes: ['domain'],
      name: 'Search crt.sh for %{query}'
    }],
    fields: [
      {
        label: 'certificates',
        field: 'certificates',
        type: 'table',
        fields: [
          {
            label: 'common_name',
            field: 'common_name'
          },
          {
            label: 'name_value',
            field: 'name_value'
          },
          {
            label: 'issuer_name',
            field: 'issuer_name'
          },
          {
            label: 'not_before',
            field: 'not_before',
            type: 'date'
          },
          {
            label: 'not_after',
            field: 'not_after',
            type: 'date'
          },
          {
            label: 'serial_number',
            field: 'serial_number'
          },
          {
            label: 'id',
            field: 'id'
          }
        ]
      }
    ]
  };

  tidbits = {
    order: 100,
    fields: [
      {
        field: 'count',
        tooltip: 'certificates found'
      }
    ]
  };

  // Cache for 1 week due to crt.sh being a flaky service that frequently returns
  // 503 errors with database recovery conflicts and timeouts
  cacheTimeout = 7 * 24 * 60 * 60 * 1000;

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchDomain (user, domain) {
    try {
      const result = await axios.get('https://crt.sh/', {
        params: {
          q: domain,
          output: 'json'
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      if (!result.data || !Array.isArray(result.data) || result.data.length === 0) {
        return undefined;
      }

      const data = {
        certificates: result.data,
        count: result.data.length,
        _cont3xt: { count: result.data.length }
      };

      return data;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      if (err?.response?.data) {
        console.log(this.name, domain, 'Error:', err.response.status, JSON.stringify(err.response.data));
      } else {
        console.log(this.name, domain, err);
      }
      return null;
    }
  }
}

new CrtShIntegration();
