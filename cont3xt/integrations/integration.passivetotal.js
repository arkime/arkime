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

class PassiveTotalIntegration extends Integration {
  name = 'PassiveTotal';
  itypes = {
    domain: 'fetchDomain',
    ip: 'fetchIp'
  };

  // Default cacheTimeout 24 hours
  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchDomain (user, domain) {
    try {
      const puser = this.getUserConfig(user, 'PassiveTotalUser');
      const pkey = this.getUserConfig(user, 'PassiveTotalKey');
      if (!puser || !pkey) {
        return undefined;
      }

      const passiveTotalWhois = axios.get('https://api.passivetotal.org/v2/whois', {
        params: {
          query: domain,
          history: false
        },
        auth: {
          username: puser,
          password: pkey
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      const passiveTotalSubDomainsResult = axios.get('https://api.passivetotal.org/v2/enrichment/subdomains', {
        params: {
          query: domain
        },
        auth: {
          username: puser,
          password: pkey
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      return { whois: (await passiveTotalWhois).data, subDomains: (await passiveTotalSubDomainsResult).data };
    } catch (err) {
      console.log(this.name, domain, err);
      return null;
    }
  }

  async fetchIp (user, ip) {
    try {
      const puser = this.getUserConfig(user, 'PassiveTotalUser');
      const pkey = this.getUserConfig(user, 'PassiveTotalKey');
      if (!puser || !pkey) {
        return undefined;
      }

      const passiveTotalPassiveDNS = await axios.get('https://api.passivetotal.org/v2/dns/passive', {
        params: {
          query: ip
        },
        auth: {
          username: puser,
          password: pkey
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      return passiveTotalPassiveDNS.data;
    } catch (err) {
      console.log(this.name, ip, err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new PassiveTotalIntegration();
