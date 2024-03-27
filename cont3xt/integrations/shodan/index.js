/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class ShodanIntegration extends Integration {
  name = 'Shodan';
  icon = 'integrations/shodan/icon.png';
  order = 260;
  itypes = {
    ip: 'fetchIp'
  };

  card = {
    title: 'Shodan for %{query}',
    searchUrls: [{
      url: 'https://www.shodan.io/host/%{query}',
      itypes: ['ip'],
      name: 'Search Shodan for %{query}'
    }],
    fields: [
      {
        label: 'tags',
        type: 'array',
        join: ', '
      },
      {
        label: 'ports',
        type: 'array',
        join: ', '
      },
      {
        label: 'Service/Port info',
        type: 'table',
        field: 'data',
        fields: [
          {
            label: 'product'
          },
          {
            label: 'port'
          },
          {
            label: 'title',
            field: 'http.title'
          },
          {
            label: 'server',
            field: 'http.server'
          },
          {
            label: 'module',
            field: '_shodan.module'
          },
          {
            label: 'data'
          },
          {
            label: 'html',
            field: 'http.html'
          }
        ]
      },
      {
        label: 'Certificates',
        type: 'table',
        field: 'data',
        fieldRoot: 'ssl.cert',
        fields: [
          {
            label: 'issued',
            field: 'issued'
          },
          {
            label: 'expires',
            field: 'expires'
          },
          {
            label: 'sha1',
            field: 'fingerprint.sha1'
          },
          {
            label: 'issuer',
            field: 'issuer',
            type: 'json'
          }
        ]
      },
      'region_code',
      {
        label: 'domains',
        type: 'array',
        join: ', '
      },
      {
        label: 'hostnames',
        type: 'array',
        join: ', '
      },
      'country_name',
      'country_code',
      'org',
      'asn',
      'city',
      'isp',
      'latitude',
      'longitude',
      'ip',
      'ip_str',
      'last_update'
    ]
  };

  homePage = 'https://www.shodan.io/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    },
    key: {
      help: 'Your Shodan api key',
      password: true,
      required: true
    }
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchIp (user, ip) {
    try {
      const key = this.getUserConfig(user, 'key');
      if (!key) {
        return undefined;
      }

      const response = await axios.get(`https://api.shodan.io/shodan/host/${ip}?key=${key}`, {
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      response.data._cont3xt = { count: response.data.data.length };
      return response.data;
    } catch (err) {
      if (err?.response?.status === 404) { return Integration.NoResult; }
      console.log(this.name, ip, err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new ShodanIntegration();
