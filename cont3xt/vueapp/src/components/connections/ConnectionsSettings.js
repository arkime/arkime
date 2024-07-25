// TODO: toby - TODO
/**
 * @typedef {Object} ConnectionsSettingsEntry
 * @property {boolean} he
 */

export const linkTypes = {
  domain: {
    DOMAIN_TO_REGISTRAR: {
      name: '-> registrar'
    },
    DOMAIN_TO_DNS_A: {
      name: '-> A records'
    },
    DOMAIN_TO_DNS_AAAA: {
      name: '-> AAAA records'
    },
    DOMAIN_TO_PT_DNS: {
      name: '-> PT DNS records'
    },
    DOMAIN_TO_PT_SUBDOMAINS: {
      name: '-> PT Subdomains'
    },
    DOMAIN_TO_HODI_CLUSTER: {
      name: '-> HODI Cluster'
    },
    DOMAIN_TO_ARKIME: {
      name: '-> Arkime'
    }
  },
  ip: {
    IP_TO_ASN: {
      name: '-> ASN'
    },
    IP_TO_PT_DNS: {
      name: '-> PT DNS'
    },
    IP_TO_CENSYS_PORT: {
      name: '-> Censys Port'
    },
    IP_TO_SHODAN_PORT: {
      name: '-> Shodan Port'
    }
  },
  other: {
    ASN_TO_ASNAME: {
      name: 'ASN -> ASNAME'
    }
  }
};
// .map(linkName => [linkName, linkName]);

/** @type {ConnectionsSettingsEntry[]} */
const connectionsSettingsShape = [
  {
    category: 'PT DNS',
    // integration: 'PT DNS',
    settings: [
      { type: 'PT_DNS_TIME' }
    ]
  }
];
