/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

const Integration = require('../../integration.js');
const ArkimeConfig = require('../../../common/arkimeConfig');
const { DBSQLClient } = require('@databricks/sql');

class DatabricksIntegration extends Integration {
  // Integration Items
  name;
  icon;
  order;
  itypes = {};

  // DB Items
  #statement;
  #session;

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
    this.icon = ArkimeConfig.getFull(section, 'icon', 'integrations/databricks/icon.png');
    this.order = DatabricksIntegration.#order++;
    this.card.title = `${this.name} for %{query}`;
    const itypes = ArkimeConfig.getFullArray(section, 'itypes', ArkimeConfig.exit);

    this.#statement = ArkimeConfig.getFull(section, 'statement', ArkimeConfig.exit);

    this.card.fields.push({ label: 'hits', type: 'json' });

    itypes.forEach(itype => {
      this.itypes[itype] = 'search';
    });

    const host = ArkimeConfig.getFull(section, 'host', ArkimeConfig.exit);
    const path = ArkimeConfig.getFull(section, 'path', ArkimeConfig.exit);
    const token = ArkimeConfig.getFull(section, 'token', ArkimeConfig.exit);

    const client = new DBSQLClient();

    client.connect({
      host,
      path,
      token
    }).then(async (client2) => {
      this.#session = await client2.openSession();
    }).catch((err) => {
      console.log(section, 'ERROR - creating Databricks client', err, host, path);
      return;
    });

    Integration.register(this);
  }

  async search (user, item) {
    const queryOperation = await this.#session.executeStatement(this.#statement, { namedParameters: { SEARCHTERM: item } });
    const resultSet = await queryOperation.fetchAll();
    await queryOperation.close();

    const data = {
      hits: resultSet,
      _cont3xt: {
        count: resultSet.length
      }
    };
    return data;
  };
}

const sections = ArkimeConfig.getSections().filter((e) => { return e.match(/^databricks:/); });
sections.forEach((section) => {

  new DatabricksIntegration(section);
});
