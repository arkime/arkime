/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class RDAPIPIntegration extends Integration {
  name = 'RDAP IP';
  icon = 'integrations/rdap/icon.png';
  order = 110;
  itypes = {
    ip: 'fetchIp'
  };

  card = {
    title: 'RDAP for %{query}',
    fields: [
      {
        label: 'name',
        field: 'name'
      },
      {
        label: 'type',
        field: 'type'
      },
      {
        label: 'country',
        field: 'country'
      },
      {
        label: 'startAddress',
        field: 'startAddress'
      },
      {
        label: 'endAddress',
        field: 'endAddress'
      },
      {
        label: 'ipVersion',
        field: 'ipVersion'
      },
      {
        label: 'parentHandle',
        field: 'parentHandle'
      },
      {
        label: 'handle',
        field: 'handle'
      },
      {
        label: 'entities',
        field: 'entities',
        type: 'table',
        fields: [
          {
            label: 'handle',
            field: 'handle'
          },
          {
            label: 'roles',
            field: 'roles',
            type: 'array',
            join: ', '
          }
        ]
      },
      {
        label: 'remarks',
        field: 'remarks',
        type: 'table',
        fields: [
          {
            label: 'title',
            field: 'title'
          },
          {
            label: 'description',
            field: 'description',
            type: 'array',
            join: ' '
          }
        ]
      },
      {
        label: 'links',
        field: 'links',
        type: 'table',
        fields: [
          {
            label: 'value',
            field: 'value',
            defang: true
          },
          {
            label: 'rel',
            field: 'rel'
          },
          {
            label: 'type',
            field: 'type'
          }
        ]
      }
    ]
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
        res.data.link = res.data.links?.[0]?.value;
        res.data._cont3xt = { count: 1 };
        return res.data;
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

class RDAPDomainIntegration extends Integration {
  name = 'RDAP Domain';
  icon = 'integrations/rdap/icon.png';
  order = 120;
  itypes = {
    domain: 'fetchDomain'
  };

  card = {
    title: 'RDAP for %{query}',
    fields: [
      {
        label: 'ldhName',
        field: 'ldhName'
      },
      {
        label: 'unicodeName',
        field: 'unicodeName'
      },
      {
        label: 'handle',
        field: 'handle'
      },
      {
        label: 'status',
        field: 'status',
        type: 'array',
        join: ', '
      },
      {
        label: 'nameservers',
        field: 'nameservers',
        type: 'table',
        fields: [
          {
            label: 'ldhName',
            field: 'ldhName'
          }
        ]
      },
      {
        label: 'secureDNS',
        field: 'secureDNS',
        type: 'json'
      },
      {
        label: 'entities',
        field: 'entities',
        type: 'table',
        fields: [
          {
            label: 'handle',
            field: 'handle'
          },
          {
            label: 'roles',
            field: 'roles',
            type: 'array',
            join: ', '
          }
        ]
      },
      {
        label: 'events',
        field: 'events',
        type: 'table',
        fields: [
          {
            label: 'eventAction',
            field: 'eventAction'
          },
          {
            label: 'eventDate',
            field: 'eventDate',
            type: 'date'
          }
        ]
      },
      {
        label: 'remarks',
        field: 'remarks',
        type: 'table',
        fields: [
          {
            label: 'title',
            field: 'title'
          },
          {
            label: 'description',
            field: 'description',
            type: 'array',
            join: ' '
          }
        ]
      },
      {
        label: 'links',
        field: 'links',
        type: 'table',
        fields: [
          {
            label: 'value',
            field: 'value',
            defang: true
          },
          {
            label: 'rel',
            field: 'rel'
          },
          {
            label: 'type',
            field: 'type'
          }
        ]
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

  async fetchDomain (user, domain) {
    try {
      const res = await axios.get(`https://rdap.org/domain/${domain}`, {
        maxRedirects: 5,
        validateStatus: false,
        headers: {
          Accept: 'application/json',
          'User-Agent': this.userAgent()
        }
      });

      if (res.status === 200) {
        res.data.link = res.data.links?.[0]?.value;
        res.data._cont3xt = { count: 1 };
        return res.data;
      } else {
        return null;
      }
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, domain, err);
      return undefined;
    }
  }
}

new RDAPIPIntegration();
new RDAPDomainIntegration();
