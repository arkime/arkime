/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const whois = require('whois-json');

class WhoisIntegration extends Integration {
  name = 'Whois';
  icon = 'integrations/whois/icon.png';
  order = 100;
  itypes = {
    domain: 'fetchDomain'
  };

  card = {
    title: 'Whois for %{query}',
    fields: [
      {
        label: 'updatedDate',
        type: 'date'
      },
      {
        label: 'creationDate',
        type: 'date'
      },
      {
        label: 'createdDate',
        type: 'date'
      },
      'registrar',
      'registrantOrganization',
      'adminCountry'
    ]
  };

  tidbits = {
    fields: [
      {
        field: 'creationDate',
        postProcess: 'removeTime',
        tooltipTemplate: '<value>',
        purpose: 'registered',
        precedence: 3,
        order: 100
      },
      {
        field: 'createdDate',
        postProcess: 'removeTime',
        tooltipTemplate: '<value>',
        purpose: 'registered',
        precedence: 3,
        order: 100
      },
      {
        field: 'registrar',
        purpose: 'registrar',
        precedence: 3,
        order: 110
      }
    ]
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchDomain (user, domain) {
    try {
      const data = await whois(domain);
      data._cont3xt = { count: 1 };
      return data;
    } catch (err) {
      console.log(this.name, domain, err);
      return undefined;
    }
  }
}

// eslint-disable-next-line no-new
new WhoisIntegration();
