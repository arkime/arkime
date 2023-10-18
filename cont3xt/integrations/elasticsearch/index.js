/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const ArkimeConfig = require('../../../common/arkimeConfig');
const ArkimeUtil = require('../../../common/arkimeUtil');
const { Client } = require('@elastic/elasticsearch');

class ElasticsearchIntegration extends Integration {
  // Integration Items
  name;
  icon;
  order;
  itypes = {};

  // ES Items
  #index;
  #method;
  #queryField;
  #timestampField;
  #url;
  #client;
  #maxResults;
  #includeId;
  #includeIndex;

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
    this.icon = ArkimeConfig.getFull(section, 'icon',
      section.startsWith('elasticsearch') ? 'integrations/elasticsearch/elasticsearch.png' : 'integrations/elasticsearch/opensearch.png');
    this.order = ElasticsearchIntegration.#order++;
    this.card.title = `${this.name} for %{query}`;
    const itypes = ArkimeConfig.getFullArray(section, 'itypes', ArkimeConfig.exit);

    this.#index = ArkimeConfig.getFull(section, 'index', ArkimeConfig.exit);
    this.#method = ArkimeConfig.getFull(section, 'method', 'search');
    if (!this.#method.match(/^(get|search)$/)) {
      console.log(section, `- method must be get or search ${this.#method}`);
      return;
    }
    if (this.#method === 'search') {
      this.#queryField = ArkimeConfig.getFull(section, 'queryField', ArkimeConfig.exit);
      this.card.fields.push({ label: 'hits', type: 'json' });
    } else {
      this.card.fields.push({ label: 'data', type: 'json' });
    }

    const imethod = (this.#method === 'search') ? 'searchMethod' : 'getMethod';
    itypes.forEach(itype => {
      this.itypes[itype] = imethod;
    });
    this.#url = ArkimeConfig.getFull(section, 'url', ArkimeConfig.exit);
    this.#timestampField = ArkimeConfig.getFull(section, 'timestampField');

    this.#maxResults = ArkimeConfig.getFull(section, 'maxResults', 20);
    this.#includeId = ArkimeConfig.getFull(section, 'includeId', false);
    this.#includeIndex = ArkimeConfig.getFull(section, 'includeIndex', false);

    const options = {
      node: this.#url.split(',')[0],
      requestTimeout: 300000,
      maxRetries: 2,
      ssl: {
        rejectUnauthorized: !!ArkimeConfig.getFull(section, 'insecure', true)
      }
    };

    const apiKey = ArkimeConfig.getFull(section, 'apiKey');
    let basicAuth = ArkimeConfig.getFull(section, 'basicAuth');
    if (apiKey) {
      options.auth = {
        apiKey
      };
    } else if (basicAuth) {
      if (!basicAuth.includes(':')) {
        basicAuth = Buffer.from(basicAuth, 'base64').toString();
      }
      basicAuth = ArkimeUtil.splitRemain(basicAuth, ':', 1);
      options.auth = {
        username: basicAuth[0],
        password: basicAuth[1]
      };
    }

    try {
      this.#client = new Client(options);
    } catch (err) {
      console.log(section, 'ERROR - creating Elasticsearch client for new Elasticsearch integration', err, options);
      return;
    }

    Integration.register(this);
  }

  async searchMethod (user, item) {
    const query = {
      query: {
        bool: {
          filter: [
            { term: { } }
          ]
        }
      },
      sort: {},
      size: this.#maxResults
    };

    query.query.bool.filter[0].term[this.#queryField] = item;

    if (this.#timestampField) {
      query.sort[this.#timestampField] = { order: 'desc' };
    }

    const results = await this.#client.search({
      index: this.#index,
      body: query,
      rest_total_hits_as_int: true
    });

    if (results.statusCode !== 200 || results.body.hits.hits.length === 0) {
      return Integration.NoResult;
    }

    const data = {
      hits: results.body.hits.hits.map(i => {
        if (this.#includeId) {
          i._source._id = i._id;
        }
        if (this.#includeIndex) {
          i._source._index = i._index;
        }
        return i._source;
      }),
      _cont3xt: {
        count: results.body.hits.hits.length
      }
    };
    return data;
  };

  async getMethod (user, item) {
    const results = await this.#client.get({
      index: this.#index,
      id: item
    });

    if (results.statusCode !== 200 || !results?.body?._source) {
      return Integration.NoResult;
    }

    const data = {
      data: results.body._source,
      _cont3xt: {
        count: 1
      }
    };
    return data;
  };
}

const sections = ArkimeConfig.getSections().filter((e) => { return e.match(/^(elasticsearch|opensearch):/); });
sections.forEach((section) => {
  // eslint-disable-next-line no-new
  new ElasticsearchIntegration(section);
});
