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
    const CNAME = [];
    if (itype === 'domain' && this.name === 'DNS' && response?.A?.Status === 0 && Array.isArray(response?.A?.Answer)) {
      const A = [];
      for (const answer of response.A.Answer) {
        if (answer.type === 1) {
          addMore(answer.data, 'ip');
          A.push(answer);
        } else if (answer.type === 5) {
          if (!CNAME.find(element => element.data === answer.data)) {
            CNAME.push(answer);
          }
        }
      }
      response.A.Answer = A;
    }
    if (itype === 'domain' && this.name === 'DNS' && response?.AAAA?.Status === 0 && Array.isArray(response?.AAAA?.Answer)) {
      const AAAA = [];
      for (const answer of response.AAAA.Answer) {
        if (answer.type === 28) {
          addMore(answer.data, 'ip');
          AAAA.push(answer);
        } else if (answer.type === 5) {
          if (!CNAME.find(element => element.data === answer.data)) {
            CNAME.push(answer);
          }
        }
      }
      response.AAAA.Answer = AAAA;
    }

    if (CNAME.length > 0) {
      response.CNAME = { Status: 0, Answer: CNAME };
    }
  }

  async fetchDomain (user, domain) {
    const error = (err) => {
      if (err?.response?.status === 400) {
        console.log(this.name, domain, err?.request?.path, err.toString());
      } else {
        console.log(this.name, domain, err);
      }
    };

    try {
      const instance = axios.create({
        headers: { Accept: 'application/dns-json' }
      });

      const queries = {};

      // Start all the queries in parallel
      for (const query of ['A', 'AAAA', 'NS', 'MX', 'TXT', 'CAA', 'SOA']) {
        queries[query] = instance.get(`https://cloudflare-dns.com/dns-query?name=${domain}&type=${query}`).catch(error);
      }
      queries.DMARC = instance.get(`https://cloudflare-dns.com/dns-query?name=_dmarc.${domain}&type=TXT`).catch(error);
      queries.BIMI = instance.get(`https://cloudflare-dns.com/dns-query?name=default._bimi.${domain}&type=TXT`).catch(error);

      const result = {};
      // Wait for them to finish
      for (const query in queries) {
        const data = (await queries[query])?.data;
        if (!data) { continue; }
        result[query] = { Status: data.Status };
        if (data.Answer) {
          result[query].Answer = data.Answer;

          if (query === 'TXT') {
            // split out SPF records into their own section
            const spfAnswer = [];
            const txtAnswer = [];
            for (const txtEntry of data.Answer) {
              const isSpf = (txtEntry.data.includes('v=spf1') || txtEntry.data.includes('spf2'));
              const targetList = isSpf ? spfAnswer : txtAnswer;
              targetList.push(txtEntry);
            }
            result.SPF = { Status: data.Status, Answer: spfAnswer };

            // group domain/site-verification at the top, sorted alphabetically
            const groupPrecedence = (txtEntry) => {
              if (txtEntry.data.includes('domain-verification')) { return 0; }
              if (txtEntry.data.includes('site-verification')) { return 1; }
              return 2;
            };
            const sortTxtEntry = (a, b) => {
              const groupDiff = groupPrecedence(a) - groupPrecedence(b);
              if (groupDiff !== 0) { return groupDiff; } // sort by group first
              return a.data.localeCompare(b.data); // fall back to alphabetical within groups
            };
            txtAnswer.sort(sortTxtEntry);
            result[query].Answer = txtAnswer; // update TXT Answer
          }
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
