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
      {
        field: 'line_type_intelligence.carrier_name',
        label: 'Carrier'
      },
      {
        field: 'line_type_intelligence.type',
        label: 'Line Type'
      },
      {
        field: 'caller_name_object.caller_name',
        label: 'Caller Name'
      },
      {
        field: 'caller_name_object.caller_type',
        label: 'Caller Type'
      },
      {
        label: 'Country Code',
        field: 'country_code'
      },
      {
        label: 'Phone Number',
        field: 'phone_number'
      },
      {
        label: 'National Format (local)',
        field: 'national_format'
      },
      {
        field: 'sms_pumping_risk.carrier_risk_category',
        label: 'SMS pumping risk category'
      },
      {
        field: 'sms_pumping_risk.sms_pumping_risk_score',
        label: 'SMS pumping risk score'
      }
    ]
  };

  tidbits = {
    order: 100,
    fields: [
      {
        label: 'Name',
        field: 'caller_name_object.caller_name',
        display: 'cont3xtField'
      },
      {
        tooltip: 'Carrier',
        field: 'line_type_intelligence.carrier_name'
      },
      {
        tooltip: 'Type',
        field: 'caller_name_object.caller_type'
      },
      {
        field: 'country_code',
        postProcess: [
          'countryEmoji',
          { template: '<value> <data.country_code>' }
        ]
      },
      {
        field: 'sms_pumping_risk.sms_pumping_risk_score',
        tooltip: 'SMS pumping risk score',
        display: 'badge'
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

      const result = await axios.get(`https://lookups.twilio.com/v2/PhoneNumbers/${query}?Fields=caller_name,line_type_intelligence,sms_pumping_risk`, {
        headers: {
          'User-Agent': this.userAgent()
        },
        auth: {
          username: sid,
          password: token
        }
      });

      // copy over caller_name and carrier from new v2 schema to v1 schema
      // so that old overview cards still work
      const callerNameObj = result.data?.caller_name;
      result.data.caller_name_object = callerNameObj;
      result.data.caller_name = callerNameObj?.caller_name;
      result.data.carrier = result.data?.line_type_intelligence?.carrier_name;

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
