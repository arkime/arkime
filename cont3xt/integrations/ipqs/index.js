/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

function makeSamples (field) {
  return {
    label: field.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase()),
    field,
    type: 'table',
    fields: [
      { label: 'Human', field: 'human' },
      { label: 'Timestamp', field: 'timestamp', type: 'number' },
      { label: 'ISO', field: 'iso' }
    ]
  };
}

class IPQSIntegration extends Integration {
  name = 'IPQualityScore';
  icon = 'integrations/ipqs/icon.png';
  order = 620;
  itypes = {
  };

  homePage = 'https://www.ipqualityscore.com/';
  settings = {
    key: {
      help: 'Your ipqs api key',
      password: true,
      required: true
    }
  };

  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();
    Integration.register(this);
  }
}

class IPQSIPIntegration extends Integration {
  name = 'IPQS IP Reputation';
  icon = 'integrations/ipqs/icon.png';
  order = 600;
  configName = 'IPQualityScore';
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
    title: 'IPQS IP Reputation for %{query}',
    fields: [
      { label: 'Fraud Score', field: 'fraud_score', type: 'number' },
      { label: 'Country', field: 'country_code' },
      { label: 'Region', field: 'region' },
      { label: 'City', field: 'city' },
      { label: 'ASN', field: 'ASN', type: 'number' },
      { label: 'Organization', field: 'organization' },
      { label: 'Recent Abuse', field: 'recent_abuse', type: 'boolean' },
      { label: 'Frequent Abuser', field: 'frequent_abuser', type: 'boolean' },
      { label: 'Latitude', field: 'latitude', type: 'number' },
      { label: 'Longitude', field: 'longitude', type: 'number' }
    ]
  };

  cacheTimeout = 24 * 60 * 60 * 1000;
  constructor () {
    super();
    Integration.register(this);
  }

  async fetchIp (user, ip) {
    try {
      const key = this.getUserConfig(user, 'key');
      if (!key) {
        console.error(`${this.name}: API key not configured for user.`);
        return undefined;
      }

      const url = `https://www.ipqualityscore.com/api/json/ip?ip=${ip}`;

      let response;
      try {
        response = await axios.get(url, {
          headers: {
            'IPQS-KEY': key,
            'User-Agent': this.userAgent()
          }
        });
      } catch (axiosError) {
        let errorMessage;
        if (axiosError.response) {
          errorMessage = `Request failed. Status: ${axiosError.response.status}. Message: ${axiosError.response.data?.message || 'Unexpected error.'}`;
        } else if (axiosError.request) {
          errorMessage = 'No response received from API.';
        } else {
          errorMessage = `Error setting up request. ${axiosError.message}`;
        }
        console.error(`${this.name}: ${errorMessage}`);

        return {
          _cont3xt: { count: 0 },
          error: errorMessage
        };
      }
      if (!response?.data || Object.keys(response.data).length === 0) {
        const errorMessage = 'Empty or invalid data received from IP API.';
        console.warn(`${this.name}: ${errorMessage}`);
        return {
          _cont3xt: { count: 0 },
          error: errorMessage
        };
      }
      if (response.data?.success === false) {
        console.error(`${this.name}: IP API reported failure for IP ${ip}. Message: ${response.data.message || 'No specific message.'}`);
        return {
          _cont3xt: { count: 0 },
          error: response.data.message || 'Invalid input'
        };
      }
      response.data._cont3xt = { count: 1 };
      return response.data;
    } catch (err) {
      const errorMessage = `Unexpected error while fetching IP data: ${err.message}`;
      console.error(`${this.name}: ${errorMessage}`, err);
      return {
        _cont3xt: { count: 0 },
        error: errorMessage
      };
    }
  }
}

class IPQSEmailIntegration extends Integration {
  name = 'IPQS Email Reputation';
  icon = 'integrations/ipqs/icon.png';
  order = 605;
  configName = 'IPQualityScore';

  itypes = {
    email: 'fetchEmailVerification'
  };

  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  cacheTimeout = 24 * 60 * 60 * 1000;

  card = {
    title: 'IPQS Email Reputation for %{query}',
    fields: [
      { label: 'Fraud Score', field: 'fraud_score' },
      { label: 'SMTP Score', field: 'smtp_score' },
      { label: 'Overall Score', field: 'overall_score' },
      { label: 'Valid', field: 'valid' },
      { label: 'Honeypot', field: 'honeypot' },
      { label: 'Recent Abuse', field: 'recent_abuse' },
      makeSamples('domain_age'),
      makeSamples('first_seen'),
      { label: 'A Records', field: 'a_records', type: 'array', separator: ', ' },
      { label: 'MX Records', field: 'mx_records', type: 'array', separator: ', ' }
    ]
  };

  constructor () {
    super();
    Integration.register(this);
  }

  async fetchEmailVerification (user, email) {
    try {
      const key = this.getUserConfig(user, 'key');
      if (!key) {
        console.error(`${this.name}: API key not configured for user.`);
        return undefined;
      }

      const url = `https://www.ipqualityscore.com/api/json/email?email=${encodeURIComponent(email)}`;

      let response;
      try {
        response = await axios.get(url, {
          headers: {
            'IPQS-KEY': key,
            'User-Agent': this.userAgent()
          }
        });
      } catch (axiosError) {
        let errorMessage;
        if (axiosError.response) {
          errorMessage = `Email API request failed for ${email}. Status: ${axiosError.response.status}, Message: ${axiosError.response.data?.message || 'No error message.'}`;
          console.error(`${this.name}: ${errorMessage}`);
        } else if (axiosError.request) {
          errorMessage = `No response received for ${email}.`;
          console.error(`${this.name}: ${errorMessage}. Request: ${JSON.stringify(axiosError.request)}`);
        } else {
          errorMessage = `Error setting up request for ${email}: ${axiosError.message}`;
          console.error(`${this.name}: ${errorMessage}`);
        }

        return {
          _cont3xt: { count: 0 },
          error: errorMessage
        };
      }

      if (!response?.data || Object.keys(response.data).length === 0) {
        const errorMessage = `Empty or invalid data received from API for ${email}.`;
        console.warn(`${this.name}: ${errorMessage}`);
        return {
          _cont3xt: { count: 0 },
          error: errorMessage
        };
      }

      if (response.data?.success === false) {
        console.error(`${this.name}: API reported failure for ${email}. Message: ${response.data.message || 'No specific message.'}`);
        return {
          _cont3xt: { count: 0 },
          error: response.data.message || 'Invalid input'
        };
      }

      response.data._cont3xt = { count: 1 };
      return response.data;
    } catch (err) {
      console.error(`${this.name}: Unexpected error fetching data for ${email}:`, err);
      return {
        _cont3xt: { count: 0 },
        error: `Unexpected error occurred while fetching data for ${email}.`
      };
    }
  }
}

class IPQSEmailLeakIntegration extends Integration {
  name = 'IPQS Email Leak Records';
  icon = 'integrations/ipqs/icon.png';
  order = 606;
  configName = 'IPQualityScore';

  itypes = {
    email: 'fetchEmailLeak'
  };

  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  cacheTimeout = 24 * 60 * 60 * 1000;

  card = {
    title: 'IPQS Email Leak Records for %{query}',
    fields: [
      { label: 'Exposed', field: 'exposed', type: 'boolean' },
      { label: 'Source', field: 'source', type: 'array', separator: ', ' },
      { label: 'Plain Text Password', field: 'plain_text_password', type: 'boolean' },
      makeSamples('first_seen')
    ]
  };

  constructor () {
    super();
    Integration.register(this);
  }

  async fetchEmailLeak (user, email) {
    try {
      const key = this.getUserConfig(user, 'key');
      if (!key) {
        console.error(`${this.name}: API key not configured for user.`);
        return undefined;
      }

      const url = `https://ipqualityscore.com/api/json/leaked/email?email=${encodeURIComponent(email)}`;

      let response;
      try {
        response = await axios.get(url, {
          headers: {
            'IPQS-KEY': key,
            'User-Agent': this.userAgent()
          }
        });
      } catch (axiosError) {
        let errorMsg;
        if (axiosError.response) {
          errorMsg = `Request failed. Status: ${axiosError.response.status}. Message: ${axiosError.response.data?.message || 'No message provided.'}`;
          console.error(`${this.name}: Email Leak API request failed for ${email}. Status: ${axiosError.response.status}, Data: ${JSON.stringify(axiosError.response.data)}`);
        } else if (axiosError.request) {
          errorMsg = 'No response received from the Email Leak API.';
          console.error(`${this.name}: No response received from Darkweb API for ${email}. Request: ${JSON.stringify(axiosError.request)}`);
        } else {
          errorMsg = `Request setup failed. Message: ${axiosError.message}`;
          console.error(`${this.name}: Error setting up Darkweb API request for ${email}. Message: ${axiosError.message}`);
        }

        return {
          _cont3xt: { count: 0 },
          error: errorMsg
        };
      }
      if (!response?.data || Object.keys(response.data).length === 0) {
        const errorMsg = `Empty or invalid data received from the Email Leak API for ${email}.`;
        console.warn(`${this.name}: ${errorMsg} Response: ${JSON.stringify(response?.data)}`);
        return {
          _cont3xt: { count: 0 },
          error: errorMsg
        };
      }
      if (response.data?.success === false) {
        console.error(`${this.name}: Darkweb API reported failure for ${email}. Message: ${response.data.message || 'No specific message.'}`);
        return {
          _cont3xt: { count: 0 },
          error: response.data.message || 'Invalid input'
        };
      }

      response.data._cont3xt = { count: 1 };
      return response.data;
    } catch (err) {
      console.error(`${this.name}: An unexpected error occurred while fetching darkweb data for ${email}:`, err);
      return {
        _cont3xt: { count: 0 },
        error: `Unexpected error occurred while fetching data for ${email}.`
      };
    }
  }
}

class IPQSUrlIntegration extends Integration {
  name = 'IPQS URL Reputation';
  icon = 'integrations/ipqs/icon.png';
  order = 605;
  configName = 'IPQualityScore';

  itypes = {
    url: 'fetchUrl',
    domain: 'fetchUrl'
  };

  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  card = {
    title: 'IPQS URL Reputation for %{query}',
    fields: [
      { label: 'Risk Score', field: 'risk_score', type: 'number' },
      { label: 'Domain', field: 'domain' },
      { label: 'IP Address', field: 'ip_address' },
      { label: 'Domain Rank', field: 'domain_rank', type: 'number' },
      { label: 'Parking Detected', field: 'parking', type: 'boolean' },
      { label: 'Spamming Detected', field: 'spamming', type: 'boolean' },
      { label: 'Malware Detected', field: 'malware', type: 'boolean' },
      { label: 'Phishing Detected', field: 'phishing', type: 'boolean' },
      { label: 'Suspicious Activity', field: 'suspicious', type: 'boolean' },
      { label: 'Adult', field: 'adult', type: 'boolean' },
      { label: 'Country Code', field: 'country_code' },
      makeSamples('domain_age'),
      { label: 'A Records', field: 'a_records', type: 'array', separator: ', ' },
      { label: 'MX Records', field: 'mx_records', type: 'array', separator: ', ' },
      { label: 'NS Records', field: 'ns_records', type: 'array', separator: ', ' }
    ]
  };

  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();
    Integration.register(this);
  }

  async fetchUrl (user, urlParam) {
    try {
      const key = this.getUserConfig(user, 'key');
      if (!key) {
        console.error(`${this.name}: API key not configured for user.`);
        return undefined;
      }

      const url = `https://ipqualityscore.com/api/json/url/?url=${encodeURIComponent(urlParam)}`;

      let response;
      try {
        response = await axios.get(url, {
          headers: {
            'IPQS-KEY': key,
            'User-Agent': this.userAgent()
          }
        });
      } catch (axiosError) {
        let errorMsg;
        if (axiosError.response) {
          errorMsg = `URL API request failed. Status: ${axiosError.response.status}. Message: ${axiosError.response.data?.message || 'No message provided.'}`;
          console.error(`${this.name}: URL API request failed for ${urlParam}. Status: ${axiosError.response.status}, Data: ${JSON.stringify(axiosError.response.data)}`);
        } else if (axiosError.request) {
          errorMsg = 'No response received from the URL API.';
          console.error(`${this.name}: No response received from URL API for ${urlParam}. Request: ${JSON.stringify(axiosError.request)}`);
        } else {
          errorMsg = `Error setting up URL API request. Message: ${axiosError.message}`;
          console.error(`${this.name}: Error setting up URL API request for ${urlParam}. Message: ${axiosError.message}`);
        }
        return {
          _cont3xt: { count: 0 },
          error: errorMsg
        };
      }

      if (!response?.data || Object.keys(response.data).length === 0) {
        const errorMsg = `Empty or invalid data received from the URL API for ${urlParam}.`;
        console.warn(`${this.name}: ${errorMsg}`);
        return {
          _cont3xt: { count: 0 },
          error: errorMsg
        };
      }

      if (response.data?.success === false) {
        console.error(`${this.name}: URL API reported failure for ${urlParam}. Message: ${response.data.message || 'No specific message.'}`);
        return {
          _cont3xt: { count: 0 },
          error: response.data.message || 'Invalid input'
        };
      }

      response.data._cont3xt = { count: 1 };
      return response.data;
    } catch (err) {
      console.error(`${this.name}: An unexpected error occurred while fetching URL data for ${urlParam}:`, err);
      return {
        _cont3xt: { count: 0 },
        error: `Unexpected error occurred while fetching data for ${urlParam}.`
      };
    }
  }
}

class IPQSPhoneIntegration extends Integration {
  name = 'IPQS Phone Validation';
  icon = 'integrations/ipqs/icon.png';
  order = 607;
  configName = 'IPQualityScore';

  itypes = {
    phone: 'fetchPhone'
  };

  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  card = {
    title: 'IPQS Phone Validation for %{query}',
    fields: [
      { label: 'Fraud Score', field: 'fraud_score', type: 'number' },
      { label: 'Recent Abuse', field: 'recent_abuse', type: 'boolean' },
      { label: 'Risky', field: 'risky', type: 'boolean' },
      { label: 'Active', field: 'active', type: 'boolean' },
      { label: 'Country', field: 'country' },
      { label: 'City', field: 'city' },
      { label: 'ZIP Code', field: 'zip_code' },
      { label: 'Leaked', field: 'leaked', type: 'boolean' },
      { label: 'Spammer', field: 'spammer', type: 'boolean' }
    ]
  };

  cacheTimeout = 24 * 60 * 60 * 1000;

  constructor () {
    super();
    Integration.register(this);
  }

  async fetchPhone (user, phone) {
    if (!phone || typeof phone !== 'string') {
      console.error(`${this.name}: Invalid or missing phone number (expected string) provided: "${phone}"`);
      return undefined;
    }

    let cleanedPhone = phone.replace(/[\s-]/g, '');

    if (cleanedPhone.startsWith('++')) {
      cleanedPhone = '+' + cleanedPhone.substring(2);
    }

    try {
      const key = this.getUserConfig(user, 'key');
      if (!key) {
        console.error(`${this.name}: API key not configured for user.`);
        return undefined;
      }

      const url = `https://www.ipqualityscore.com/api/json/phone/?phone=${encodeURIComponent(cleanedPhone)}`;

      let response;
      try {
        response = await axios.get(url, {
          headers: {
            'IPQS-KEY': key,
            'User-Agent': this.userAgent()
          }
        });
      } catch (axiosError) {
        let errorMsg;
        if (axiosError.response) {
          errorMsg = `Phone API request failed. Status: ${axiosError.response.status}. Message: ${axiosError.response.data?.message || 'No message provided.'}`;
          console.error(`${this.name}: Phone API request failed for phone ${phone}. Status: ${axiosError.response.status}, Data: ${JSON.stringify(axiosError.response.data)}`);
        } else if (axiosError.request) {
          errorMsg = 'No response received from the Phone API.';
          console.error(`${this.name}: No response received from Phone API for phone ${phone}. Request: ${JSON.stringify(axiosError.request)}`);
        } else {
          errorMsg = `Error setting up Phone API request. Message: ${axiosError.message}`;
          console.error(`${this.name}: Error setting up Phone API request for phone ${phone}. Message: ${axiosError.message}`);
        }

        return {
          _cont3xt: { count: 0 },
          error: errorMsg
        };
      }

      if (!response?.data || Object.keys(response.data).length === 0) {
        const errorMsg = `Empty or invalid data received from the Phone API for phone ${phone}.`;
        console.warn(`${this.name}: ${errorMsg}`);
        return {
          _cont3xt: { count: 0 },
          error: errorMsg
        };
      }
      if (response.data?.success === false) {
        console.error(`${this.name}: Phone API reported failure for phone ${phone}. Message: ${response.data.message || 'No specific message.'}`);
        return {
          _cont3xt: { count: 0 },
          error: response.data.message || 'Invalid input'
        };
      }

      response.data._cont3xt = { count: 1 };
      return response.data;
    } catch (err) {
      console.error(`${this.name}: An unexpected error occurred while fetching phone data for phone ${phone}:`, err);
      return {
        _cont3xt: { count: 0 },
        error: `Unexpected error occurred while fetching data for ${phone}.`
      };
    }
  }
}
 
new IPQSIntegration();
new IPQSIPIntegration();
new IPQSEmailIntegration();
new IPQSEmailLeakIntegration();
new IPQSUrlIntegration();
new IPQSPhoneIntegration();
