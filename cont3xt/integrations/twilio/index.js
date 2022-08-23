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
      }
    ]
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetch (user, query) {
    try {
      const sid = this.getUserConfig(user, 'Twilio', 'sid');
      if (!sid) {
        return undefined;
      }

      const token = this.getUserConfig(user, 'Twilio', 'token');
      if (!token) {
        return undefined;
      }

      const result = await axios.get(`https://lookups.twilio.com/v1/PhoneNumbers/+1${query}?Type=carrier&Type=caller-name`, {
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
