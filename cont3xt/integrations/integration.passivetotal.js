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
const Integration = require('../integration.js');
const axios = require('axios');

class PassiveTotalIntegration extends Integration {
  name = 'PassiveTotal';
  icon = 'public/passiveTotalIcon.png';
  noStats = true;
  itypes = {
  };

  userSettings = {
    PassiveTotalUser: {
      help: 'Your Passive Total api user'
    },
    PassiveTotalKey: {
      help: 'Your Passive Total api key',
      password: true
    }
  };

  // Default cacheTimeout 24 hours
  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();

    Integration.register(this);
  }
}

class PassiveTotalWhoisIntegration extends Integration {
  name = 'PassiveTotalWhois';
  icon = 'public/passiveTotalIcon.png';
  itypes = {
    domain: 'fetchDomain'
  };

  // Default cacheTimeout 24 hours
  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchDomain (user, domain) {
    try {
      const puser = this.getUserConfigFull(user, 'PassiveTotal', 'PassiveTotalUser');
      const pkey = this.getUserConfigFull(user, 'PassiveTotal', 'PassiveTotalKey');
      if (!puser || !pkey) {
        return undefined;
      }

      const result = await axios.get('https://api.passivetotal.org/v2/whois', {
        params: {
          query: domain,
          history: false
        },
        auth: {
          username: puser,
          password: pkey
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });
      return result.data;
    } catch (err) {
      console.log(this.name, domain, err);
      return null;
    }
  }
}

class PassiveTotalDomainsIntegration extends Integration {
  name = 'PassiveTotalDomains';
  icon = 'public/passiveTotalIcon.png';
  itypes = {
    domain: 'fetchDomain'
  };

  // Default cacheTimeout 24 hours
  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchDomain (user, domain) {
    try {
      const puser = this.getUserConfigFull(user, 'PassiveTotal', 'PassiveTotalUser');
      const pkey = this.getUserConfigFull(user, 'PassiveTotal', 'PassiveTotalKey');
      if (!puser || !pkey) {
        return undefined;
      }

      const result = await axios.get('https://api.passivetotal.org/v2/enrichment/subdomains', {
        params: {
          query: domain
        },
        auth: {
          username: puser,
          password: pkey
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      return result.data;
    } catch (err) {
      console.log(this.name, domain, err);
      return null;
    }
  }
}

class PassiveTotalDNSIntegration extends Integration {
  name = 'PassiveTotalDNS';
  icon = 'public/passiveTotalIcon.png';
  itypes = {
    ip: 'fetchIp'
  };

  // Default cacheTimeout 24 hours
  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchIp (user, ip) {
    try {
      const puser = this.getUserConfigFull(user, 'PassiveTotal', 'PassiveTotalUser');
      const pkey = this.getUserConfigFull(user, 'PassiveTotal', 'PassiveTotalKey');
      if (!puser || !pkey) {
        return undefined;
      }

      const result = await axios.get('https://api.passivetotal.org/v2/dns/passive', {
        params: {
          query: ip
        },
        auth: {
          username: puser,
          password: pkey
        },
        headers: {
          'User-Agent': this.userAgent()
        }
      });

      return result.data;
    } catch (err) {
      console.log(this.name, ip, err);
      return null;
    }
  }
}

/* eslint-disable no-new */
new PassiveTotalIntegration();
new PassiveTotalWhoisIntegration();
new PassiveTotalDomainsIntegration();
new PassiveTotalDNSIntegration();
