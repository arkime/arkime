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
    hash: 'fetchHash',
    domain: 'fetchDomain'
  };

  homePage = 'https://threatfox.abuse.ch/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    },
    key: {
      help: 'Your ThreatFox API key',
      password: true,
      required: true
    }
  };

  card = {
    title: 'ThreatFox for %{query}',
    searchUrls: [{
      url: 'https://threatfox.abuse.ch/browse.php?search=%{query}',
      itypes: ['ip', 'domain', 'hash'],
      name: 'Search ThreatFox for %{query}'
    }],
    fields: [
      {
        label: 'query_status',
        field: 'query_status'
      },
      {
        label: 'Threats',
        field: 'data',
        type: 'table',
        defaultSortField: 'first_seen',
        defaultSortDirection: 'desc',
        fields: [
          {
            label: 'IOC',
            field: 'ioc',
            pivot: true
          },
          {
            label: 'Malware',
            field: 'malware_printable'
          },
          {
            label: 'Threat Type',
            field: 'threat_type_desc'
          },
          {
            label: 'IOC Type',
            field: 'ioc_type_desc'
          },
          {
            label: 'Confidence',
            field: 'confidence_level'
          },
          {
            label: 'First Seen',
            field: 'first_seen',
            type: 'date'
          },
          {
            label: 'Last Seen',
            field: 'last_seen',
            type: 'date'
          },
          {
            label: 'Reporter',
            field: 'reporter'
          },
          {
            label: 'Tags',
            field: 'tags',
            type: 'array',
            join: ', '
          },
          {
            label: 'Malpedia',
            field: 'malware_malpedia',
            type: 'url'
          },
          {
            label: 'Reference',
            field: 'reference',
            type: 'url'
          }
        ]
      }
    ]
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetch (user, body) {
    try {
      const key = this.getUserConfig(user, 'key');
      if (!key) {
        return undefined;
      }

      const result = await axios.post('https://threatfox-api.abuse.ch/api/v1/', body, {
        headers: {
          'User-Agent': this.userAgent(),
          'Auth-Key': key
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

  fetchDomain (user, domain) {
    return this.fetch(user, `{ "query": "search_ioc", "search_term": "${domain}" }`);
  }
}

new ThreatFoxIntegration();
