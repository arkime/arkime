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

class ShodanIntegration extends Integration {
  name = 'Shodan';
  icon = 'public/shodanIcon.png';
  itypes = {
    ip: 'fetchIp'
  };

  card = {
    title: 'Shodan for %{query}',
    fields: [
      {
        name: 'tags',
        type: 'array',
        join: ', '
      },
      {
        name: 'ports',
        type: 'array',
        join: ', '
      },
      {
        name: 'Service/Port info',
        type: 'table',
        field: 'data',
        fields: [
          {
            name: 'product'
          },
          {
            name: 'port'
          },
          {
            name: 'title',
            field: 'http.title'
          },
          {
            name: 'server',
            field: 'http.server'
          },
          {
            name: 'module',
            field: '_shodan.module'
          },
          {
            name: 'data'
          },
          {
            name: 'html',
            field: 'http.html'
          }
        ]
      },
      {
        name: 'Certificates',
        type: 'table',
        field: 'data',
        fields: [
          {
            name: 'issued',
            field: 'ssl.cert.issued'
          },
          {
            name: 'expires',
            field: 'ssl.cert.expires'
          },
          {
            name: 'sha1',
            field: 'ssl.cert.fingerprint.sha1'
          },
          {
            name: 'issuer',
            field: 'ssl.cert.issuer',
            type: 'json'
          }
        ]
      },
      'region_code',
      {
        name: 'domains',
        type: 'array',
        join: ', '
      },
      {
        name: 'hostnames',
        type: 'array',
        join: ', '
      },
      'country_name',
      'country_code',
      'org',
      'asn',
      'city',
      'isp',
      'latitude',
      'longitude',
      'ip',
      'ip_str',
      'last_update'
    ]
  }

  userSettings = {
    ShodanKey: {
      help: 'Your Shodan api key',
      password: true
    }
  }

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchIp (user, ip) {
    try {
      const key = this.getUserConfig(user, 'ShodanKey');
      if (!key) {
        return undefined;
      }

      const response = await axios.get(`https://api.shodan.io/shodan/host/${ip}?key=${key}`, {
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      response.data._count = 1;
      return response.data;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, ip, err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new ShodanIntegration();
