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
    }
  };

  card = {
    title: 'URLHaus for %{query}',
    fields: [
    ]
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetch (user, query) {
    try {
      const result = await axios.post('https://urlhaus-api.abuse.ch/v1/host/', `host=${query}`, {
        headers: {
          'User-Agent': this.userAgent()
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

// eslint-disable-next-line no-new
new URLHausIntegration();
