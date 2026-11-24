/******************************************************************************/
/* SourceIP.io integration, contributed by Alan Keister (@AlanKK)
 *
 * Who operates a public IP and how they use it (self_operated,
 * shared_platform or general_use), plus the ASN origins and registry
 * allocation. https://sourceip.io/api
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const Integration = require('../../integration.js');
const axios = require('axios');

const ANON_URL = 'https://sourceip.io/api/v1/lookup';
const DEFAULT_ENDPOINT = 'https://api.sourceip.io';

class SourceIPIntegration extends Integration {
  name = 'SourceIP.io';
  section = 'sourceip';
  icon = 'integrations/sourceip/icon.png';
  order = 335;
  // the dataset is rebuilt daily and the quota is small (100/day anonymous, 1000/day with a free key)
  cacheTimeout = '1d';
  itypes = {
    ip: 'fetchIp'
  };

  homePage = 'https://sourceip.io/';
  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    },
    key: {
      help: 'Optional SourceIP.io API key, a free key allows 1,000 lookups a day instead of 100',
      password: true,
      required: false
    },
    endpoint: {
      help: `The endpoint that came with your key, default ${DEFAULT_ENDPOINT}. Only used with your own key`,
      required: false
    }
  };

  card = {
    title: 'SourceIP.io for %{query}',
    searchUrls: [{
      url: 'https://sourceip.io/lookup?ip=%{query}',
      itypes: ['ip'],
      name: 'Search SourceIP.io for %{query}'
    }],
    fields: [
      {
        label: 'Providers',
        field: 'providers',
        type: 'table',
        fields: [
          { label: 'Name', field: 'name' },
          { label: 'Category', field: 'category' },
          { label: 'Attribution', field: 'attribution' },
          { label: 'CIDR', field: 'cidr' },
          { label: 'Cloud Region', field: 'cloudRegion' },
          { label: 'Country', field: 'country' },
          { label: 'Region', field: 'region' },
          { label: 'City', field: 'city' }
        ]
      },
      {
        label: 'ASN Origins',
        field: 'origins',
        type: 'table',
        fields: [
          { label: 'AS#', field: 'asn' },
          { label: 'Name', field: 'name' },
          { label: 'Prefix', field: 'prefix' },
          { label: 'Country', field: 'country' },
          { label: 'Source', field: 'source' },
          { label: 'RPKI', field: 'rpki' }
        ]
      },
      {
        label: 'Registry Allocation',
        field: 'allocation',
        type: 'table',
        fields: [
          { label: 'RIR', field: 'rir' },
          { label: 'Prefix', field: 'prefix' },
          { label: 'Status', field: 'status' },
          { label: 'Allocated', field: 'allocated' },
          { label: 'Country', field: 'country' }
        ]
      },
      { label: 'Provider data updated', field: 'lastUpdated' },
      { label: 'ASN data updated', field: 'asnLastUpdated' }
    ]
  };

  tidbits = {
    order: 335,
    fields: [
      {
        tooltip: 'SourceIP.io provider',
        field: 'providers',
        fieldRoot: 'name',
        type: 'array',
        display: 'primaryGroup'
      }
    ]
  };

  #endpoint;
  #axiosTls;

  constructor () {
    super();

    this.#endpoint = this.getConfig('endpoint', DEFAULT_ENDPOINT).replace(/\/+$/, '');
    // insecure/caTrustFile/client certs apply to the configured endpoint only
    this.#axiosTls = this.axiosTls(this.#endpoint);

    Integration.register(this);
  }

  async fetchIp (user, ip) {
    try {
      const key = this.getUserConfig(user, 'key');
      // a user's own endpoint is only used with the user's own key, so the
      // configured key is never sent to a host a user picked
      let endpoint = this.#endpoint;
      if (this.hasUserConfig(user, 'key') && this.hasUserConfig(user, 'endpoint')) {
        endpoint = this.getUserConfig(user, 'endpoint').replace(/\/+$/, '');
      }

      const headers = { 'User-Agent': this.userAgent() };
      if (key) { headers['x-api-key'] = key; }
      const url = key ? `${endpoint}/v1/lookup` : ANON_URL;

      const res = await axios.get(url, {
        params: { ip },
        headers,
        timeout: 30 * 1000,
        ...(key && endpoint === this.#endpoint ? this.#axiosTls : {})
      });
      const d = res.data;

      const providers = (d.providers ?? []).map((p) => ({
        name: p.name,
        category: p.category,
        attribution: p.attribution,
        cidr: p.ip_cidr,
        cloudRegion: p.location?.cloud_region,
        country: p.location?.country,
        region: p.location?.region_name ?? p.location?.region,
        city: p.location?.city
      }));
      // the API lists an origin once per source (rpki and irr), merge them into one row per asn+prefix
      const merged = new Map();
      for (const o of (d.network?.origins ?? [])) {
        const k = `${o.asn}|${o.prefix}`;
        const m = merged.get(k);
        if (m === undefined) {
          merged.set(k, { asn: o.asn, name: o.name, prefix: o.prefix, country: o.country, source: o.source, rpki: o.rpki_status });
        } else {
          if (o.source && !m.source?.split(', ').includes(o.source)) { m.source = [m.source, o.source].filter(Boolean).join(', '); }
          if (m.rpki !== 'valid' && o.rpki_status) { m.rpki = o.rpki_status; }
        }
      }
      const origins = [...merged.values()];
      const a = d.network?.allocation;
      const allocation = a ? [{ rir: a.rir, prefix: a.prefix, status: a.status, allocated: a.allocated, country: a.country }] : [];

      if (providers.length === 0 && origins.length === 0 && allocation.length === 0) {
        return Integration.NoResult;
      }

      return {
        providers,
        origins,
        allocation,
        lastUpdated: d.last_updated,
        asnLastUpdated: d.asn_last_updated,
        // number shown on the integration button, one per matching provider entry
        _cont3xt: { count: providers.length }
      };
    } catch (err) {
      const httpStatus = err?.response?.status;
      if (httpStatus === 404 || httpStatus === 400) {
        return Integration.NoResult; // unparseable or unknown address
      }
      if (httpStatus === 429) {
        console.log(this.name, ip, 'rate limited, quota resets at', err.response.headers?.['x-ratelimit-reset']);
        return undefined;
      }

      console.log(this.name, ip, err?.message ?? err);
      return undefined;
    }
  }
}

new SourceIPIntegration();
