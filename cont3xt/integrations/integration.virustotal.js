const Integration = require('../integration.js');
const axios = require('axios');

class VirusTotalIntegration extends Integration {
  name = 'VirusTotal';
  itypes = {
    domain: 'fetchDomain',
    ip: 'fetchIp',
    hash: 'fetchHash'
  };

  key;

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

      const virusTotalRes = await axios.get('https://www.virustotal.com/vtapi/v2/domain/report', {
        params: {
          apikey: key,
          domain: domain
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      return virusTotalRes.data;
    } catch (err) {
      console.log('VirusTotal', domain, err);
      return null;
    }
  }

  async fetchIp (user, ip) {
    try {
      const key = this.getUserConfig(user, 'VirusTotalKey');
      if (!key) {
        return undefined;
      }

      const virusTotalRes = await axios.get('https://www.virustotal.com/vtapi/v2/ip-address/report', {
        params: {
          apikey: key,
          ip: ip
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      return virusTotalRes.data;
    } catch (err) {
      console.log('VirusTotal', ip, err);
      return null;
    }
  }

  async fetchHash (user, hash) {
    try {
      const key = this.getUserConfig(user, 'VirusTotalKey');
      if (!key) {
        return undefined;
      }

      const virusTotalRes = await axios.get('https://www.virustotal.com/vtapi/v2/file/report', {
        params: {
          apikey: key,
          resource: hash
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      return virusTotalRes.data;
    } catch (err) {
      console.log('VirusTotal', hash, err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new VirusTotalIntegration();
