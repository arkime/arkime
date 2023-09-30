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
    this.name = ArkimeConfig.get(section, 'name', section);
    this.icon = ArkimeConfig.get(section, 'icon',
      section.startsWith('elasticsearch') ? 'integrations/elasticsearch/elasticsearch.png' : 'integrations/elasticsearch/opensearch.png');
    this.order = ElasticsearchIntegration.#order++;
    this.card.title = `${this.name} for %{query}`;
    const itypes = ArkimeConfig.get(section, 'itypes', ArkimeConfig.exit);

    this.#index = ArkimeConfig.get(section, 'index', ArkimeConfig.exit);
    this.#method = ArkimeConfig.get(section, 'method', 'search');
    if (!this.#method.match(/^(get|search)$/)) {
      console.log(section, `- method must be get or search ${this.#method}`);
      return;
    }
    if (this.#method === 'search') {
      this.#queryField = ArkimeConfig.get(section, 'queryField', ArkimeConfig.exit);
      this.card.fields.push({ label: 'hits', type: 'json' });
    } else {
      this.card.fields.push({ label: 'data', type: 'json' });
    }

    const imethod = (this.#method === 'search') ? 'searchMethod' : 'getMethod';
    itypes.split(',').forEach(itype => {
      this.itypes[itype] = imethod;
    });
    this.#url = ArkimeConfig.get(section, 'url', ArkimeConfig.exit);
    this.#timestampField = ArkimeConfig.get(section, 'timestampField');

    this.#maxResults = ArkimeConfig.get(section, 'maxResults', 20);
    this.#includeId = ArkimeConfig.get(section, 'includeId', false);
    this.#includeIndex = ArkimeConfig.get(section, 'includeIndex', false);

    const options = {
      node: this.#url.split(',')[0],
      requestTimeout: 300000,
      maxRetries: 2,
      ssl: {
        rejectUnauthorized: !!ArkimeConfig.get(section, 'insecure', true)
      }
    };

    const apiKey = ArkimeConfig.get(section, 'apiKey');
    let basicAuth = ArkimeConfig.get(section, 'basicAuth');
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
