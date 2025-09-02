/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class Quad9 extends Integration {
  name = 'Quad9';
  itypes = {
    ip: 'fetch',
    domain: 'fetch'
  };

  homePage = 'https://dns.quad9.net/dns-query';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  tidbits = {
    order: 200,
    fields: [
      {
        field: 'blocked',
        postProcess: {
          if: { equals: true },
          then: 'Quad9',
          else: undefined
        },
        tooltipTemplate: 'Blocked by: <data.blocked_by>'
      }
    ]
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetch (user, query) {
    try {
      const result = await axios.get(`https://api.quad9.net/search/${query}`, {
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

 
new Quad9();
