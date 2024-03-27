/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class AlienVaultOTXIntegration extends Integration {
  name = 'AlienVaultOTX';
  icon = 'integrations/alienvaultotx/icon.png';
  order = 13000;
  cacheTimeout = '1w';
  itypes = {
    ip: 'fetchIp',
    domain: 'fetchDomain',
    hash: 'fetchHash',
    url: 'fetchHash'
  };

  homePage = 'https://otx.alienvault.com/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    },
    key: {
      help: 'Your Alien Vault OTX Key',
      password: true,
      required: true
    }
  };

  card = {
    title: 'AlienVault OTX for %{query}',
    searchUrls: [{
      url: 'https://otx.alienvault.com/indicator/ip/%{query}',
      itypes: ['ip'],
      name: 'Search AlienVault OTX for %{query}'
    }, {
      url: 'https://otx.alienvault.com/indicator/domain/%{query}',
      itypes: ['domain'],
      name: 'Search AlienVault OTX for %{query}'
    }, {
      url: 'https://otx.alienvault.com/indicator/hostname/%{query}',
      itypes: ['url'],
      name: 'Search AlienVault OTX for Hostname: %{query}'
    }, {
      url: 'https://otx.alienvault.com/indicator/url/%{query}',
      itypes: ['url'],
      name: 'Search AlienVault OTX for URL: %{query}'
    }, {
      url: 'https://otx.alienvault.com/indicator/file/%{query}',
      itypes: ['hash'],
      name: 'Search AlienVault OTX for URL: %{query}'
    }],
    fields: [
      {
        label: 'Validation',
        field: 'general.validation',
        type: 'table',
        fields: [
          { label: 'Name', field: 'name' }, { label: 'Source', field: 'source' }, { label: 'Message', field: 'message' }
        ]
      },
      {
        label: 'Pulse count:',
        field: 'general.pulse_info.count',
        type: 'string'
      },
      {
        label: 'Malware count:',
        field: 'malware.count',
        type: 'string'
      },
      {
        label: 'Reputation:',
        field: 'general.reputation',
        type: 'string'
      },
      {
        label: 'Other Reputation:',
        field: 'reputation.reputation.activities',
        type: 'table',
        fields: [
          { label: 'Type', field: 'name' }, { label: 'Data', field: 'data_key' }, { label: 'Earliest Observation', field: 'first_date', type: 'date' }, { label: 'Latest Observation', field: 'last_date', type: 'date' }, { label: 'Alt Data', field: 'data' }
        ]
      },
      {
        label: 'Pulses:',
        field: 'general.pulse_info.pulses',
        type: 'table',
        fields: [
          { label: 'Name', field: 'name' }, { label: 'Description', field: 'description' }, { label: 'Created', field: 'created', type: 'date' }, { label: 'Modified', field: 'modified', type: 'date' }, { label: 'Tags', field: 'tags', type: 'array', pivot: 'true' }, { label: 'Targeted Countries', field: 'targeted_countries', type: 'array' }, { label: 'Industries', field: 'industries', type: 'array' }, { label: 'Malware', field: 'malware_families', type: 'table', fields: [{ label: 'Name', field: 'display_name', pivot: 'true' }] }, { label: 'ID', field: 'id' }
        ]
      },
      {
        label: 'Malware:',
        field: 'malware.data',
        type: 'table',
        fields: [
          { label: 'Hash', field: 'hash', pivot: 'true' }, { label: 'Detections', field: 'detections' }, { label: 'Date', field: 'date', type: 'date' }
        ]
      },
      {
        label: 'URL List',
        field: 'url_list.url_list',
        type: 'table',
        fields: [
          { label: 'URL', field: 'url', pivot: 'true' }, { label: 'Status', field: 'httpcode' }, { label: 'GSB', field: 'gsb' }, { label: 'Date', field: 'date', type: 'date' }
        ]
      },
      {
        label: 'PDNS',
        field: 'passive_dns.passive_dns',
        type: 'table',
        fields: [
          { label: 'Type', field: 'record_type' }, { label: 'Hostname', field: 'hostname', pivot: 'true' }, { label: 'address', field: 'address' }, { label: 'first_seen', field: 'first', type: 'date' }, { label: 'last_seen', field: 'last', type: 'date' }
        ]
      },
      {
        label: 'Whois - Related Domains',
        field: 'whois.related',
        type: 'table',
        fields: [
          { label: 'Domain', field: 'domain' }, { label: 'Related by', field: 'related' }, { label: 'Related type', field: 'related_type' }
        ]
      },
      {
        label: 'Samples',
        field: 'analysis',
        type: 'table',
        fields: [
          { label: 'TLP', field: 'analysis.metadata.tlp' }, { label: 'Class', field: 'analysis.info.results.file_class' }, { label: 'Size', field: 'analysis.info.results.filesize' }, { label: 'Status', field: 'analysis.plugins.adobemalwareclassifier.results.alerts', type: 'array' }, { label: 'PEHash', field: 'analysis.plugins.pe32info.results.pehash' }, { label: 'IMPHash', field: 'analysis.plugins.pe32info.results.imphash' }, { label: 'CompileTime', field: 'analysis.plugins.exiftool.results.Time_Stamp' }, { label: 'Date', field: 'analysis.datetime_int', type: 'date' }
        ]
      }
    ]
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetch (user, type, query) {
    try {
      const key = this.getUserConfig(user, 'key');
      if (!key) {
        return undefined;
      }

      const base = `https://otx.alienvault.com/api/v1/indicators/${type}/${query}`;

      const result = {};

      result.general = (await axios.get(`${base}/general`, {
        headers: {
          'User-Agent': this.userAgent(),
          'X-OTX-API-KEY': key
        }
      })).data;

      for (const section of result.general.sections) {
        if (section === 'nids_list') { continue; }
        result[section] = (await axios.get(`${base}/${section}`, {
          headers: {
            'User-Agent': this.userAgent(),
            'X-OTX-API-KEY': key
          }
        })).data;
      }

      result._cont3xt = { count: 0 };
      result._cont3xt.count += result.general?.pulse_info?.count ?? 0;
      result._cont3xt.count += result.malware?.count ?? 0;
      if (result._cont3xt.count > 0) {
        result._cont3xt.severity = 'high';
      }
      return result;
    } catch (err) {
      if (err?.response?.status === 400) { return Integration.NoResult; }

      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, type, query, err);
      return null;
    }
  }

  fetchIp (user, ip) {
    return this.fetch(user, ip.includes('.') ? 'IPv4' : 'IPv6', ip);
  }

  fetchDomain (user, domain) {
    return this.fetch(user, 'domain', domain);
  }

  fetchHash (user, hash) {
    return this.fetch(user, 'file', hash);
  }

  fetchUrl (user, url) {
    return this.fetch(user, 'url', url);
  }
}

// eslint-disable-next-line no-new
new AlienVaultOTXIntegration();
