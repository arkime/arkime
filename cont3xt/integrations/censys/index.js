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
const { SearchClient } = require('@censys/node');

class CensysIntegration extends Integration {
  name = 'Censys';
  icon = 'integrations/censys/icon.png';
  order = 220;
  itypes = {
    ip: 'fetchIp'
  };

  homePage = 'https://www.censys.io/'
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    },
    id: {
      help: 'Your censys api id',
      required: true
    },
    secret: {
      help: 'Your censys api secret',
      password: true,
      required: true
    }
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchIp (user, ip) {
    const id = this.getUserConfig(user, this.name, 'id');
    const secret = this.getUserConfig(user, this.name, 'secret');
    if (!id || !secret) {
      return undefined;
    }

    try {
      const c = new SearchClient({
        apiId: id,
        apiSecret: secret
      });

      const fields = [
        'ip',
        'location',
        'protocols',
        'updated_at',
        '443.https.get.title',
        '443.https.get.headers.server',
        '443.https.get.headers.x_powered_by',
        '443.https.get.metadata.description',
        '443.https.tls.certificate.parsed.subject_dn',
        '443.https.tls.certificate.parsed.names',
        '443.https.tls.certificate.parsed.subject.common_name'
      ];

      const query2 = c.v1.ipv4.search(ip, fields);

      let censysData = {};

      for await (const page of query2) {
        censysData = { ...censysData, ...page };
      }

      /* { //TODO: account for balance!
          c.v1.ipv4.account().then(res => {

          });
      } */

      censysData._count = 1;
      return censysData;
    } catch (err) {
      console.log(this.name, ip, err);
      return undefined;
    }
  }
}

// eslint-disable-next-line no-new
new CensysIntegration();
