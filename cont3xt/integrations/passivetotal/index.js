/******************************************************************************/
/* Copyright Yahoo Inc.
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
const Integration = require('../../integration.js');
const axios = require('axios');

class PassiveTotalIntegration extends Integration {
  name = 'PassiveTotal';
  icon = 'integrations/passivetotal/icon.png';
  noStats = true;
  itypes = {
  };

  homePage = 'https://community.riskiq.com/registration/signupforfree'
  settings = {
    user: {
      help: 'Your Passive Total api user',
      required: true
    },
    key: {
      help: 'Your Passive Total api key',
      password: true,
      required: true
    }
  };

  // Default cacheTimeout 24 hours
  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();

    Integration.register(this);
  }
}

class PassiveTotalWhoisIntegration extends Integration {
  name = 'PT Whois';
  icon = 'integrations/passivetotal/iconWhois.png';
  order = 120;
  configName = 'PassiveTotal';
  itypes = {
    domain: 'fetchDomain'
  };

  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  }

  card = {
    title: 'PassiveTotal Whois for %{query}',
    fields: [
      'registrar',
      'organization',
      {
        label: 'registered',
        type: 'date'
      },
      {
        label: 'expiresAt',
        type: 'date'
      },
      {
        label: 'lastLoadedAt',
        type: 'date'
      },
      {
        label: 'nameServers',
        type: 'array',
        join: ', '
      },
      {
        label: 'registrant',
        type: 'json'
      },
      'whoisServer',
      'name',
      'telephone',
      'domainStatus',
      'contactEmail',
      {
        label: 'registryUpdatedAt',
        type: 'date'
      }
    ]
  }

  // Default cacheTimeout 24 hours
  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchDomain (user, domain) {
    try {
      const puser = this.getUserConfig(user, 'PassiveTotal', 'user');
      const pkey = this.getUserConfig(user, 'PassiveTotal', 'key');
      if (!puser || !pkey) {
        return undefined;
      }

      const result = await axios.get('https://api.passivetotal.org/v2/whois', {
        params: {
          query: domain,
          history: false
        },
        auth: {
          username: puser,
          password: pkey
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      result.data._count = 1;
      return result.data;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, domain, err);
      return null;
    }
  }
}

class PassiveTotalSubdomainsIntegration extends Integration {
  name = 'PT Subdomains';
  icon = 'integrations/passivetotal/iconSubDomains.png';
  order = 320;
  configName = 'PassiveTotal';
  itypes = {
    domain: 'fetchDomain'
  };

  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  }

  card = {
    title: 'PassiveTotal Subdomains for %{query}',
    fields: [
      {
        label: 'subdomains',
        type: 'array',
        field: 'subdomains'
      }
    ]
  }

  // Default cacheTimeout 24 hours
  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchDomain (user, domain) {
    try {
      const puser = this.getUserConfig(user, 'PassiveTotal', 'user');
      const pkey = this.getUserConfig(user, 'PassiveTotal', 'key');
      if (!puser || !pkey) {
        return undefined;
      }

      const result = await axios.get('https://api.passivetotal.org/v2/enrichment/subdomains', {
        params: {
          query: domain
        },
        auth: {
          username: puser,
          password: pkey
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      result.data._count = result.data.subdomains.length;
      if (result.data.subdomains.length === 0) { return undefined; }
      return result.data;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, domain, err);
      return null;
    }
  }
}

class PassiveTotalDNSIntegration extends Integration {
  name = 'PT DNS';
  icon = 'integrations/passivetotal/icon.png';
  order = 300;
  configName = 'PassiveTotal';
  itypes = {
    ip: 'fetch',
    domain: 'fetch'
  };

  homePage = ''
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  }

  card = {
    title: 'PassiveTotal Domain Passive DNS for %{query}',
    fields: [
      'firstSeen',
      'lastSeen',
      'totalRecords',
      {
        label: 'results',
        type: 'table',
        field: 'results',
        defaultSortField: 'firstSeen',
        defaultSortDirection: 'desc',
        fields: [
          {
            label: 'DNS Type',
            field: 'recordType'
          },
          {
            label: 'Type',
            field: 'resolveType'
          },
          {
            label: 'Value',
            field: 'resolve',
            pivot: true
          },
          {
            label: 'First Seen',
            field: 'firstSeen',
            type: 'date'
          },
          {
            label: 'Last Seen',
            field: 'lastSeen',
            type: 'date'
          }
        ]
      }
    ]
  }

  // Default cacheTimeout 24 hours
  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();

    Integration.register(this);
  }

  async fetch (user, query) {
    try {
      const puser = this.getUserConfig(user, 'PassiveTotal', 'user');
      const pkey = this.getUserConfig(user, 'PassiveTotal', 'key');
      if (!puser || !pkey) {
        return undefined;
      }

      const result = await axios.get('https://api.passivetotal.org/v2/dns/passive', {
        params: {
          query: query
        },
        auth: {
          username: puser,
          password: pkey
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      if (result.data.totalRecords === 0 || result.data.results.length === 0) { return undefined; }
      result.data._count = parseInt(result.data.totalRecords);
      return result.data;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, query, err);
      return null;
    }
  }
}

/* eslint-disable no-new */
new PassiveTotalIntegration();
new PassiveTotalWhoisIntegration();
new PassiveTotalSubdomainsIntegration();
new PassiveTotalDNSIntegration();
