/******************************************************************************/
/* source.phpipam.js  -- WISE source for phpIPAM IP address management
 *
 * Enriches IPs with hostname, subnet, VLAN, and VRF data from phpIPAM.
 *
 * Lookup behaviour:
 *   - Exact host match  → hostname + subnet + vlan + vrf
 *   - No host but IP falls inside a known subnet → subnet + vlan + vrf
 *     (most-specific / longest-prefix subnet wins via IPTrie)
 *   - No match at all   → session is not enriched
 *
 * Auth model: phpIPAM "App code" security.  The App Code is sent as a
 * static token on every request (phpipam-token header).  If your phpIPAM
 * is configured for "User token" auth instead, set useSessionAuth=true in
 * the config section and supply username + password.
 *
 * Cache strategy (preloadAddresses=true, the default):
 *   On startup and every `reload` minutes the plugin bulk-fetches:
 *     - All subnets  → IPTrie  (longest-prefix match)
 *     - All VLANs    → Map
 *     - All VRFs     → Map
 *     - All addresses per subnet  → Map (ip → {hostname, subnetInfo})
 *   getIp() is then 100% in-memory; phpIPAM is never called per session.
 *   Subnet address fetches run with a concurrency cap (`addrConcurrency`,
 *   default 10) to avoid hammering phpIPAM.
 *
 * With preloadAddresses=false the address cache is skipped and a single
 * live /addresses/search/{ip}/ call is made per unique IP (still wrapped
 * by WISE's own result cache).
 */

'use strict';

const WISESource = require('./wiseSource.js');
const axios = require('axios');
const iptrie = require('arkime-iptrie');

// ---------------------------------------------------------------------------
class PHPIPAMSource extends WISESource {
  // -------------------------------------------------------------------------
  constructor (api, section) {
    // dontCache=false  → WISE caches results; cacheAgeMin config still applies
    super(api, section, {});

    this.url = api.getConfig(section, 'url');
    this.appId = api.getConfig(section, 'appId');
    this.appCode = api.getConfig(section, 'appCode');
    // ArkimeConfig.getFull auto-converts 'true'/'false' strings to booleans
    this.useSessionAuth = api.getConfig(section, 'useSessionAuth', false) === true;
    this.verifyTLS = api.getConfig(section, 'verifyTLS', true) !== false;
    this.preloadAddresses = api.getConfig(section, 'preloadAddresses', true) !== false;
    this.addrConcurrency = +api.getConfig(section, 'addrConcurrency', 10);
    this.reloadMins = +api.getConfig(section, 'reload', 60);

    if (!this.url || !this.appId || !this.appCode) {
      console.log(this.section, '- ERROR: url, appId, and appCode are required');
      return;
    }

    // Normalise base URL – strip trailing slash
    this.url = this.url.replace(/\/+$/, '');

    // ---- Field definitions -------------------------------------------------
    // Base fields: used by WISE when encoding results. The capture plugin
    // remaps these at write time to src/dst variants via [custom-fields-remap]
    // in config.ini, so the base fields themselves are never stored in ES.
    this.hostnameField = api.addField(
      'field:ipam.hostname;db:ipam.hostname;kind:termfield;' +
      'friendly:IPAM Hostname;help:Hostname registered in phpIPAM;count:false'
    );
    this.subnetField = api.addField(
      'field:ipam.subnet;db:ipam.subnet;kind:termfield;' +
      'friendly:IPAM Subnet;help:Most-specific phpIPAM subnet (CIDR);count:false'
    );
    this.vlanField = api.addField(
      'field:ipam.vlan;db:ipam.vlan;kind:termfield;' +
      'friendly:IPAM VLAN;help:VLAN name/number from phpIPAM subnet;count:false'
    );
    this.vrfField = api.addField(
      'field:ipam.vrf;db:ipam.vrf;kind:termfield;' +
      'friendly:IPAM VRF;help:VRF name from phpIPAM subnet;count:false'
    );
    this.ipDescField = api.addField(
      'field:ipam.ip_desc;db:ipam.ip_desc;kind:termfield;' +
      'friendly:IPAM IP Description;help:Description of the IP address in phpIPAM;count:false'
    );
    this.subnetDescField = api.addField(
      'field:ipam.subnet_desc;db:ipam.subnet_desc;kind:termfield;' +
      'friendly:IPAM Subnet Description;help:Description of the subnet in phpIPAM;count:false'
    );
    this.vlanDescField = api.addField(
      'field:ipam.vlan_desc;db:ipam.vlan_desc;kind:termfield;' +
      'friendly:IPAM VLAN Description;help:Description of the VLAN in phpIPAM;count:false'
    );
    this.vrfDescField = api.addField(
      'field:ipam.vrf_desc;db:ipam.vrf_desc;kind:termfield;' +
      'friendly:IPAM VRF Description;help:Description of the VRF in phpIPAM;count:false'
    );

    // ---- Internal caches ---------------------------------------------------
    this.sessionToken = null;           // used only when useSessionAuth=true
    this.subnetTrie = new iptrie.IPTrie();
    this.subnetById = new Map();        // subnetId → subnetInfo
    this.vlanCache = new Map();         // vlanId  → display string
    this.vrfCache = new Map();          // vrfId   → display string
    this.hostCache = new Map();         // ip      → {hostname, subnetInfo}

    // Register as an IP source
    api.addSource(section, this, ['ip']);

    // ---- Value actions (right-click links into phpIPAM UI) -----------------
    // Use func (not url) so the viewer puts them in menuItems and opens
    // them as target="_blank" links instead of fetching inline.
    const base = this.url;
    api.addValueAction(`${section}_search_ip`, {
      name: 'phpIPAM: Search IP',
      func: `return { url: '${base}/tools/search/' + encodeURIComponent(value), name: 'phpIPAM: Search IP', value: value };`,
      category: 'ip'
    });
    api.addValueAction(`${section}_search_hostname`, {
      name: 'phpIPAM: Search Hostname',
      func: `return { url: '${base}/tools/search/' + encodeURIComponent(value), name: 'phpIPAM: Search Hostname', value: value };`,
      fields: 'ipam.src.hostname,ipam.dst.hostname'
    });
    api.addValueAction(`${section}_search_subnet`, {
      name: 'phpIPAM: Search Subnet',
      func: `return { url: '${base}/tools/search/' + encodeURIComponent(value), name: 'phpIPAM: Search Subnet', value: value };`,
      fields: 'ipam.src.subnet,ipam.dst.subnet'
    });
    api.addValueAction(`${section}_search_vlan`, {
      name: 'phpIPAM: Search VLAN',
      func: `return { url: '${base}/tools/search/' + encodeURIComponent(value), name: 'phpIPAM: Search VLAN', value: value };`,
      fields: 'ipam.src.vlan,ipam.dst.vlan'
    });
    api.addValueAction(`${section}_search_vrf`, {
      name: 'phpIPAM: Search VRF',
      func: `return { url: '${base}/tools/search/' + encodeURIComponent(value), name: 'phpIPAM: Search VRF', value: value };`,
      fields: 'ipam.src.vrf,ipam.dst.vrf'
    });

    // Initial cache load then periodic refresh
    this.loadCache().catch(err =>
      console.log(this.section, '- ERROR during initial load:', err.message)
    );
    if (this.reloadMins > 0) {
      setInterval(() => {
        this.loadCache().catch(err =>
          console.log(this.section, '- ERROR during reload:', err.message)
        );
      }, this.reloadMins * 60 * 1000);
    }
  }

  // -------------------------------------------------------------------------
  // Authentication helpers
  // -------------------------------------------------------------------------

  /**
   * Returns the Axios request headers needed for every phpIPAM API call.
   * For App Code auth the code is the static token.
   * For session auth a token obtained from /user/ is used.
   */
  authHeaders () {
    if (this.useSessionAuth) {
      return this.sessionToken ? { 'phpipam-token': this.sessionToken } : {};
    }
    // App Code auth – send the code directly as the token
    return { 'phpipam-token': this.appCode };
  }

  /**
   * Obtain a session token by POSTing to /user/.
   * Only called when useSessionAuth=true.
   */
  async authenticate () {
    const username = this.api.getConfig(this.section, 'username', this.appId);
    const password = this.api.getConfig(this.section, 'password', this.appCode);
    try {
      const resp = await axios.post(
        `${this.url}/api/${this.appId}/user/`,
        {},
        {
          auth: { username, password },
          httpsAgent: this.verifyTLS ? undefined : new (require('https').Agent)({ rejectUnauthorized: false })
        }
      );
      if (resp.data?.data?.token) {
        this.sessionToken = resp.data.data.token;
        return true;
      }
    } catch (e) {
      console.log(this.section, '- ERROR authenticating:', e.message);
    }
    return false;
  }

  // -------------------------------------------------------------------------
  // Generic API GET with token-refresh retry
  // -------------------------------------------------------------------------

  async apiGet (path) {
    // Ensure we have a session token if needed
    if (this.useSessionAuth && !this.sessionToken) {
      const ok = await this.authenticate();
      if (!ok) throw new Error('Authentication failed');
    }

    const doGet = () =>
      axios.get(`${this.url}/api/${this.appId}${path}`, {
        headers: this.authHeaders(),
        timeout: 10000,
        validateStatus: null,  // handle all status codes ourselves
        httpsAgent: this.verifyTLS ? undefined : new (require('https').Agent)({ rejectUnauthorized: false })
      });

    let resp = await doGet();

    // Refresh token on 401 (only relevant for session auth)
    if (resp.status === 401 && this.useSessionAuth) {
      this.sessionToken = null;
      await this.authenticate();
      resp = await doGet();
    }

    if (resp.status === 404) return null;

    if (resp.status !== 200) {
      throw new Error(`HTTP ${resp.status} for ${path}`);
    }

    return resp.data;
  }

  // -------------------------------------------------------------------------
  // Cache loading (subnets, VLANs, VRFs)
  // -------------------------------------------------------------------------

  async loadCache () {
    await Promise.all([this.loadVLANs(), this.loadVRFs()]);
    await this.loadSubnets();
    if (this.preloadAddresses) {
      await this.loadAddresses();
    }
    const addrCount = this.preloadAddresses ? this.hostCache.size : 0;
    console.log(this.section,
      `- Cache refreshed: ${this.subnetById.size} subnets, ` +
      `${this.vlanCache.size} VLANs, ${this.vrfCache.size} VRFs, ` +
      `${addrCount} hosts`);
  }

  // -------------------------------------------------------------------------
  // Concurrency-limited helper: run `tasks` (array of async fns) with at
  // most `limit` running simultaneously.
  // -------------------------------------------------------------------------
  async pLimit (tasks, limit) {
    const results = [];
    let idx = 0;
    async function worker () {
      while (idx < tasks.length) {
        const i = idx++;
        try { results[i] = await tasks[i](); } catch (_) { results[i] = null; }
      }
    }
    const workers = Array.from({ length: Math.min(limit, tasks.length) }, worker);
    await Promise.all(workers);
    return results;
  }

  // -------------------------------------------------------------------------
  // Bulk-load all registered addresses by iterating every known subnet.
  // -------------------------------------------------------------------------
  async loadAddresses () {
    const subnetIds = [...this.subnetById.keys()];
    if (subnetIds.length === 0) return;

    const newHostCache = new Map();
    let loaded = 0;
    let errors = 0;

    const tasks = subnetIds.map(id => async () => {
      try {
        const data = await this.apiGet(`/subnets/${id}/addresses/`);
        if (!data?.data) return; // 404 = empty subnet, normal
        for (const addr of data.data) {
          if (!addr.ip) continue;
          newHostCache.set(addr.ip, {
            hostname: addr.hostname || null,
            description: addr.description || null,
            subnetInfo: this.subnetById.get(String(addr.subnetId ?? id))
          });
        }
        loaded++;
      } catch (e) {
        errors++;
      }
    });

    await this.pLimit(tasks, this.addrConcurrency);
    this.hostCache = newHostCache;

    if (errors > 0) {
      console.log(this.section, `- WARN: ${errors}/${subnetIds.length} subnets failed during address load`);
    }
  }

  async loadVLANs () {
    try {
      const data = await this.apiGet('/vlan/');
      if (!data?.data) return;
      this.vlanCache.clear();
      for (const v of data.data) {
        // Store just the VLAN number
        const num = v.number || v.vlanId;
        const display = num ? String(num) : (v.name || String(v.vlanId ?? v.id));
        this.vlanCache.set(String(v.vlanId ?? v.id), { display, description: v.description || null });
      }
    } catch (e) {
      console.log(this.section, '- WARN: could not load VLANs:', e.message);
    }
  }

  async loadVRFs () {
    try {
      const data = await this.apiGet('/vrf/');
      if (!data?.data) return;
      this.vrfCache.clear();
      for (const v of data.data) {
        // phpIPAM uses vrfId (not id) as the primary key in vrf listings
        const key = String(v.vrfId ?? v.id);
        this.vrfCache.set(key, { name: v.name || v.rd || key, description: v.description || null });
      }
    } catch (e) {
      console.log(this.section, '- WARN: could not load VRFs:', e.message);
    }
  }

  async loadSubnets () {
    try {
      const data = await this.apiGet('/subnets/');
      if (!data?.data) return;

      const newTrie = new iptrie.IPTrie();
      const newById = new Map();

      for (const s of data.data) {
        if (!s.subnet || s.mask == null) continue;
        const prefixLen = +s.mask;
        const cidr = `${s.subnet}/${prefixLen}`;
        const info = {
          cidr,
          description: s.description || null,
          vlanId: s.vlanId ? String(s.vlanId) : null,
          vrfId: s.vrfId ? String(s.vrfId) : null
        };
        try {
          newTrie.add(s.subnet, prefixLen, info);
          newById.set(String(s.id), info);
        } catch (_) { /* skip malformed entries */ }
      }

      this.subnetTrie = newTrie;
      this.subnetById = newById;
    } catch (e) {
      console.log(this.section, '- ERROR: could not load subnets:', e.message);
    }
  }

  // -------------------------------------------------------------------------
  // Result builder
  // -------------------------------------------------------------------------

  /**
   * Given an optional hostname and an optional subnet info object (from the
   * trie), build the WISE encoded result buffer.
   */
  buildResult (hostname, ipDesc, subnetInfo) {
    const args = [];

    if (hostname) {
      args.push(this.hostnameField, hostname);
    }
    if (ipDesc) {
      args.push(this.ipDescField, ipDesc);
    }

    if (subnetInfo) {
      args.push(this.subnetField, subnetInfo.cidr);
      if (subnetInfo.description) {
        args.push(this.subnetDescField, subnetInfo.description);
      }

      if (subnetInfo.vlanId) {
        const vlan = this.vlanCache.get(subnetInfo.vlanId);
        if (vlan) {
          args.push(this.vlanField, vlan.display);
          if (vlan.description) args.push(this.vlanDescField, vlan.description);
        }
      }

      if (subnetInfo.vrfId) {
        const vrf = this.vrfCache.get(subnetInfo.vrfId);
        if (vrf) {
          args.push(this.vrfField, vrf.name);
          if (vrf.description) args.push(this.vrfDescField, vrf.description);
        }
      }
    }

    if (args.length === 0) return WISESource.emptyResult;
    return WISESource.encodeResult.apply(null, args);
  }

  // -------------------------------------------------------------------------
  // IP lookup – called by WISE for every source IP / destination IP
  // -------------------------------------------------------------------------

  async getIp (ip, cb) {
    try {
      if (this.preloadAddresses) {
        // -----------------------------------------------------------------
        // Fully cached path – zero phpIPAM calls per session
        // -----------------------------------------------------------------

        // 1. Check host cache for an exact registered address
        const hostEntry = this.hostCache.get(ip);
        if (hostEntry) {
          const subnetInfo = hostEntry.subnetInfo || this.subnetTrie.find(ip) || null;
          return cb(null, this.buildResult(hostEntry.hostname, hostEntry.description, subnetInfo));
        }

        // 2. No registered address – fall back to subnet trie (subnet-only enrichment)
        const subnetInfo = this.subnetTrie.find(ip) || null;
        if (!subnetInfo) return cb(null, undefined);
        return cb(null, this.buildResult(null, null, subnetInfo));

      } else {
        // -----------------------------------------------------------------
        // Live lookup path (preloadAddresses=false)
        // -----------------------------------------------------------------
        const subnetInfo = this.subnetTrie.find(ip) || null;
        let hostname = null;
        let ipDesc = null;
        let hostSubnetInfo = subnetInfo;

        try {
          const data = await this.apiGet(`/addresses/search/${encodeURIComponent(ip)}/`);
          if (data?.data?.length) {
            const addr = data.data[0];
            hostname = addr.hostname || null;
            ipDesc = addr.description || null;
            if (addr.subnetId) {
              const byId = this.subnetById.get(String(addr.subnetId));
              if (byId) hostSubnetInfo = byId;
            }
          }
        } catch (e) {
          if (e.response?.status === 401 && this.useSessionAuth) this.sessionToken = null;
        }

        if (!hostname && !ipDesc && !hostSubnetInfo) return cb(null, undefined);
        return cb(null, this.buildResult(hostname, ipDesc, hostSubnetInfo));
      }
    } catch (e) {
      console.log(this.section, '- ERROR looking up IP', ip, ':', e.message);
      return cb(null, undefined);
    }
  }
}

// ---------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('phpipam', {
    singleton: false,
    name: 'phpipam',
    description: 'Enriches IPs with hostname, subnet, VLAN, and VRF data from a phpIPAM server. ' +
      'By default all subnets, VLANs, VRFs, and registered addresses are bulk-loaded into memory hourly; ' +
      'getIp() is then 100% in-memory with zero per-session phpIPAM calls.',
    link: 'https://phpipam.net/api/api-documentation/',
    types: ['ip'],
    fields: [
      { name: 'url', required: true, help: 'phpIPAM base URL, e.g. https://ipam.example.com' },
      { name: 'appId', required: true, help: 'phpIPAM Application ID' },
      { name: 'appCode', required: true, help: 'phpIPAM Application Security Code (used as static token for App Code auth)' },
      { name: 'verifyTLS', required: false, help: 'Set false to disable TLS certificate verification (for self-signed/internal certs). Default true.' },
      { name: 'useSessionAuth', required: false, help: 'Set true to use user-token auth (POST /user/ to get session token). Default false.' },
      { name: 'username', required: false, help: 'Username for useSessionAuth=true. Defaults to appId.' },
      { name: 'password', required: false, help: 'Password for useSessionAuth=true. Defaults to appCode.' },
      { name: 'preloadAddresses', required: false, help: 'Bulk-load all registered addresses per subnet at startup/reload (default true). Set false to do live per-IP lookups instead.' },
      { name: 'addrConcurrency', required: false, help: 'Max parallel subnet address-fetch requests during preload. Default 10.' },
      { name: 'reload', required: false, help: 'Minutes between full cache refresh. Default 60.' },
      { name: 'cacheAgeMin', required: false, help: 'Minutes to cache per-IP WISE results. Default 60.' }
    ]
  });

  // Support multiple [phpipam:name] sections
  const sections = api.getConfigSections().filter(s => s === 'phpipam' || s.startsWith('phpipam:'));
  sections.forEach(s => new PHPIPAMSource(api, s));
};
