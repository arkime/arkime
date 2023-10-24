/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class BGPViewIntegration extends Integration {
  name = 'BGPView';
  icon = 'integrations/bgpview/icon.png';
  order = 340;
  cacheTimeout = '1w';
  itypes = {
    ip: 'fetch'
  };

  homePage = 'https://bgpview.io/';
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
        label: 'Data',
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
  };

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
