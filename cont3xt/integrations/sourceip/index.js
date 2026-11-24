/******************************************************************************/
/* SourceIP.io integration for Cont3xt
 *
 * Resolves an IP to the operator that publishes it, how that operator uses the
 * space (self_operated / shared_platform / general_use) and the ASN/RIR data.
 * Install into /opt/arkime/cont3xt/integrations/sourceip/ (see README.md).
 * API docs: https://sourceip.io/api
 */
const Integration = require('../../integration.js');
const axios = require('axios');

const ANON_URL = 'https://sourceip.io/api/v1/lookup';

class SourceIPIntegration extends Integration {
  name = 'SourceIP.io';
  section = 'sourceip';
  icon = 'integrations/sourceip/icon.png';
  order = 335;
  // the dataset is rebuilt daily and quota is small (100/day anon, 1000/day free key)
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
      help: 'Optional SourceIP.io API key (free key: 1,000 lookups/day instead of 100). Can also be set with the SOURCEIP_API_KEY environment variable.',
      password: true,
      required: false
    },
    endpoint: {
      help: 'Keyed API endpoint, normally https://api.sourceip.io (no trailing path). Both this and the key are needed for keyed lookups; otherwise the anonymous endpoint is used. Can also be set with SOURCEIP_ENDPOINT.',
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

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchIp (user, ip) {
    try {
      const key = this.getUserConfig(user, 'key') ?? process.env.SOURCEIP_API_KEY;
      const endpoint = (this.getUserConfig(user, 'endpoint') ?? process.env.SOURCEIP_ENDPOINT ?? '').replace(/\/+$/, '');

      const headers = { 'User-Agent': this.userAgent() };
      if (key) { headers['x-api-key'] = key; }
      const url = (key && endpoint) ? `${endpoint}/v1/lookup` : ANON_URL;

      const res = await axios.get(url, { params: { ip }, headers });
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
      // the API lists an origin once per source (rpki and irr); merge them into one row per asn+prefix
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
      const status = err?.response?.status;
      if (status === 404 || status === 400) {
        return Integration.NoResult; // unparseable / unknown address
      }
      if (status === 429) {
        console.log(this.name, ip, 'rate limited, quota resets at', err.response.headers?.['x-ratelimit-reset']);
        return undefined;
      }

      console.log(this.name, ip, err?.message ?? err);
      return undefined;
    }
  }
}

new SourceIPIntegration();
