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
    },
    key: {
      help: 'Your abuse.ch Auth-Key, free at https://auth.abuse.ch/',
      password: true,
      required: true
    }
  };

  card = {
    title: 'URLHaus for %{query}',
    searchUrls: [{
      url: 'https://urlhaus.abuse.ch/browse.php?search=%{query}',
      itypes: ['domain', 'url', 'hash', 'text'],
      name: 'Search URLHaus for %{query}'
    }],
    fields: [
      {
        label: 'query_status',
        field: 'query_status'
      },
      {
        label: 'First Seen',
        field: 'firstseen',
        type: 'date'
      },
      {
        label: 'URL Count',
        field: 'url_count'
      },
      {
        label: 'Reference',
        field: 'urlhaus_reference',
        type: 'url'
      },
      {
        label: 'URLs',
        field: 'urls',
        type: 'table',
        defaultSortField: 'date_added',
        defaultSortDirection: 'desc',
        fields: [
          {
            label: 'URL',
            field: 'url',
            pivot: true
          },
          {
            label: 'Status',
            field: 'url_status'
          },
          {
            label: 'Threat',
            field: 'threat'
          },
          {
            label: 'Date Added',
            field: 'date_added',
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
            label: 'Reference',
            field: 'urlhaus_reference',
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

  async fetch (user, query) {
    try {
      const key = this.getUserConfig(user, 'key');
      if (!key) {
        return undefined;
      }

      const result = await axios.post('https://urlhaus-api.abuse.ch/v1/host/', `host=${encodeURIComponent(query)}`, {
        headers: {
          'User-Agent': this.userAgent(),
          'Auth-Key': key
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

new URLHausIntegration();
