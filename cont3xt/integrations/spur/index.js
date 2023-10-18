/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class SpurIntegration extends Integration {
  name = 'Spur';
  icon = 'integrations/spur/icon.png';
  order = 200;
  itypes = {
    ip: 'fetchIp'
  };

  card = {
    title: 'Spur for %{query}',
    fields: [
      {
        label: 'as',
        type: 'json'
      },
      {
        label: 'geoLite',
        type: 'json'
      },
      {
        label: 'geoPrecision',
        type: 'json'
      },
      'infrastructure',
      {
        label: 'assignment',
        type: 'json'
      },
      {
        label: 'deviceBehaviors',
        type: 'json'
      },
      {
        label: 'proxiedTraffic',
        type: 'json'
      },
      {
        label: 'similarIPs',
        type: 'json'
      },
      {
        label: 'vpnOperators',
        type: 'json'
      },
      {
        label: 'wifi',
        type: 'json'
      },
      {
        label: 'tunnel operators',
        field: 'tunnels',
        fieldRoot: 'operator',
        type: 'array'
      },
      {
        label: 'client proxies',
        field: 'client.proxies',
        type: 'array'
      },
      {
        label: 'client behaviors',
        field: 'client.behaviors',
        type: 'array'
      },
      {
        label: 'risks',
        type: 'array'
      }
    ]
  };

  tidbits = {
    order: 300,
    fields: [
      {
        tooltip: 'infrastructure',
        field: 'infrastructure',
        display: 'badge'
      },
      {
        tooltip: 'tunnel operators',
        field: 'tunnels',
        fieldRoot: 'operator',
        type: 'array',
        display: 'warningGroup'
      },
      {
        tooltip: 'client proxies',
        field: 'client.proxies',
        type: 'array',
        display: 'warningGroup'
      },
      {
        tooltip: 'client behaviors',
        field: 'client.behaviors',
        type: 'array',
        display: 'warningGroup'
      },
      {
        tooltip: 'risks',
        field: 'risks',
        type: 'array',
        display: 'warningGroup'
      }
    ]
  };

  homePage = 'https://spur.us/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    },
    key: {
      help: 'Your spur api key',
      password: true,
      required: true
    }
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchIp (user, ip) {
    try {
      const key = this.getUserConfig(user, 'key');
      if (!key) {
        return undefined;
      }

      const response = await axios.get(`https://api.spur.us/v2/context/${ip}`, {
        headers: {
          Token: key,
          'User-Agent': this.userAgent()
        }
      });

      for (const rkey in response.data) {
        if (typeof (response.data) === 'object') {
          if (response.data[rkey].exists === false) {
            delete response.data[rkey];
          } else {
            delete response.data[rkey].exists;
          }
        }
      }

      response.data._cont3xt = { count: 1 };
      return response.data;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, ip, err);
      return undefined;
    }
  }
}

// eslint-disable-next-line no-new
new SpurIntegration();
