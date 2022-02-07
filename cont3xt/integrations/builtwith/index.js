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

class BuiltWithIntegration extends Integration {
  name = 'BuiltWith';
  icon = 'integrations/builtwith/icon.png';
  order = 360;
  cacheTimeout = '1w';
  itypes = {
    domain: 'fetchDomain'
  };

  homePage = 'https://builtwith.com/'
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    },
    key: {
      help: 'Your BuiltWith Key',
      password: true,
      required: true
    }
  }

  card = {
    title: 'BuiltWith for %{query}',
    fields: [
      'domain',
      {
        label: 'first',
        type: 'seconds'
      },
      {
        label: 'last',
        type: 'seconds'
      },
      {
        label: 'groups',
        type: 'table',
        fields: [
          'name',
          'live',
          'dead',
          {
            label: 'latest',
            type: 'seconds'
          },
          {
            label: 'oldest',
            type: 'seconds'
          }
        ]
      }
    ]
  }

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchDomain (user, query) {
    try {
      const key = this.getUserConfig(user, this.name, 'key');
      if (!key) {
        return undefined;
      }

      const result = await axios.get(`https://api.builtwith.com/free1/api.json?KEY=${key}&LOOKUP=${query}`, {
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      result.data._cont3xt = { count: result.data.groups?.length ?? 0 };
      return result.data;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new BuiltWithIntegration();
