/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const maxmind = require('maxmind');
const fs = require('fs');
const ArkimeConfig = require('../../../common/arkimeConfig');

class MaxmindIntegration extends Integration {
  name = 'Maxmind';
  cacheable = false;
  itypes = {
    ip: 'fetchIp'
  };

  tidbits = {
    order: 200,
    fields: [
      {
        tooltip: 'asn',
        field: 'asn.autonomous_system_number',
        type: 'badge',
        template: 'AS<value>'
      },
      {
        tooltip: 'organization',
        field: 'asn.autonomous_system_organization',
        type: 'badge'
      },
      {
        field: 'country.country.iso_code',
        type: 'badge',
        postProcess: 'countryEmoji',
        tooltipTemplate: '<data.country.country.names.en> (<value>)'
      }
    ]
  };

  constructor () {
    super();

    const asnPaths = ArkimeConfig.getFullArray(['Maxmind', 'cont3xt'], 'geoLite2ASN', '/var/lib/GeoIP/GeoLite2-ASN.mmdb;/usr/share/GeoIP/GeoLite2-ASN.mmdb');
    const countryPaths = ArkimeConfig.getFullArray(['Maxmind', 'cont3xt'], 'geoLite2Country', '/var/lib/GeoIP/GeoLite2-Country.mmdb;/usr/share/GeoIP/GeoLite2-Country.mmdb');

    if (asnPaths.length === 0 && countryPaths.length === 0) {
      return;
    }

    if (asnPaths.length) {
      for (const path of asnPaths) {
        if (fs.existsSync(path)) {
          maxmind.open(path)
            .then(lookup => { this.asnLookup = lookup; })
            .catch(err => console.log(err));
          break;
        }
      }
    }

    if (countryPaths.length) {
      for (const path of countryPaths) {
        if (fs.existsSync(path)) {
          maxmind.open(path)
            .then(lookup => { this.countryLookup = lookup; })
            .catch(err => console.log(err));
          break;
        }
      }
    }

    Integration.register(this);
  }

  async fetchIp (user, ip) {
    try {
      const result = {};
      if (this.asnLookup) {
        result.asn = this.asnLookup.get(ip);
      }
      if (this.countryLookup) {
        result.country = this.countryLookup.get(ip);
      }

      return result;
    } catch (err) {
      console.log(this.name, ip, err);
      return undefined;
    }
  }
}

// eslint-disable-next-line no-new
new MaxmindIntegration();
