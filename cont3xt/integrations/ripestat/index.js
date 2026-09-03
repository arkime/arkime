/******************************************************************************/
/* Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

// whois falls back to these IANA placeholders when nothing real is registered
const WHOLE_SPACE = ['0.0.0.0/0', '0.0.0.0 - 255.255.255.255', '::/0'];

// whois key names differ per RIR, map them to one set of names
const WHOIS_ALIASES = {
  range: ['inetnum', 'netrange', 'cidr', 'inet6num'],
  netname: ['netname'],
  owner: ['organization', 'orgname', 'owner', 'descr', 'org'],
  status: ['status', 'nettype'],
  allocated: ['regdate', 'created']
};

class RIPEstatIntegration extends Integration {
  name = 'RIPEstat';
  icon = 'integrations/ripestat/icon.png';
  order = 340;
  cacheTimeout = '1w';
  itypes = {
    ip: 'fetch'
  };

  homePage = 'https://stat.ripe.net/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    },
    sourceapp: {
      help: 'Identifier sent to RIPEstat as the sourceapp parameter, RIPE asks for this when making more than 1000 requests a day'
    }
  };

  card = {
    title: 'RIPEstat for %{query}',
    searchUrls: [{
      url: 'https://stat.ripe.net/app/launchpad/%{query}',
      itypes: ['ip'],
      name: 'Search RIPEstat for IP: %{query}'
    }],
    fields: [
      {
        label: 'PTR',
        field: 'ptr',
        type: 'array',
        join: ', '
      },
      {
        label: 'Prefixes',
        field: 'prefixes',
        type: 'table',
        fields: [
          {
            label: 'AS#',
            field: 'asn'
          },
          {
            label: 'Prefix',
            field: 'prefix'
          },
          {
            label: 'ASName',
            field: 'holder'
          },
          {
            label: 'Announced',
            field: 'announced'
          }
        ]
      },
      {
        label: 'RIR Allocation',
        field: 'allocations',
        type: 'table',
        fields: [
          {
            label: 'RIR Name',
            field: 'rir'
          },
          {
            label: 'Prefix',
            field: 'range'
          },
          {
            label: 'Name',
            field: 'netname'
          },
          {
            label: 'Organization',
            field: 'owner'
          },
          {
            label: 'Date Allocated',
            field: 'allocated'
          },
          {
            label: 'Allocation Status',
            field: 'status'
          }
        ]
      },
      {
        label: 'Related Prefixes',
        field: 'relatedPrefixes',
        type: 'array',
        join: ', '
      },
      {
        label: 'Block',
        field: 'block'
      }
    ]
  };

  constructor () {
    super();

    Integration.register(this);
  }

  // RIPEstat returns each whois record as a list of key/value pairs, and the
  // key names depend on which RIR answered, so normalize to WHOIS_ALIASES
  static #normalizeWhois (record) {
    const seen = {};
    for (const kv of record ?? []) {
      const key = kv?.key?.toLowerCase();
      if (key && seen[key] === undefined && kv.value) { seen[key] = kv.value; }
    }

    const result = {};
    for (const [field, aliases] of Object.entries(WHOIS_ALIASES)) {
      result[field] = aliases.map(a => seen[a]).find(v => v !== undefined);
    }
    return result;
  }

  async #get (path, query, sourceapp) {
    const params = { resource: query };
    if (sourceapp) { params.sourceapp = sourceapp; }

    const res = await axios.get(`https://stat.ripe.net/data/${path}/data.json`, {
      params,
      headers: {
        Accept: 'application/json',
        'User-Agent': this.userAgent()
      }
    });

    return res.data?.status === 'ok' ? res.data.data : undefined;
  }

  async fetch (user, query) {
    try {
      const sourceapp = this.getUserConfig(user, 'sourceapp');

      const [prefix, reverse, whois, rir] = await Promise.all([
        this.#get('prefix-overview', query, sourceapp),
        this.#get('reverse-dns-ip', query, sourceapp),
        this.#get('whois', query, sourceapp),
        this.#get('rir', query, sourceapp)
      ]);

      const prefixes = (prefix?.asns ?? []).map(asn => ({
        asn: asn.asn,
        holder: asn.holder,
        prefix: prefix.resource,
        announced: prefix.announced
      }));

      // an unannounced prefix has no asns, still worth showing what it is
      if (prefixes.length === 0 && prefix?.resource) {
        prefixes.push({ prefix: prefix.resource, announced: prefix.announced });
      }

      const rirName = rir?.rirs?.[0]?.rir ?? whois?.authorities?.[0]?.toUpperCase();
      const allocations = (whois?.records ?? []).map(record => ({
        rir: rirName,
        ...RIPEstatIntegration.#normalizeWhois(record)
      })).filter(a => (a.range || a.netname) && !WHOLE_SPACE.includes(a.range));

      const ptr = reverse?.result ?? [];

      if (prefixes.length === 0 && allocations.length === 0 && ptr.length === 0) {
        return null;
      }

      return {
        ptr,
        prefixes,
        allocations,
        relatedPrefixes: prefix?.related_prefixes ?? [],
        block: prefix?.block?.desc,
        _cont3xt: { count: prefixes.length }
      };
    } catch (err) {
      console.log(this.name, query, err);
      return null;
    }
  }
}

new RIPEstatIntegration();
