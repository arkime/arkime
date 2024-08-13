/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const ArkimeConfig = require('../../../common/arkimeConfig');
const axios = require('axios');

class WiseIntegration extends Integration {
  name;
  section;
  icon = '../assets/wise.png';
  itypes = {
    domain: 'fetchDomain',
    email: 'fetchEmail',
    hash: 'fetchHash',
    ip: 'fetchIp'
  };

  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  card = {
    fields: [{
      label: 'Results',
      field: 'results',
      type: 'table',
      fields: [
        {
          label: 'Key',
          field: 'key'
        },
        {
          label: 'Value',
          field: 'value'
        }
      ]
    }]
  };

  #wiseUrl;

  // ----------------------------------------------------------------------------
  constructor (section) {
    super();

    this.section = section;
    this.name = ArkimeConfig.getFull(section, 'name', section);
    this.icon = ArkimeConfig.getFull(section, 'icon', this.icon);
    this.#wiseUrl = ArkimeConfig.getFull(section, 'arkimeUrl', 'http://localhost:8081');

    Integration.register(this);
  }

  // ----------------------------------------------------------------------------
  async doFetch (user, item, type) {
    const response = await axios.get(`${this.#wiseUrl}/${type}/${item}`);

    const results = response.data.map(e => {
      return {
        key: e.field,
        value: e.value
      };
    });

    return {
      results,
      _cont3xt: {
        count: results.length
      }
    };
  }

  // ----------------------------------------------------------------------------
  async fetchDomain (user, item, ip) {
    return this.doFetch(user, item, 'domain');
  };

  // ----------------------------------------------------------------------------
  async fetchEmail (user, item) {
    return this.doFetch(user, item, 'email');
  }

  // ----------------------------------------------------------------------------
  async fetchHash (user, item) {
    if (item.length === 32) {
      return this.doFetch(user, item, 'md5');
    } else {
      return this.doFetch(user, item, 'sha256');
    }
  }

  // ----------------------------------------------------------------------------
  async fetchIp (user, item) {
    return this.doFetch(user, item, 'ip');
  }
}

// ----------------------------------------------------------------------------
const sections = ArkimeConfig.getSections().filter((e) => { return e.match(/^wise:/); });
sections.forEach((section) => {
  // eslint-disable-next-line no-new
  new WiseIntegration(section);
});
