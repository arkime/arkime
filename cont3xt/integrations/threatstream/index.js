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

class ThreatstreamIntegration extends Integration {
  name = 'Threatstream';
  icon = 'integrations/threatstream/icon.png';
  order = 15000;
  itypes = {
    domain: 'fetch',
    ip: 'fetch',
    email: 'fetch',
    url: 'fetch',
    hash: 'fetch'
  };

  card = {
    title: 'Threatstream for %{query}',
    fields: [
      {
        label: 'Objects',
        field: 'objects',
        type: 'table',
        fields: [
          'status',
          'tlp',
          'itype',
          'source',
          'confidence',
          'import_session_id',
          {
            label: 'created_ts',
            type: 'date'
          }
        ]
      }
    ]
  };

  homePage = 'https://www.anomali.com/products/threatstream'
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    },
    host: {
      help: 'The threatstream host to send queries. Only set if you have a on premise deployment.'
    },
    user: {
      help: 'Your threatstream api user',
      required: true
    },
    key: {
      help: 'Your threatstream api key',
      password: true,
      required: true
    }
  }

  constructor () {
    super();

    Integration.register(this);
  }

  async fetch (user, query) {
    try {
      const host = this.getUserConfig(user, 'Threatstream', 'host', 'api.threatstream.com');
      const tuser = this.getUserConfig(user, 'Threatstream', 'user');
      const tkey = this.getUserConfig(user, 'Threatstream', 'key');
      if (!tkey || !tuser) {
        return undefined;
      }
      const result = await axios.get(`https://${host}/api/v2/intelligence`, {
        params: {
          value__exact: query
        },
        headers: {
          Authorization: `apikey ${tuser}:${tkey}`,
          'User-Agent': this.userAgent()
        }
      });

      if (result.data.meta.total_count === 0) { return Integration.NoResult; }
      result.data._cont3xt = {
        severity: 'high',
        count: result.data.meta.total_count
      };

      return result.data;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, query, err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new ThreatstreamIntegration();
