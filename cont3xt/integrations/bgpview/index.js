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

class BGPViewIntegration extends Integration {
  name = 'BGPView';
  icon = 'integrations/bgpview/icon.png';
  order = 300;
  cacheTimeout = '1w';
  itypes = {
    ip: 'fetch'
  };

  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  card = {
    title: 'BGPView for %{query}',
    fields: [
      {
        field: 'data',
        type: 'table',
        fields: [
          {
            label: 'IP',
            field: 'ip'
          },
          {
            label: 'PTR',
            field: 'ptr_record'
          }
        ]
      },
      {
        label: 'RIR Allocation',
        field: 'data.rir_allocation',
        type: 'table',
        fields: [
          {
            label: 'RIR Name',
            field: 'rir_name'
          },
          {
            label: 'Prefix',
            field: 'prefix'
          },
          {
            label: 'Date Allocated',
            field: 'date_allocated',
            type: 'date'
          },
          {
            label: 'Allocation Status',
            field: 'allocation_status'
          }
        ]
      },
      {
        label: 'Prefixes',
        field: 'data.prefixes',
        type: 'table',
        fields: [
          {
            label: 'AS#',
            field: 'asn.asn'
          },
          {
            label: 'Prefix',
            field: 'prefix'
          },
          {
            label: 'ASName',
            field: 'asn.name'
          },
          {
            label: 'ASName description',
            field: 'asn.description'
          },
          {
            label: 'Name',
            field: 'name'
          },
          {
            label: 'Description',
            field: 'description'
          }
        ]
      }
    ]
  }

  constructor () {
    super();

    Integration.register(this);
  }

  async fetch (user, query) {
    try {
      const result = await axios.get(`https://api.bgpview.io/ip/${query}`, {
        headers: {
          'User-Agent': this.userAgent()
        }
      });
      return result.data;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, query, err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new BGPViewIntegration();
