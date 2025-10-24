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
    searchUrls: [{
      url: 'https://zonecruncher.com/search/?q=%{query}',
      itypes: ['ip', 'domain'],
      name: 'Search Zetalytics for %{query}'
    }],
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

  async fetchIp (user, ip) {
    try {
      const token = this.getUserConfig(user, 'token');
      if (!token) {
        return undefined;
      }

      const result = await axios.get('https://zonecruncher.com/api/v2/ip', {
        params: {
          q: ip,
          token: token
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
        console.log(this.name, ip, 'Error:', err.response.status, JSON.stringify(err.response.data));
      } else {
        console.log(this.name, ip, err);
      }
      return null;
    }
  }

  async fetchDomain (user, domain) {
    try {
      const token = this.getUserConfig(user, 'token');
      if (!token) {
        return undefined;
      }

      const result = await axios.get('https://zonecruncher.com/api/v2/domain2ip', {
        params: {
          q: domain,
          token: token
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
        console.log(this.name, domain, 'Error:', err.response.status, JSON.stringify(err.response.data));
      } else {
        console.log(this.name, domain, err);
      }
      return null;
    }
  }
}

new ZetalyticsIntegration();
