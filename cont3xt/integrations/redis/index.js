/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const ArkimeConfig = require('../../../common/arkimeConfig');
const ArkimeUtil = require('../../../common/arkimeUtil');

class RedisIntegration extends Integration {
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

  // Redis Items
  #url;
  #redisClient;
  #redisMethod;
  #keyTemplate;

  constructor (section, isCSV) {
    super();

    this.section = section;
    this.name = ArkimeConfig.getFull(section, 'name', section);
    this.icon = ArkimeConfig.getFull(section, 'icon', 'integrations/redis/icon.png');
    this.order = RedisIntegration.#order++;
    this.card.title = `${this.name} for %{query}`;
    this.#keyTemplate = ArkimeConfig.getFull(section, 'keyTemplate');
    this.#redisMethod = ArkimeConfig.getFull(section, 'redisMethod', 'get');
    const itypes = ArkimeConfig.getFullArray(section, 'itypes', ArkimeConfig.exit);

    this.#url = ArkimeConfig.getFull(section, 'url', ArkimeConfig.exit);

    itypes.forEach(itype => {
      this.itypes[itype] = `${itype}Fetch`;
    });

    this.#redisClient = ArkimeUtil.createRedisClient(ArkimeConfig.getFull(section, 'url', ArkimeConfig.exit), section);

    Integration.register(this);
  }

  // ----------------------------------------------------------------------------
  async #fetch (user, item, type) {
    if (this.#keyTemplate !== undefined) {
      item = this.#keyTemplate.replace('%key%', item).replace('%type%', this.type);
    }
    try {
      const result = await this.#redisClient[this.#redisMethod](item);

      if (result === undefined || result === null) {
        return Integration.NoResult;
      }
      return {
        data: JSON.parse(result),
        _cont3xt: {
          count: 1
        }
      };
    } catch (e) {
      return Integration.NoResult;
    }
  }

  // ----------------------------------------------------------------------------
  async ipFetch (user, item) { return this.#fetch(user, item, 'ip'); }
  async domainFetch (user, item) { return this.#fetch(user, item, 'domainip'); }
  async phoneFetch (user, item) { return this.#fetch(user, item, 'phone'); }
  async emailFetch (user, item) { return this.#fetch(user, item, 'email'); }
  async hashFetch (user, item) { return this.#fetch(user, item, 'hash'); }
  async urlFetch (user, item) { return this.#fetch(user, item, 'url'); }
  async textFetch (user, item) { return this.#fetch(user, item, 'text'); }
}

const sections = ArkimeConfig.getSections().filter((e) => { return e.match(/^redis:/); });
sections.forEach((section) => {
  // eslint-disable-next-line no-new
  new RedisIntegration(section, true);
});
