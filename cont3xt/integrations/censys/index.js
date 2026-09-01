/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
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
    searchUrls: [{
      url: 'https://platform.censys.io/hosts/%{query}',
      itypes: ['ip'],
      name: 'Search Censys for Host: %{query}'
    }],
    fields: [
      {
        label: 'Services',
        field: 'result.resource.services',
        defaultSortField: 'scan_time',
        defaultSortDirection: 'asc',
        type: 'table',
        fields: [
          {
            label: 'service',
            field: 'protocol'
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
            fieldRoot: 'cpe'
          },
          {
            label: 'scan_time',
            field: 'scan_time',
            type: 'date'
          }
        ]
      },
      {
        label: 'Certificates',
        field: 'result.resource.services',
        fieldRoot: 'cert',
        type: 'table',
        fields: [
          {
            label: 'names',
            field: 'names',
            type: 'array'
          },
          {
            label: 'subject_dn',
            field: 'parsed.subject_dn'
          },
          {
            label: 'issuer_dn',
            field: 'parsed.issuer_dn'
          },
          {
            label: 'fingerprint',
            field: 'fingerprint_sha256'
          },
          {
            label: 'issuer',
            field: 'parsed.issuer.common_name',
            type: 'array'
          }
        ]
      }
    ]
  };

  homePage = 'https://platform.censys.io/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    },
    token: {
      help: 'Your censys personal access token (PAT), created at https://accounts.censys.io/settings/personal-access-tokens',
      password: true,
      required: true
    },
    organizationId: {
      help: 'Your censys organization id, optional, free accounts do not have one'
    }
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchIp (user, ip) {
    const token = this.getUserConfig(user, 'token');
    if (!token) {
      return undefined;
    }

    try {
      const organizationId = this.getUserConfig(user, 'organizationId');

      const c = await axios.get(`https://api.platform.censys.io/v3/global/asset/host/${encodeURIComponent(ip)}`, {
        params: organizationId ? { organization_id: organizationId } : undefined,
        headers: {
          Accept: 'application/vnd.censys.api.v3.host.v1+json',
          Authorization: `Bearer ${token}`,
          'User-Agent': this.userAgent()
        }
      });

      return c.data;
    } catch (err) {
      console.log(this.name, ip, err);
      return undefined;
    }
  }
}

new CensysIntegration();
