/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class DomainToolsWhoisIntegration extends Integration {
  name = 'DomainTools Whois';
  icon = 'integrations/domaintools/icon.png';
  order = 130;
  itypes = {
    domain: 'fetchDomain'
  };

  homePage = 'https://www.domaintools.com/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    },
    user: {
      help: 'Your DomainTools API username',
      required: true
    },
    key: {
      help: 'Your DomainTools API key',
      password: true,
      required: true
    }
  };

  card = {
    title: 'DomainTools Whois for %{query}',
    searchUrls: [{
      url: 'https://whois.domaintools.com/%{query}',
      itypes: ['domain'],
      name: 'Search DomainTools Whois for %{query}'
    }],
    fields: [
      {
        label: 'registrant_name',
        field: 'registrant.name'
      },
      {
        label: 'registrant_domains',
        field: 'registrant.domains'
      },
      {
        label: 'registrar',
        field: 'registration.registrar'
      },
      {
        label: 'created',
        field: 'registration.created',
        type: 'date'
      },
      {
        label: 'expires',
        field: 'registration.expires',
        type: 'date'
      },
      {
        label: 'updated',
        field: 'registration.updated',
        type: 'date'
      },
      {
        label: 'statuses',
        field: 'registration.statuses',
        type: 'array',
        join: ', '
      },
      {
        label: 'name_servers',
        field: 'name_servers',
        type: 'table',
        fields: [
          {
            label: 'server',
            field: 'server'
          }
        ]
      },
      {
        label: 'server_ip',
        field: 'server.ip_address'
      },
      {
        label: 'server_other_domains',
        field: 'server.other_domains'
      },
      {
        label: 'whois_history_records',
        field: 'history.whois.records'
      },
      {
        label: 'whois_history_earliest',
        field: 'history.whois.earliest_event',
        type: 'date'
      },
      {
        label: 'ip_history_events',
        field: 'history.ip_address.events'
      },
      {
        label: 'ip_history_timespan_years',
        field: 'history.ip_address.timespan_in_years'
      }
    ]
  };

  tidbits = {
    fields: [
      {
        field: 'registration.created',
        postProcess: 'removeTime',
        tooltipTemplate: '<value>',
        purpose: 'registered',
        precedence: 2,
        order: 100
      },
      {
        field: 'registration.registrar',
        tooltip: 'registrar',
        purpose: 'registrar',
        precedence: 2,
        order: 110
      }
    ]
  };

  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchDomain (user, domain) {
    try {
      const dtUser = this.getUserConfig(user, 'user');
      const dtKey = this.getUserConfig(user, 'key');
      if (!dtUser || !dtKey) {
        return undefined;
      }

      const result = await axios.get(`https://api.domaintools.com/v1/${domain}`, {
        params: {
          api_username: dtUser,
          api_key: dtKey
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      if (!result.data || !result.data.response) {
        return undefined;
      }

      result.data.response._cont3xt = { count: 1 };
      return result.data.response;
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

new DomainToolsWhoisIntegration();
