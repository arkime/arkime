/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class VirusTotalIntegration extends Integration {
  name = 'VirusTotal';
  icon = 'integrations/virustotal/icon.png';
  noStats = true;
  itypes = {
  };

  settings = {
    key: {
      help: 'Your virustotal api key',
      password: true,
      required: true
    }
  };

  // Default cacheTimeout 24 hours
  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();

    Integration.register(this);
  }
}

class VirusTotalDomainIntegration extends Integration {
  name = 'VT Domain';
  icon = 'integrations/virustotal/icon.png';
  order = 600;
  configName = 'VirusTotal';
  itypes = {
    domain: 'fetchDomain'
  };

  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  card = {
    title: 'VirusTotal Domain for %{query}',
    searchUrls: [{
      url: 'https://www.virustotal.com/gui/domain/%{query}',
      itypes: ['domain'],
      name: 'Search VirusTotal for %{query}'
    }, {
      url: 'https://www.virustotal.com/gui/ip-address/%{query}',
      itypes: ['ip'],
      name: 'Search VirusTotal for %{query}'
    }, {
      url: 'https://www.virustotal.com/gui/file/%{query}/detection',
      itypes: ['hash'],
      name: 'Search VirusTotal for %{query}'
    }],
    fields: [
      'asn',
      'as_owner',
      'country',
      'verbose_msg',
      'Alexa category',
      'Alexa rank',
      'Alexa domain info',
      'Webutation domain info',
      'BitDefender category',
      'Sophos category',
      'Forcepoint ThreatSeeker category',
      'BitDefender domain info',
      {
        label: 'detected_urls',
        type: 'table',
        fields: [
          'positives',
          'total',
          {
            label: 'url',
            defang: true
          },
          'scan_date'
        ]
      },
      {
        label: 'undetected_urls',
        type: 'table',
        fields: [
          'positives',
          'total',
          {
            label: 'url',
            defang: true
          },
          {
            label: 'sha256',
            field: 'sha256',
            pivot: 'true'
          },
          'scan_date'
        ]
      },
      makeSamples('detected_downloaded_samples'),
      makeSamples('undetected_downloaded_samples'),
      makeSamples('detected_referrer_samples'),
      makeSamples('undetected_referrer_samples'),
      makeSamples('detected_communicating_samples'),
      makeSamples('undetected_communicating_samples'),
      {
        label: 'resolutions',
        type: 'table',
        fields: [
          'ip_address',
          'last_resolved'
        ]
      },
      {
        label: 'Pcaps',
        field: 'pcaps',
        type: 'array'
      }
    ]
  };

  tidbits = {
    fields: [
      {
        field: 'whois',
        postProcess: [{ restOfLineFollowing: 'Creation Date: ' }, 'removeTime'],
        purpose: 'registered',
        precedence: 1,
        order: 100
      },
      {
        field: 'whois',
        postProcess: { restOfLineFollowing: 'Registrar: ' },
        purpose: 'registrar',
        precedence: 1,
        order: 110
      }
    ]
  };

  // Default cacheTimeout 24 hours
  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchDomain (user, domain) {
    try {
      const key = this.getUserConfig(user, 'key');
      if (!key) {
        return undefined;
      }
      const response = await axios.get('https://www.virustotal.com/vtapi/v2/domain/report', {
        params: {
          apikey: key,
          domain
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      // Fix undetected_urls
      if (response.data.undetected_urls !== undefined && Array.isArray(response.data.undetected_urls[0])) {
        const uus = [];
        for (const uu of response.data.undetected_urls) {
          uus.push({
            url: uu[0],
            sha256: uu[1],
            positives: uu[2],
            total: uu[3],
            scan_date: uu[4]
          });
        }
        response.data.undetected_urls = uus;
      }

      response.data._cont3xt = { count: 1 };
      return response.data;
    } catch (err) {
      console.log(this.name, domain, err);
      return null;
    }
  }
}

class VirusTotalIPIntegration extends Integration {
  name = 'VT IP';
  icon = 'integrations/virustotal/icon.png';
  order = 620;
  configName = 'VirusTotal';
  itypes = {
    ip: 'fetchIp'
  };

  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  card = {
    title: 'VirusTotal IP for %{query}',
    fields: [
      'asn',
      'as_owner',
      'country',
      'verbose_msg',
      'Alexa category',
      'Alexa rank',
      'Alexa domain info',
      'Webutation domain info',
      'BitDefender category',
      'Forcepoint ThreatSeeker category',
      'BitDefender domain info',
      {
        label: 'detected_urls',
        type: 'table',
        fields: [
          'positives',
          'total',
          {
            label: 'url',
            defang: true
          },
          'scan_date'
        ]
      },
      {
        label: 'undetected_urls',
        type: 'table',
        fields: [
          'positives',
          'total',
          {
            label: 'url',
            defang: true
          },
          {
            label: 'sha256',
            field: 'sha256',
            pivot: 'true'
          },
          'scan_date'
        ]
      },
      makeSamples('detected_downloaded_samples'),
      makeSamples('undetected_downloaded_samples'),
      makeSamples('detected_referrer_samples'),
      makeSamples('undetected_referrer_samples'),
      makeSamples('detected_communicating_samples'),
      makeSamples('undetected_communicating_samples'),
      {
        label: 'resolutions',
        type: 'table',
        fields: [
          'hostname',
          'last_resolved'
        ]
      },
      {
        label: 'Pcaps',
        field: 'pcaps',
        type: 'array'
      }
    ]
  };

  // Default cacheTimeout 24 hours
  cacheTimeout = 24 * 60 * 60 * 1000;

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

      const response = await axios.get('https://www.virustotal.com/vtapi/v2/ip-address/report', {
        params: {
          apikey: key,
          ip
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      // Fix undetected_urls
      if (response.data.undetected_urls !== undefined && Array.isArray(response.data.undetected_urls[0])) {
        const uus = [];
        for (const uu of response.data.undetected_urls) {
          uus.push({
            url: uu[0],
            sha256: uu[1],
            positives: uu[2],
            total: uu[3],
            scan_date: uu[4]
          });
        }
        response.data.undetected_urls = uus;
      }

      response.data._cont3xt = { count: 1 };
      return response.data;
    } catch (err) {
      console.log(this.name, ip, err);
      return null;
    }
  }
}

class VirusTotalHashIntegration extends Integration {
  name = 'VT Hash';
  icon = 'integrations/virustotal/icon.png';
  order = 640;
  configName = 'VirusTotal';
  itypes = {
    hash: 'fetchHash'
  };

  homePage = 'https://www.virustotal.com/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  card = {
    title: 'VirusTotal Hash for %{query}',
    fields: [
      'scan_date',
      'total',
      'positives',
      'md5',
      'sha1',
      'sha256',
      'permalink',
      'scan_id',
      'resource',
      'verbose_msg',
      {
        label: 'scans',
        fields: [
          'scan type',
          'detected',
          'result',
          'update',
          'version'
        ]
      },
      'response_code',
      {
        label: 'Pcaps',
        field: 'pcaps',
        type: 'array'
      }
    ]
  };

  // Default cacheTimeout 24 hours
  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchHash (user, hash) {
    try {
      const key = this.getUserConfig(user, 'key');
      if (!key) {
        return undefined;
      }

      const response = await axios.get('https://www.virustotal.com/vtapi/v2/file/report', {
        params: {
          apikey: key,
          resource: hash
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      response.data._cont3xt = { count: response.data.response_code === 0 ? 0 : 1 };
      return response.data;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, hash, err);
      return null;
    }
  }
}

function makeSamples (field) {
  return {
    label: field,
    field,
    type: 'table',
    fields: [
      'positives',
      'total',
      {
        label: 'sha256',
        field: 'sha256',
        pivot: 'true'
      },
      'date'
    ]
  };
}

/* eslint-disable no-new */
new VirusTotalIntegration();
new VirusTotalDomainIntegration();
new VirusTotalIPIntegration();
new VirusTotalHashIntegration();
