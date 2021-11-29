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

class AlienVaultOTXIntegration extends Integration {
  name = 'AlienVaultOTX';
  icon = 'integrations/alienvaultotx/icon.png';
  order = 300;
  cacheTimeout = '1w';
  itypes = {
    ip: 'fetchIp',
    domain: 'fetchDomain',
    hash: 'fetchHash',
    url: 'fetchHash'
  };

  userSettings = {
    AlienVaultOTXKey: {
      help: 'Your Alien Vault OTX Key'
    }
  }

  card = {
    title: 'Alien Vault OTX for %{query}',
    fields: [
    ]
  }

  constructor () {
    super();

    Integration.register(this);
  }

  async fetch (user, type, query) {
    try {
      const key = this.getUserConfig(user, 'AlienVaultOTXKey');
      if (!key) {
        return undefined;
      }

      const base = `https://otx.alienvault.com/api/v1/indicators/${type}/${query}`;

      const result = {};

      result.general = (await axios.get(`${base}/general`, {
        headers: {
          'User-Agent': this.userAgent(),
          'X-OTX-API-KEY': key
        }
      })).data;

      for (const section of result.general.sections) {
        if (section === 'nids_list') { continue; }
        result[section] = (await axios.get(`${base}/${section}`, {
          headers: {
            'User-Agent': this.userAgent(),
            'X-OTX-API-KEY': key
          }
        })).data;
      }

      result._count = 0;
      result._count += result.general?.pulse_info?.count ?? 0;
      result._count += result.malware?.count ?? 0;
      if (result._count > 0) {
        result._severity = 'high';
      }
      return result;
    } catch (err) {
      if (err?.response?.status === 400) { return Integration.NoResult; }

      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, type, query, err);
      return null;
    }
  }

  fetchIp (user, ip) {
    return this.fetch(user, ip.includes('.') ? 'IPv4' : 'IPv6', ip);
  }

  fetchDomain (user, domain) {
    return this.fetch(user, 'domain', domain);
  }

  fetchHash (user, hash) {
    return this.fetch(user, 'file', hash);
  }

  fetchUrl (user, url) {
    return this.fetch(user, 'url', url);
  }
}

// eslint-disable-next-line no-new
new AlienVaultOTXIntegration();
