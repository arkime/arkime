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

class ThreatstreamIntegration extends Integration {
  name = 'Threatstream';
  icon = 'public/threatstreamIcon.webp';
  itypes = {
    domain: 'fetch',
    ip: 'fetch',
    email: 'fetch',
    url: 'fetch',
    hash: 'fetch'
  };

  userSettings = {
    ThreatstreamHost: {
      help: 'The threatstream host to send queries'
    },
    ThreatstreamApiUser: {
      help: 'Your threatstream api user'
    },
    ThreatstreamApiKey: {
      help: 'Your threatstream api key',
      password: true
    }
  }

  constructor () {
    super();

    Integration.register(this);
  }

  async fetch (user, query) {
    try {
      const host = this.getUserConfig(user, 'ThreatstreamHost', 'api.threatstream.com');
      const tuser = this.getUserConfig(user, 'ThreatstreamApiUser');
      const tkey = this.getUserConfig(user, 'ThreatstreamApiKey');
      if (!tkey || !tuser) {
        return undefined;
      }
      const threatstreamRes = await axios.get(`https://${host}/api/v2/intelligence`, {
        params: {
          value__exact: query
        },
        headers: {
          Authorization: `apikey ${tuser}:${tkey}`,
          'User-Agent': this.userAgent()
        }
      });

      return threatstreamRes.data;
    } catch (err) {
      console.log(this.name, query, err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new ThreatstreamIntegration();
