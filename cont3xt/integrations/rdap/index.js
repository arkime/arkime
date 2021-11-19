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

class RDAPIntegration extends Integration {
  name = 'RDAP';
  itypes = {
    ip: 'fetchIp'
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchIp (user, ip) {
    try {
      const res = await axios.get(`https://rdap.db.ripe.net/ip/${ip}`, {
        maxRedirects: 5,
        validateStatus: false,
        headers: {
          Accept: 'application/json',
          'User-Agent': this.userAgent()
        }
      });
      if (res.status === 200) {
        return { name: res.data.name, link: res.data.links[0].value };
      } else {
        return null;
      }
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, ip, err);
      return undefined;
    }
  }
}

// eslint-disable-next-line no-new
new RDAPIntegration();
