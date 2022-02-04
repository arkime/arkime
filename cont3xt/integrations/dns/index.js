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

  addMoreIntegrations (itype, response, addMore) {
    if (itype === 'domain' && this.name === 'DNS' && response?.A?.Status === 0 && Array.isArray(response?.A?.Answer)) {
      for (const answer of response.A.Answer) {
        addMore(answer.data, 'ip');
      }
    }
    if (itype === 'domain' && this.name === 'DNS' && response?.AAAA?.Status === 0 && Array.isArray(response?.AAAA?.Answer)) {
      for (const answer of response.AAAA.Answer) {
        addMore(answer.data, 'ip');
      }
    }
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
      queries.DMARC = instance.get(`https://cloudflare-dns.com/dns-query?name=_dmarc.${domain}&type=TXT`);
      queries.BIMI = instance.get(`https://cloudflare-dns.com/dns-query?name=default._bimi.${domain}&type=TXT`);

      const result = {};
      // Wait for them to finish
      for (const query in queries) {
        const data = (await queries[query]).data;
        result[query] = { Status: data.Status };
        if (data.Answer) {
          result[query].Answer = data.Answer;
        }
      }

      if (result?.CAA?.Answer) {
        result.CAA.Answer.forEach(item => {
          const data = item.data.replace(/ /g, '');
          const len = parseInt(`0x${data.slice(6, 8)}`);
          item.data = `${parseInt(data.slice(4, 6))} ${Buffer.from(data.slice(8, 8 + len * 2), 'hex').toString()} ${Buffer.from(data.slice(8 + len * 2), 'hex').toString()}`;
        });
      }

      return result;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, domain, err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new DNSIntegration();
