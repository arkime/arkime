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
const Integration = require('../integration.js');
const axios = require('axios');

class URLScanIntegration extends Integration {
  name = 'URLScan';
  icon = 'public/urlscanIcon.png';
  itypes = {
    domain: 'fetch',
    ip: 'fetch'
  };

  userSettings = {
    URLScanKey: {
      help: 'Your URLScan api key',
      password: true
    }
  }

  card = {
    title: 'URL Scan for %{query}',
    fields: [
      'total',
      {
        field: 'took',
        type: 'ms'
      },
      'has_more',
      {
        name: 'results',
        type: 'table',
        field: 'results',
        fields: [
          {
            name: 'visibility',
            field: 'task.visibility'
          },
          {
            name: 'method',
            field: 'task.method'
          },
          {
            name: 'url',
            field: 'task.url',
            defang: true // Maybe should be on server side?
          },
          {
            name: 'country',
            field: 'page.country'
          },
          {
            name: 'server',
            field: 'page.server'
          },
          {
            name: 'status',
            field: 'page.status'
          },
          {
            name: 'screenshot',
            field: 'screenshot',
            type: 'url'
          }
        ]
      }
    ]
  }

  constructor () {
    super();

    Integration.register(this);
  }

  async fetch (user, query) {
    try {
      const key = this.getUserConfig(user, 'URLScanKey');
      if (!key) {
        return undefined;
      }

      const result = await axios.get('https://urlscan.io/api/v1/search/', {
        params: {
          q: query
        },
        headers: {
          'API-Key': key,
          'User-Agent': this.userAgent()
        }
      });

      if (result.data.total === 0) { return; }
      result.data._count = result.data.total;
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
