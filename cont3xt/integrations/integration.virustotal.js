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

class VirusTotalIntegration extends Integration {
  name = 'VirusTotal';
  itypes = {
    domain: 'fetchDomain',
    ip: 'fetchIp',
    hash: 'fetchHash'
  };

  // Default cacheTimeout 24 hours
  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchDomain (user, domain) {
    try {
      const key = this.getUserConfig(user, 'VirusTotalKey');
      if (!key) {
        return undefined;
      }

      const virusTotalRes = await axios.get('https://www.virustotal.com/vtapi/v2/domain/report', {
        params: {
          apikey: key,
          domain: domain
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      return virusTotalRes.data;
    } catch (err) {
      console.log(this.name, domain, err);
      return null;
    }
  }

  async fetchIp (user, ip) {
    try {
      const key = this.getUserConfig(user, 'VirusTotalKey');
      if (!key) {
        return undefined;
      }

      const virusTotalRes = await axios.get('https://www.virustotal.com/vtapi/v2/ip-address/report', {
        params: {
          apikey: key,
          ip: ip
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      return virusTotalRes.data;
    } catch (err) {
      console.log(this.name, ip, err);
      return null;
    }
  }

  async fetchHash (user, hash) {
    try {
      const key = this.getUserConfig(user, 'VirusTotalKey');
      if (!key) {
        return undefined;
      }

      const virusTotalRes = await axios.get('https://www.virustotal.com/vtapi/v2/file/report', {
        params: {
          apikey: key,
          resource: hash
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      return virusTotalRes.data;
    } catch (err) {
      console.log(this.name, hash, err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new VirusTotalIntegration();
