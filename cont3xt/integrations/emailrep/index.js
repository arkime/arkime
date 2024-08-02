/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class EmailReputationIntegration extends Integration {
  name = 'Email Reputation';
  icon = 'integrations/emailrep/icon.png';
  itypes = {
    email: 'fetch'
  };

  homePage = 'https://emailrep.io/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    },
    key: {
      help: 'Your Email Reputation Key',
      password: true,
      required: true
    }
  };

  card = {
    title: 'Email Reputation for %{query}',
    searchUrls: [{
      url: 'https://emailrep.io/%{query}',
      itypes: ['email'],
      name: 'Search Email Reputation for %{query}'
    }],
    fields: [ // TODO which fields should be displayed in the card?
      {
        label: 'Reputation',
        field: 'reputation'
      },
      {
        label: 'Suspicious',
        field: 'suspicious'
      },
      {
        label: 'References',
        field: 'references'
      },
      {
        label: 'Blacklisted',
        field: 'details.blacklisted'
      },
      {
        label: 'Malicious Activity',
        field: 'details.malicious_activity'
      }
    ]
  };

  tidbits = {
    order: 200,
    fields: [
      {
        tooltip: 'Free Provider?',
        field: 'details.free_provider',
        type: 'badge'
      },
      {
        tooltip: 'Disposable?',
        field: 'details.disposable',
        type: 'badge'
      },
      {
        tooltip: 'Domain Reputation',
        field: 'details.domain_reputation',
        type: 'badge'
      },
      {
        tooltip: 'Days Since Domain Creation',
        field: 'details.days_since_domain_creation',
        type: 'badge'
      }
    ]
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetch (user, query) {
    const key = this.getUserConfig(user, 'key');
    if (!key) {
      return undefined;
    }

    try {
      const result = await axios.get(`https://emailrep.io/${query}`, {
        headers: {
          'User-Agent': this.userAgent(),
          key
        }
      }).data;

      result._cont3xt = { count: 0 };
      // count all of these fields that are true
      const fields = ['blacklisted', 'malicious_activity', 'malicious_activity_recent', 'credentials_leaked', 'credentials_leaked_recent', 'data_breach', 'new_domain', 'suspicious_tld', 'spam', 'accept_all', 'spoofable'];
      for (const field in fields) {
        if (result.details[field]) {
          result._cont3xt.count++;
        }
      }

      if (result._cont3xt.count > 0) {
        result._cont3xt.severity = 'high';
      }

      console.log('EMAIL REP RESULT', result); // TODO REMOVE
      return result;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, query, err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new EmailReputationIntegration();
