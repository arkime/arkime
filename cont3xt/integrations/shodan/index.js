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

class ShodanIntegration extends Integration {
  name = 'Shodan';
  icon = 'integrations/shodan/icon.png';
  order = 240;
  itypes = {
    ip: 'fetchIp'
  };

  card = {
    title: 'Shodan for %{query}',
    fields: [
      {
        label: 'tags',
        type: 'array',
        join: ', '
      },
      {
        label: 'ports',
        type: 'array',
        join: ', '
      },
      {
        label: 'Service/Port info',
        type: 'table',
        field: 'data',
        fields: [
          {
            label: 'product'
          },
          {
            label: 'port'
          },
          {
            label: 'title',
            field: 'http.title'
          },
          {
            label: 'server',
            field: 'http.server'
          },
          {
            label: 'module',
            field: '_shodan.module'
          },
          {
            label: 'data'
          },
          {
            label: 'html',
            field: 'http.html'
          }
        ]
      },
      {
        label: 'Certificates',
        type: 'table',
        field: 'data',
        fields: [
          {
            label: 'issued',
            field: 'ssl.cert.issued',
            type: 'date'
          },
          {
            label: 'expires',
            field: 'ssl.cert.expires',
            type: 'date'
          },
          {
            label: 'sha1',
            field: 'ssl.cert.fingerprint.sha1'
          },
          {
            label: 'issuer',
            field: 'ssl.cert.issuer',
            type: 'json'
          }
        ]
      },
      'region_code',
      {
        label: 'domains',
        type: 'array',
        join: ', '
      },
      {
        label: 'hostnames',
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

      response.data._count = response.data.data.length;
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
