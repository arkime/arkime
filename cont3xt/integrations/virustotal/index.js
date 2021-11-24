/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class VirusTotalIntegration extends Integration {
  name = 'VirusTotal';
  icon = 'integrations/virustotal/icon.png';
  noStats = true;
  itypes = {
  };

  userSettings = {
    VirusTotalKey: {
      help: 'Your virustotal api key',
      password: true
    }
  }

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
  order = 320;
  configName = 'VirusTotal'
  itypes = {
    domain: 'fetchDomain'
  };

  card = {
    title: 'VirusTotal Domain for %{query}',
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
          'sha256',
          'scan_date'
        ]
      },
      makeSamples('detected_downloaded_samples'),
      makeSamples('undetected_downloaded_samples'),
      makeSamples('detected_referrer_samples'),
      makeSamples('undetected_referrer_samples'),
      {
        label: 'resolutions',
        type: 'table',
        fields: [
          'ip_address',
          'last_resolved'
        ]
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
      const key = this.getUserConfig(user, 'VirusTotalKey');
      if (!key) {
        return undefined;
      }

      const response = await axios.get('https://www.virustotal.com/vtapi/v2/domain/report', {
        params: {
          apikey: key,
          domain: domain
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

      response.data._count = 1;
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
  order = 340;
  configName = 'VirusTotal'
  itypes = {
    ip: 'fetchIp'
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
          'sha256',
          'scan_date'
        ]
      },
      makeSamples('detected_downloaded_samples'),
      makeSamples('undetected_downloaded_samples'),
      makeSamples('detected_referrer_samples'),
      makeSamples('undetected_referrer_samples'),
      {
        label: 'resolutions',
        type: 'table',
        fields: [
          'hostname',
          'last_resolved'
        ]
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
      const key = this.getUserConfig(user, 'VirusTotalKey');
      if (!key) {
        return undefined;
      }

      const response = await axios.get('https://www.virustotal.com/vtapi/v2/ip-address/report', {
        params: {
          apikey: key,
          ip: ip
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

      response.data._count = 1;
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
  order = 360;
  configName = 'VirusTotal'
  itypes = {
    hash: 'fetchHash'
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
      'response_code'
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
      const key = this.getUserConfig(user, 'VirusTotalKey');
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

      response.data._count = 1;
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
    field: field,
    type: 'table',
    fields: [
      'positives',
      'total',
      'sha256',
      'date'
    ]
  };
}

/* eslint-disable no-new */
new VirusTotalIntegration();
new VirusTotalDomainIntegration();
new VirusTotalIPIntegration();
new VirusTotalHashIntegration();
