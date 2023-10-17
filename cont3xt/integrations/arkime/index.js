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

class ArkimeIntegration extends Integration {
  name;
  section;
  icon = '../assets/Arkime_Logo_Mark_FullGradient.png';
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
    fields: [
    ]
  };

  // ----------------------------------------------------------------------------

  #prefix;
  #client;
  #searchDays;
  #maxResults;

  // ----------------------------------------------------------------------------
  constructor (section) {
    super();

    this.section = section;
    this.name = ArkimeConfig.getFull(section, 'name', section);
    this.icon = ArkimeConfig.getFull(section, 'icon', this.icon);
    this.#prefix = ArkimeUtil.formatPrefix(ArkimeConfig.getFull(section, 'prefix'));
    this.#searchDays = parseInt(ArkimeConfig.getFull(section, 'searchDays', -1), 10);
    this.#maxResults = ArkimeConfig.getFull(section, 'maxResults', 20);

    const elasticsearch = ArkimeConfig.getFullArray(section, 'elasticsearch', 'http://localhost:9200');
    const elasticsearchAPIKey = ArkimeConfig.getFull(section, 'elasticsearchAPIKey');
    let elasticsearchBasicAuth = ArkimeConfig.getFull(section, 'elasticsearchBasicAuth');

    const options = {
      node: elasticsearch,
      requestTimeout: 300000,
      maxRetries: 2,
      ssl: {
        rejectUnauthorized: !!ArkimeConfig.getFull(section, 'insecure', true)
      }
    };

    if (elasticsearchAPIKey) {
      options.auth = {
        apiKey: elasticsearchAPIKey
      };
    } else if (elasticsearchBasicAuth) {
      if (!elasticsearchBasicAuth.includes(':')) {
        elasticsearchBasicAuth = Buffer.from(elasticsearchBasicAuth, 'base64').toString();
      }
      elasticsearchBasicAuth = ArkimeUtil.splitRemain(elasticsearchBasicAuth, ':', 1);
      options.auth = {
        username: elasticsearchBasicAuth[0],
        password: elasticsearchBasicAuth[1]
      };
    }

    try {
      this.#client = new Client(options);
    } catch (err) {
      console.log(section, 'ERROR - creating Elasticsearch client for new Arkime integration', err, options);
      return;
    }

    Integration.register(this);
  }

  // ----------------------------------------------------------------------------
  async doFetch (user, item, fields) {
    const query = {
      query: {
        bool: {
          must: [
            { range: { lastPacket: { gte: new Date() - (this.#searchDays * 7 * 24 * 60 * 60 * 1000) } } },
            { bool: { should: fields.map(field => { return { term: { [field]: item } }; }) } }
          ]
        }
      },
      sort: { lastPacket: { order: 'desc' } },
      size: this.#maxResults
    };

    if (this.#searchDays !== -1) {
      query.query.bool.must.shift();
    }

    const results = await this.#client.search({
      index: `${this.#prefix}sessions3-*`,
      body: query,
      rest_total_hits_as_int: true
    });

    if (results.statusCode !== 200 || results.body.hits.hits.length === 0) {
      return Integration.NoResult;
    }

    const data = {
      hits: results.body.hits.hits.map(i => {
        return i._source;
      }),
      _cont3xt: {
        count: results.body.hits.hits.length
      }
    };
    return data;
  }

  // ----------------------------------------------------------------------------
  async fetchDomain (user, item) {
    return this.doFetch(user, item, [
      'dhcp.host',
      'dns.host',
      'dns.host',
      'dns.mailserverHost',
      'dns.nameserverHost',
      'dns.hostTokens',
      'dns.mailserverHost',
      'dns.nameserverHost',
      'http.host',
      'quic.host',
      'smb.host',
      'socks.host',
      'oracle.host'
    ]);
  };

  // ----------------------------------------------------------------------------
  async fetchEmail (user, item) {
    return this.doFetch(user, item, [
      'email.dst',
      'email.src'
    ]);
  }

  // ----------------------------------------------------------------------------
  async fetchHash (user, item) {
    return this.doFetch(user, item, [
      'http.md5',
      'email.md5',
      'http.sha256',
      'email.sha256'
    ]);
  }

  // ----------------------------------------------------------------------------
  async fetchIp (user, item) {
    return this.doFetch(user, item, [
      'dns.ip',
      'dns.https.ip',
      'dns.mailserverIp',
      'dns.nameserverIp',
      'destination.ip',
      'socks.ip',
      'source.ip',
      'http.xffIp',
      'dstOuterIp',
      'srcOuterIp',
      'radius.endpointIp',
      'radius.framedIp'
    ]);
  }
}

// ----------------------------------------------------------------------------
const sections = ArkimeConfig.getSections().filter((e) => { return e.match(/^arkime:/); });
sections.forEach((section) => {
  // eslint-disable-next-line no-new
  new ArkimeIntegration(section);
});
