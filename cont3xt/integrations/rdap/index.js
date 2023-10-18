/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class RDAPIntegration extends Integration {
  name = 'RDAP';
  itypes = {
    ip: 'fetchIp'
  };

  tidbits = {
    order: 100,
    fields: [
      {
        tooltip: 'link',
        field: 'link',
        postProcess: 'baseRIR',
        display: 'cont3xtCopyLink'
      },
      {
        tooltip: 'name',
        field: 'name'
      }
    ]
  };

  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
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
