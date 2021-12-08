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

class CymruIntegration extends Integration {
  name = 'Cymru';
  icon = 'integrations/cymru/icon.png';
  itypes = {
    hash: 'fetchHash'
  };

  card = {
    title: 'Cymru for %{query}',
    fields: [
      {
        field: 'lastSeen',
        label: 'Last Seen',
        type: 'seconds'
      }
    ]
  }

  cacheTimeout = '1h';

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchHash (user, hash) {
    try {
      const instance = axios.create({
        headers: { Accept: 'application/dns-json' }
      });

      const response = await instance.get(`https://cloudflare-dns.com/dns-query?name=${hash}.hash.cymru.com&type=TXT`);
      if (response.data.Status === 3) {
        return Integration.NoResult;
      }
      const str = response.data.Answer[0].data;
      const parts = str.slice(1, str.length - 1).split(' ');
      const result = {
        lastSeen: parseInt(parts[0]),
        _count: parseInt(parts[1])
      };

      if (result._count > 0) {
        result._severity = 'high';
      }
      return result;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, hash, err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new CymruIntegration();
