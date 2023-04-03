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
