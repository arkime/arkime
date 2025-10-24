/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class ZetalyticsIntegration extends Integration {
  name = 'Zetalytics';
  icon = 'integrations/zetalytics/icon.png';
  itypes = {
    domain: 'fetchDomain',
    ip: 'fetchIp'
  };

  homePage = 'https://zetalytics.com/';
  settings = {
    token: {
      help: 'Your Zetalytics API token',
      password: true,
      required: true
    }
  };

  card = {
    title: 'Zetalytics Passive DNS for %{query}',
    fields: [
      {
        label: 'info',
        field: 'info'
      },
      {
        label: 'Results',
        field: 'results',
        type: 'table',
        defaultSortField: 'last_seen',
        defaultSortDirection: 'desc',
        fields: [
          {
            label: 'Domain',
            field: 'domain',
            pivot: true
          },
          {
            label: 'IP',
            field: 'value_ip',
            pivot: true
          },
          {
            label: 'QNAME',
            field: 'qname'
          },
          {
            label: 'First Seen',
            field: 'date',
            type: 'date'
          },
          {
            label: 'Last Seen',
            field: 'last_seen',
            type: 'date'
          },
          {
            label: 'Type',
            field: 'type'
          }
        ]
      }
    ]
  };

  tidbits = {
    order: 100,
    fields: [
      {
        field: 'total',
        tooltip: 'passive DNS records'
      }
    ]
  };

  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();

    Integration.register(this);
  }

  async fetch (user, query, endpoint) {
    try {
      const token = this.getUserConfig(user, 'token');
      if (!token) {
        return undefined;
      }

      const result = await axios.get(`https://zonecruncher.com/api/v2/${endpoint}`, {
        params: {
          token,
          q: query
        },
        headers: {
          Accept: 'application/json',
          'User-Agent': this.userAgent()
        }
      });

      if (!result.data || !result.data.results || result.data.results.length === 0) {
        return undefined;
      }

      result.data._cont3xt = { count: result.data.total };
      return result.data;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) {
        return null;
      }
      if (err?.response?.data) {
        console.log(this.name, query, 'Error:', err.response.status, JSON.stringify(err.response.data));
      } else {
        console.log(this.name, query, err);
      }
      return null;
    }
  }

  fetchIp (user, ip) {
    return this.fetch(user, ip, 'ip');
  }

  fetchDomain (user, domain) {
    return this.fetch(user, domain, 'domain2ip');
  }
}

new ZetalyticsIntegration();
