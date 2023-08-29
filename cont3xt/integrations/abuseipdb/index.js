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

class AbuseIPDBIntegration extends Integration {
  name = 'AbuseIPDB';
  icon = 'integrations/abuseipdb/icon.png';
  order = 240;
  itypes = {
    ip: 'fetchIp'
  };

  card = {
    title: 'AbuseIPDB for %{query}',
    fields: [
      'usageType',
      'isp',
      'abuseConfidenceScore',
      'isWhitelisted',
      'countryCode',
      'countryName',
      'domain',
      {
        label: 'hostnames',
        type: 'array'
      },
      'totalReports',
      'numDistinctUsers',
      {
        label: 'lastReportedAt',
        type: 'date'
      },
      {
        label: 'Reports:',
        field: 'reports',
        type: 'table',
        fields: [
          {
            label: 'Report Date',
            field: 'reportedAt',
            type: 'date'
          },
          {
            label: 'Comment',
            field: 'comment'
          },
          {
            label: 'Category',
            field: 'categories'
          },
          {
            label: 'Reported From',
            field: 'reporterCountryName'
          },
          {
            label: 'ReporterID',
            field: 'reporterId'
          }
        ]
      }
    ]
  };

  homePage = 'https://www.abuseipdb.com/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    },
    key: {
      help: 'Your AbuseIPDB api key',
      password: true,
      required: true
    }
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchIp (user, ip) {
    try {
      const key = this.getUserConfig(user, 'key');
      if (!key) {
        return undefined;
      }

      const response = await axios.get(`https://api.abuseipdb.com/api/v2/check?key=${key}&verbose=true&ipAddress=${encodeURIComponent(ip)}`, {
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      response.data.data._cont3xt = { count: response.data.data.totalReports };
      return response.data.data;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, ip, err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new AbuseIPDBIntegration();
