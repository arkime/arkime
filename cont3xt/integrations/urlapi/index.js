/******************************************************************************/
/* Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const ArkimeConfig = require('../../../common/arkimeConfig');
const axios = require('axios');

class URLApiIntegration extends Integration {
  // Integration Items
  name;
  icon;
  order;
  itypes = {};

  static #order = 50000;

  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  card = {
    fields: [
      { label: 'data', type: 'json' }
    ]
  };

  // URLApi Items
  #url;
  #headers = {};
  #keyHeader;
  #resultField;
  #axiosOptions;

  constructor (section) {
    super();

    this.section = section;
    this.name = ArkimeConfig.getFull(section, 'name', section);
    this.icon = ArkimeConfig.getFull(section, 'icon', 'integrations/urlapi/urlapi.png');
    this.order = URLApiIntegration.#order++;
    this.card.title = `${this.name} for %{query}`;

    this.#url = ArkimeConfig.getFull(section, 'url', ArkimeConfig.exit);
    if (!this.#url.match(/^https?:\/\//)) {
      console.log(`ERROR - ${section} url must start with http:// or https://`);
      process.exit(1);
    }

    const itypes = ArkimeConfig.getFullArray(section, 'itypes', ArkimeConfig.exit);
    itypes.forEach(itype => {
      this.itypes[itype] = 'fetch';
    });

    ArkimeConfig.getFullArray(section, 'headers', [], ';').forEach((header) => {
      const colon = header.indexOf(':');
      if (colon > 0) {
        this.#headers[header.slice(0, colon).trim()] = header.slice(colon + 1).trim();
      }
    });

    // Users can set their own key, which is only ever sent in this header to the configured url
    this.#keyHeader = ArkimeConfig.getFull(section, 'keyHeader');
    if (this.#keyHeader) {
      this.settings.key = {
        help: `Your api key, sent in the ${this.#keyHeader} header`,
        password: true
      };
    }

    this.#resultField = ArkimeConfig.getFull(section, 'resultField');

    this.#axiosOptions = {
      timeout: parseInt(ArkimeConfig.getFull(section, 'timeout', 30)) * 1000,
      maxContentLength: parseInt(ArkimeConfig.getFull(section, 'maxSize', 10 * 1024 * 1024)),
      maxRedirects: parseInt(ArkimeConfig.getFull(section, 'maxRedirects', 0))
    };

    // insecure, caTrustFile and the clientKey/clientCert (mTLS) settings
    this.#axiosOptions.httpsAgent = this.httpsAgent(this.#url);

    Integration.register(this);
  }

  async fetch (user, query) {
    // Replace with functions so a $& or $' in the query isn't a replacement pattern.
    // Standard base64 has / + = so it is still url encoded, base64url is url safe as is.
    const b64 = Buffer.from(query).toString('base64');
    const url = this.#url
      .replace(/{value}/g, () => encodeURIComponent(query))
      .replace(/{valueBase64}/g, () => encodeURIComponent(b64))
      .replace(/{valueBase64Url}/g, () => b64.replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, ''));

    const headers = { 'User-Agent': this.userAgent(), ...this.#headers };
    if (this.#keyHeader) {
      const key = this.getUserConfig(user, 'key');
      if (key) { headers[this.#keyHeader] = key; }
    }

    try {
      const response = await axios.get(url, { ...this.#axiosOptions, headers });
      const data = (typeof response.data === 'string') ? JSON.parse(response.data) : response.data;
      if (!data || typeof data !== 'object' || (this.#resultField && data[this.#resultField] === undefined)) {
        return Integration.NoResult;
      }

      return {
        data,
        _cont3xt: {
          count: 1
        }
      };
    } catch (err) {
      if (err.response?.status === 404) {
        return Integration.NoResult;
      }
      throw err;
    }
  }
}

ArkimeConfig.getSections().filter((e) => { return e.match(/^urlapi:/); }).forEach((section) => {
  new URLApiIntegration(section);
});
