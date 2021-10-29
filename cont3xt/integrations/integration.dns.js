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

class DNSIntegration extends Integration {
  name = 'DNS';
  itypes = {
    domain: 'fetchDomain'
  };

  // Default cacheTimeout 10 minutes
  cacheTimeout = 10 * 1000;

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchDomain (user, domain) {
    try {
      const instance = axios.create({
        headers: { Accept: 'application/dns-json' }
      });

      const queries = {};

      // Start all the queries in parallel
      for (const query of ['A', 'AAAA', 'NS', 'MX', 'TXT', 'CAA', 'SOA']) {
        queries[query] = instance.get(`https://cloudflare-dns.com/dns-query?name=${domain}&type=${query}`);
      }
      queries.dmarcTXT = instance.get(`https://cloudflare-dns.com/dns-query?name=_dmarc.${domain}&type=TXT`);

      const result = {};
      // Wait for them to finish
      for (const query in queries) {
        const data = (await queries[query]).data;
        result[query] = { Status: data.Status };
        if (data.Answer) {
          result[query].Answer = data.Answer;
        }
      }
      return result;
    } catch (err) {
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new DNSIntegration();
