/******************************************************************************/
/* Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
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
    fields: [{
      label: 'Protocols',
      field: 'protocols',
      type: 'table',
      fields: [
        {
          label: 'Protocol',
          field: 'key'
        },
        {
          label: 'Count',
          field: 'doc_count'
        }
      ]
    }, {
      label: 'Sessions',
      field: 'hits',
      type: 'table',
      fields: [
        {
          label: 'Source IP',
          field: 'source.ip',
          pivot: true,
          options: {
            srcip: {
              field: {
                path: ['source', 'ip']
              },
              name: 'Arkime Src IP Query',
              href: '%{arkimeUrl}/sessions?expression=ip.src==%{value}'
            },
            id: {
              field: {
                path: ['id']
              },
              name: 'Arkime Session',
              href: '%{arkimeUrl}/sessions?expression=id==%{value}'
            },
            copy: 'Copy',
            pivot: 'Pivot'
          }
        },
        {
          label: 'Source Port',
          field: 'source.port'
        },
        {
          label: 'Destination IP',
          field: 'destination.ip',
          pivot: true,
          options: {
            dstip: {
              field: {
                path: ['destination', 'ip']
              },
              name: 'Arkime Dst IP Query',
              href: '%{arkimeUrl}/sessions?expression=ip.dst==%{value}'
            },
            id: {
              field: {
                path: ['id']
              },
              name: 'Arkime Session',
              href: '%{arkimeUrl}/sessions?expression=id==%{value}'
            },
            copy: 'Copy',
            pivot: 'Pivot'
          }
        },
        {
          label: 'Destination Port',
          field: 'destination.port'
        }
      ]
    }]
  };

  // ----------------------------------------------------------------------------

  #prefix;
  #client;
  #arkimeUrl;
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
    this.#arkimeUrl = ArkimeConfig.getFull(section, 'arkimeUrl', 'http://localhost:8005');
    if (this.#arkimeUrl.endsWith('/')) {
      this.#arkimeUrl = this.#arkimeUrl.slice(0, -1);
    }

    const time = this.#searchDays === -1 ? 'ALL' : `${this.#searchDays} days`;
    this.card.title = `${this.name} | Searching ${time} | Displaying ${this.#maxResults} results`;
    // replace href with correct arkime url and add date to query
    this.card.fields.forEach((field) => {
      if (!field.fields) { return; }
      field.fields.forEach((subfield) => {
        if (!subfield.options) { return; }
        for (const option in subfield.options) {
          if (subfield.options[option].href) {
            subfield.options[option].href = subfield.options[option].href.replace('%{arkimeUrl}', this.#arkimeUrl);
            subfield.options[option].href += `&date=${this.#searchDays === -1 ? -1 : this.#searchDays * 24}`;
          }
        }
      });
    });

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
      aggregations: {
        protocols: { terms: { field: 'protocol' } }
      },
      sort: { lastPacket: { order: 'desc' } },
      size: this.#maxResults
    };

    if (this.#searchDays === -1) {
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

    return {
      protocols: results.body.aggregations.protocols.buckets,
      hits: results.body.hits.hits.map(i => ({ ...i._source, id: i._id })),
      _cont3xt: {
        count: results.body.hits.total
      }
    };
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
