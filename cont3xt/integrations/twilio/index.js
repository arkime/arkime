/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

class TwilioIntegration extends Integration {
  name = 'Twilio';
  icon = 'integrations/twilio/icon.png';
  order = 440;
  cacheTimeout = '1w';
  itypes = {
    phone: 'fetch'
  };

  homePage = 'https://www.twilio.com/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    },
    sid: {
      help: 'Your Twilio SID',
      required: true
    },
    token: {
      help: 'Your Twilio Token',
      password: true,
      required: true
    }
  };

  card = {
    title: 'Twilio for %{query}',
    searchUrls: [{
      url: 'https://www.twilio.com/lookup/%{query}',
      itypes: ['phone'],
      name: 'Search Twilio for %{query}'
    }],
    fields: [
      'phone_number',
      {
        label: 'caller_name',
        type: 'json'
      },
      {
        label: 'carrier',
        type: 'json'
      }
    ]
  };

  tidbits = {
    order: 100,
    fields: [
      {
        label: 'Caller Name',
        field: 'caller_name.caller_name',
        display: 'cont3xtField'
      },
      {
        tooltip: 'carrier name',
        field: 'carrier.name'
      },
      {
        tooltip: 'carrier type',
        field: 'carrier.type'
      },
      {
        field: 'country_code',
        postProcess: [
          'countryEmoji',
          { template: '<value> <data.country_code>' }
        ]
      }
    ]
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetch (user, query) {
    try {
      const sid = this.getUserConfig(user, 'sid');
      if (!sid) {
        return undefined;
      }

      const token = this.getUserConfig(user, 'token');
      if (!token) {
        return undefined;
      }

      query = query.replace(/[-. ]/g, '');
      query = query.replace(/^\+011/, '+');
      query = query.replace(/^\+00/, '+');
      if (query[0] !== '+') {
        query = '+1' + query;
      }

      const result = await axios.get(`https://lookups.twilio.com/v1/PhoneNumbers/${query}?Type=carrier&Type=caller-name`, {
        headers: {
          'User-Agent': this.userAgent()
        },
        auth: {
          username: sid,
          password: token
        }
      });

      result.data._cont3xt = { count: 1 };
      return result.data;
    } catch (err) {
      if (Integration.debug <= 1 && err?.response?.status === 404) { return null; }
      console.log(this.name, query, err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new TwilioIntegration();
