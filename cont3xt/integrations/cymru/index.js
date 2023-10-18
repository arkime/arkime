/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class CymruIntegration extends Integration {
  name = 'Cymru';
  icon = 'integrations/cymru/icon.png';
  order = 520;
  itypes = {
    hash: 'fetchHash'
  };

  card = {
    title: 'Cymru for %{query}',
    fields: [
      {
        field: 'lastSeen',
        label: 'Last Seen',
        type: 'seconds'
      }
    ]
  };

  cacheTimeout = '1h';

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchHash (user, hash) {
    try {
      const instance = axios.create({
        headers: { Accept: 'application/dns-json' }
      });

      const response = await instance.get(`https://cloudflare-dns.com/dns-query?name=${hash}.hash.cymru.com&type=TXT`);
      if (response.data.Status === 3) {
        return Integration.NoResult;
      }
      const str = response.data.Answer[0].data;
      const parts = str.slice(1, str.length - 1).split(' ');
      const result = {
        lastSeen: parseInt(parts[0]),
        _cont3xt: { count: parseInt(parts[1]) }
      };

      if (result._cont3xt.count > 0) {
        result._cont3xt.severity = 'high';
      }
      return result;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, hash, err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new CymruIntegration();
