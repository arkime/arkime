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

class CensysIntegration extends Integration {
  name = 'Censys';
  icon = 'integrations/censys/icon.png';
  order = 220;
  itypes = {
    ip: 'fetchIp'
  };

  card = {
    title: 'Censys for %{query}',
    fields: [
      {
        label: 'Services',
        field: 'result.services',
        defaultSortField: 'observed_at',
        defaultSortDirection: 'asc',
        type: 'table',
        fields: [
          {
            label: 'service',
            field: 'extended_service_name'
          },
          {
            label: 'port',
            field: 'port'
          },
          {
            label: 'proto',
            field: 'transport_protocol'
          },
          {
            label: 'banner',
            field: 'banner'
          },
          {
            label: 'product',
            field: 'software',
            type: 'array',
            fieldRoot: 'uniform_resource_identifier'
          },
          {
            label: 'observed_at',
            field: 'observed_at',
            type: 'date'
          }
        ]
      },
      {
        label: 'Certificates',
        field: 'result.services',
        fieldRoot: 'tls.certificates.leaf_data',
        type: 'table',
        fields: [
          {
            label: 'names',
            field: 'names',
            type: 'array'
          },
          {
            label: 'subject_dn',
            field: 'subject_dn'
          },
          {
            label: 'issuer_dn',
            field: 'issuer_dn'
          },
          {
            label: 'fingerprint',
            field: 'fingerprint'
          },
          {
            label: 'issuer',
            field: 'issuer.common_name',
            type: 'array'
          }
        ]
      }
    ]
  };

  homePage = 'https://search.censys.io/';
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
    const id = this.getUserConfig(user, 'id');
    const secret = this.getUserConfig(user, 'secret');
    if (!id || !secret) {
      return undefined;
    }

    try {
      const c = await axios.get(`https://search.censys.io/api/v2/hosts/${ip}`, {
        headers: {
          'User-Agent': this.userAgent()
        },
        auth: {
          username: id,
          password: secret
        }
      });

      return c.data;
    } catch (err) {
      console.log(this.name, ip, err);
      return undefined;
    }
  }
}

// eslint-disable-next-line no-new
new CensysIntegration();
