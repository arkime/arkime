/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class URLScanIntegration extends Integration {
  name = 'URLScan';
  icon = 'integrations/urlscan/icon.png';
  order = 500;
  itypes = {
    domain: 'fetch',
    ip: 'fetch'
  };

  homePage = 'https://urlscan.io/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    },
    key: {
      help: 'Your URLScan api key',
      password: true,
      required: true
    }
  };

  card = {
    title: 'URL Scan for %{query}',
    fields: [
      'total',
      {
        label: 'took'
      },
      'has_more',
      {
        label: 'results',
        type: 'table',
        fields: [
          {
            label: 'report',
            field: 'task.uuid',
            type: 'externalLink',
            postProcess: { template: 'https://urlscan.io/result/<value>' }
          },
          {
            label: 'scan date',
            field: 'task.time',
            postProcess: 'removeTime'
          },
          {
            label: 'visibility',
            field: 'task.visibility'
          },
          {
            label: 'method',
            field: 'task.method'
          },
          {
            label: 'url',
            field: 'task.url',
            defang: true // Maybe should be on server side?
          },
          {
            label: 'country',
            field: 'page.country'
          },
          {
            label: 'server',
            field: 'page.server'
          },
          {
            label: 'status',
            field: 'page.status'
          },
          {
            label: 'screenshot',
            field: 'screenshot',
            type: 'externalLink'
          },
          {
            label: 'malicious?',
            field: 'verdicts.malicious'
          },
          {
            label: 'brand',
            field: 'brand',
            type: 'array',
            fieldRoot: 'key'
          }
        ]
      }
    ]
  };

  tidbits = {
    order: 1000,
    fields: [
      {
        field: 'results',
        fieldRoot: 'verdicts.malicious',
        type: 'array',
        postProcess: [
          'removeNullish',
          { filter: false },
          'dedupeArray',
          { replaceValues: 'Malicious' }
        ],
        display: 'dangerGroup',
        tooltip: 'malicious?'
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

      const result = await axios.get('https://urlscan.io/api/v1/search/', {
        params: {
          q: encodeURIComponent(query)
        },
        headers: {
          'API-Key': key,
          'User-Agent': this.userAgent()
        }
      });

      if (result.data.total === 0) { return; }
      result.data._cont3xt = { count: result.data.total };
      return result.data;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, query, err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new URLScanIntegration();
