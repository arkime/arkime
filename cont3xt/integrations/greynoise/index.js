/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class GreyNoiseIntegration extends Integration {
  name = 'GreyNoise';
  icon = 'integrations/greynoise/icon.png';
  order = 210;
  itypes = {
    ip: 'fetchIp'
  };

  card = {
    title: 'GreyNoise for %{query}',
    fields: [
      'classification',
      {
        label: 'last_seen',
        field: 'last_seen',
        type: 'date'
      },
      'name',
      {
        label: 'link',
        field: 'link',
        type: 'url'
      },
      'noise',
      'riot'
    ]
  };

  tidbits = {
    order: 400,
    fields: [
      {
        field: 'riot',
        postProcess: {
          if: { equals: true },
          then: 'RIOT',
          else: undefined
        }
      },
      {
        field: 'noise',
        postProcess: {
          if: { equals: true },
          then: 'NOISE',
          else: undefined
        }
      }
    ]
  };

  homePage = 'https://www.greynoise.io/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    },
    key: {
      help: 'Your GreyNoise api key',
      password: true,
      required: false
    }
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchIp (user, ip) {
    try {
      const key = this.getUserConfig(user, 'key');

      const headers = { 'User-Agent': this.userAgent() };
      // use key if provided (optional, allows one to get around the daily rate limit)
      if (key != null) { headers.key = key; }

      const c = await axios.get(`https://api.greynoise.io/v3/community/${ip}`, {
        headers
      });

      // add one to count for riot or noise being true (differentiate from NoResult button)
      let count = 0;
      if (c.data.riot) { count++; }
      if (c.data.noise) { count++; }
      c.data._cont3xt = { count };
      return c.data;
    } catch (err) {
      if (err?.response?.status === 404) {
        return Integration.NoResult; // IP not found in riot dataset or noise
      }

      return undefined;
    }
  }
}

// eslint-disable-next-line no-new
new GreyNoiseIntegration();
