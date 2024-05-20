/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

const Integration = require('../../integration.js');
const ArkimeConfig = require('../../../common/arkimeConfig');
const { createClient } = require('@clickhouse/client');

class ClickHouseIntegration extends Integration {
  // Integration Items
  name;
  icon;
  order;
  itypes = {};

  // CH Items
  #statement;
  #url;
  #client;

  static #order = 50000;

  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  card = {
    fields: [
    ]
  };

  constructor (section) {
    super();

    this.section = section;
    this.name = ArkimeConfig.getFull(section, 'name', section);
    this.icon = ArkimeConfig.getFull(section, 'icon', 'integrations/clickhouse/clickhouse.jpg');
    this.order = ClickHouseIntegration.#order++;
    this.card.title = `${this.name} for %{query}`;
    const itypes = ArkimeConfig.getFullArray(section, 'itypes', ArkimeConfig.exit);

    this.#statement = ArkimeConfig.getFull(section, 'statement', ArkimeConfig.exit);

    this.card.fields.push({ label: 'hits', type: 'json' });

    itypes.forEach(itype => {
      this.itypes[itype] = 'search';
    });
    this.#url = ArkimeConfig.getFull(section, 'url', ArkimeConfig.exit);

    try {
      this.#client = createClient({ url: this.#url });
    } catch (err) {
      console.log(section, 'ERROR - creating ClickHouse client', err, this.#url);
      return;
    }

    Integration.register(this);
  }

  async searchMethod (user, item) {
    const resultSet = await this.#client.query({
      query: this.#statement,
      format: 'JSONEachRow',
      query_params: { value: item }
    });

    const hits = resultSet.json();
    const data = {
      hits,
      _cont3xt: {
        count: hits.length
      }
    };
    return data;
  };
}

const sections = ArkimeConfig.getSections().filter((e) => { return e.match(/^clickhouse:/); });
sections.forEach((section) => {
  // eslint-disable-next-line no-new
  new ClickHouseIntegration(section);
});
