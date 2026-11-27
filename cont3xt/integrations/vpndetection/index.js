/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class VPNDetectionIntegration extends Integration {
  name = 'VPNDetection';
  icon = 'integrations/vpndetection/icon.png';
  order = 205;
  itypes = {
    ip: 'fetchIp'
  };

  card = {
    title: 'VPNDetection for %{query}',
    searchUrls: [{
      url: 'https://vpndetection.io/%{query}',
      itypes: ['ip'],
      name: 'Search VPNDetection for %{query}'
    }],
    fields: [
      'is_vpn',
      {
        label: 'vpn provider',
        field: 'vpn.provider_name'
      },
      'is_tor',
      'is_relay',
      {
        label: 'relay provider',
        field: 'relay.provider_name'
      },
      'is_resproxy',
      'is_dcproxy',
      'is_mobproxy',
      'is_pubproxy',
      'is_hosting',
      'is_anycast',
      'hostname',
      {
        label: 'asn',
        type: 'json'
      },
      {
        label: 'location',
        type: 'json'
      }
    ]
  };

  tidbits = {
    order: 250,
    fields: [
      {
        field: 'is_vpn',
        tooltip: 'VPNDetection: VPN',
        postProcess: {
          if: { equals: true },
          then: 'VPN',
          else: undefined
        }
      },
      {
        field: 'is_tor',
        tooltip: 'VPNDetection: Tor exit',
        postProcess: {
          if: { equals: true },
          then: 'TOR',
          else: undefined
        }
      },
      {
        field: 'is_relay',
        tooltip: 'VPNDetection: privacy relay',
        postProcess: {
          if: { equals: true },
          then: 'RELAY',
          else: undefined
        }
      },
      {
        field: 'is_resproxy',
        tooltip: 'VPNDetection: residential proxy',
        postProcess: {
          if: { equals: true },
          then: 'RESIDENTIAL PROXY',
          else: undefined
        }
      },
      {
        field: 'is_dcproxy',
        tooltip: 'VPNDetection: datacenter proxy',
        postProcess: {
          if: { equals: true },
          then: 'DATACENTER PROXY',
          else: undefined
        }
      },
      {
        field: 'is_mobproxy',
        tooltip: 'VPNDetection: mobile proxy',
        postProcess: {
          if: { equals: true },
          then: 'MOBILE PROXY',
          else: undefined
        }
      },
      {
        field: 'is_pubproxy',
        tooltip: 'VPNDetection: public proxy',
        postProcess: {
          if: { equals: true },
          then: 'PUBLIC PROXY',
          else: undefined
        }
      }
    ]
  };

  homePage = 'https://vpndetection.io/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    },
    key: {
      help: 'Your VPNDetection api key (a free key covers VPN, Tor and relays; paid plans add the proxy and hosting flags)',
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

      const response = await axios.get(`https://api.vpndetection.io/${ip}`, {
        headers: {
          Authorization: `Bearer ${key}`,
          'User-Agent': this.userAgent()
        }
      });

      const data = response.data;
      const flags = ['is_vpn', 'is_tor', 'is_relay', 'is_resproxy', 'is_dcproxy', 'is_mobproxy', 'is_pubproxy'];
      data._cont3xt = { count: flags.filter(f => data[f] === true).length };
      return data;
    } catch (err) {
      console.log(this.name, ip, err);
      return null;
    }
  }
}

new VPNDetectionIntegration();
