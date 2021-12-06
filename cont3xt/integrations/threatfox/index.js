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

class ThreatFoxIntegration extends Integration {
  name = 'ThreatFox';
  icon = 'integrations/threatfox/icon.png';
  order = 300;
  cacheTimeout = '1w';
  itypes = {
    ip: 'fetchIp',
    hash: 'fetchHash'
  };

  homePage = 'https://threatfox.abuse.ch/'
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  }

  card = {
    title: 'ThreatFox for %{query}',
    fields: [
    ]
  }

  constructor () {
    super();

    Integration.register(this);
  }

  async fetch (user, body) {
    try {
      const result = await axios.post('https://threatfox-api.abuse.ch/api/v1/', body, {
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      if (result.data.query_status.startsWith('no_result')) {
        return Integration.NoResult;
      }

      if (result.data.query_status === 'ok') {
        result.data._count = result.data.data.length;
      } else {
        result.data._count = 0;
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
}

// eslint-disable-next-line no-new
new ThreatFoxIntegration();
