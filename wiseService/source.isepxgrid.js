/******************************************************************************/
/* source.isepxgrid.js  -- WISE source for Cisco ISE pxGrid 2.0
 *
 * Enriches IPs with identity and endpoint data from ISE active sessions.
 *
 * Fields returned per IP:
 *   ise.user    – adUserResolvedIdentities  (fallback: userName)
 *   ise.mac     – macAddress
 *   ise.os      – endpointOperatingSystem   (fallback: endpointProfile)
 *   ise.posture – postureStatus
 *   ise.nasip   – nasIpAddress (network node the endpoint authenticated through)
 *
 * Maintains a persistent STOMP-over-WebSocket subscription to ISE's
 * /topic/com.cisco.ise.session topic and keeps an in-memory Map of active sessions
 * keyed by IP address.  All active sessions are bulk-loaded on startup via
 * getSessions (DISCONNECTED/TERMINATED entries skipped), then kept current by
 * STOMP events:
 *   - CONNECT/STARTED event  → upsert (add or overwrite) all IPs for that session;
 *     stale IPs from the same auditSessionId that are no longer present are purged
 *   - DISCONNECT event → remove all IPs ever associated with that auditSessionId
 *
 * The cache is cleared and rebuilt from ISE on:
 *   - Restart (Maps start empty, bulk load runs after STOMP subscribe)
 *   - cacheAgeMin interval (Maps cleared then repopulated via getSessions to ensure consistency with ISE)
 *   - WebSocket reconnect (cancels pending refresh; new bulk load re-arms it)
 *
 * getIp() is a pure Map lookup — if an IP isn't in the Map it has no active session.
 *
 * Auth model: mutual TLS (Arkime node certificate, cert-based pxGrid account).
 *   AccountActivate auto-creates the account on first run — no password needed.
 *   IMPORTANT: The CN of the client certificate must be DNS-resolvable by ISE to
 *   the source IP of the server running WISE.  ISE performs a reverse-lookup on
 *   the CN during the mTLS handshake to verify the connecting host; if the CN
 *   does not resolve to the source IP the connection will be rejected.
 *
 * Config section in wise.ini:
 *   [isepxgrid]
 *   host         = isepxgrid.example.com   (required, ISE pxGrid node hostname)
 *   port         = 8910              (optional, default 8910)
 *   certFile     = /opt/arkime/etc/arkime.cert
 *   keyFile      = /opt/arkime/etc/arkime.key
 *   caFile       = /opt/arkime/etc/cacert.crt   (optional)
 *   nodeName     = arkime-example-com   (required, Node Name for pxGrid account, typically the CN of the client certificate)
 *   cacheAgeMin  = 1440                (optional, default 1440 — full refresh interval)
 *   logEvents    = false             (optional, log ADD/UPDATE/REMOVE to wise.log)
 */

'use strict';

const WISESource = require('./wiseSource.js');
const https = require('https');
const fs = require('fs');
const axios = require('axios');
const WSClient = require('ws');

// ---------------------------------------------------------------------------
// Minimal STOMP 1.2 client over an already-open WebSocket
// ---------------------------------------------------------------------------
class StompClient {
  constructor (ws) {
    this.ws = ws;
    this._handlers = {};
    this._idsub = 0;
    this._subscriptions = {}; // id → callback
    ws.on('message', (raw) => this._onRaw(raw));
  }

  // Subscribe to a topic; cb(headers, body) called per MESSAGE.
  // Returns the subscription id.
  subscribe (topic, cb) {
    const id = `sub-${++this._idsub}`;
    this._subscriptions[id] = cb;
    this._send('SUBSCRIBE', { id, destination: topic, ack: 'auto' }, '');
    return id;
  }

  // Send STOMP CONNECT; returns Promise resolved with CONNECTED headers
  connect (login, passcode) {
    return new Promise((resolve, reject) => {
      this._onceFrame('CONNECTED', (headers) => resolve(headers));
      this._onceFrame('ERROR', (headers, body) =>
        reject(new Error(body || JSON.stringify(headers))));
      this._send('CONNECT', {
        'accept-version': '1.2,1.1,1.0',
        login,
        passcode,
        'heart-beat':     '0,0'
      }, '');
    });
  }

  disconnect () {
    try { this._send('DISCONNECT', {}, ''); } catch (_) {}
  }

  // ---- private ------------------------------------------------------------

  _send (command, headers, body) {
    let frame = command + '\n';
    for (const [k, v] of Object.entries(headers)) frame += `${k}:${v}\n`;
    frame += `\n${body}\0`;
    // ISE pxGrid pubsub requires binary WebSocket frames (not text frames).
    // Passing a Buffer causes ws to use opcode 0x02 (binary) instead of 0x01 (text).
    this.ws.send(Buffer.from(frame));
  }

  _onceFrame (command, cb) {
    if (!this._handlers[command]) this._handlers[command] = [];
    this._handlers[command].push({ once: true, cb });
  }

  _onRaw (raw) {
    const str = raw.toString();
    if (str === '\n' || str === '\r\n') return; // STOMP heartbeat

    const nullIdx = str.indexOf('\0');
    const frameStr = nullIdx >= 0 ? str.slice(0, nullIdx) : str;
    const nlIdx = frameStr.indexOf('\n');
    if (nlIdx < 0) return;

    const command = frameStr.slice(0, nlIdx).trim();
    const rest = frameStr.slice(nlIdx + 1);
    const blankIdx = rest.indexOf('\n\n');
    const rawHdrs = blankIdx >= 0 ? rest.slice(0, blankIdx) : rest;
    const body = blankIdx >= 0 ? rest.slice(blankIdx + 2) : '';

    const headers = {};
    for (const line of rawHdrs.split('\n')) {
      const colon = line.indexOf(':');
      if (colon > 0) headers[line.slice(0, colon)] = line.slice(colon + 1);
    }

    // Dispatch once-handlers (CONNECTED, ERROR)
    const once = (this._handlers[command] || []).filter(h => h.once);
    for (const h of once) h.cb(headers, body);
    this._handlers[command] = (this._handlers[command] || []).filter(h => !h.once);

    // Dispatch subscription callbacks for MESSAGE frames
    if (command === 'MESSAGE') {
      const cb = this._subscriptions[headers.subscription];
      if (cb) cb(headers, body);
    }
  }
}

// ---------------------------------------------------------------------------
class ISEPxGridSource extends WISESource {
  // -------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, {});

    // ---- Config ------------------------------------------------------------
    this.host = api.getConfig(section, 'host');
    this.port = +api.getConfig(section, 'port', 8910);
    this.certFile = api.getConfig(section, 'certFile', '');
    this.keyFile = api.getConfig(section, 'keyFile', '');
    this.caFile = api.getConfig(section, 'caFile', '');
    this.nodeName = api.getConfig(section, 'nodeName', '');
    this.logEvents = api.getConfig(section, 'logEvents', false);
    this.cacheAgeMin = +api.getConfig(section, 'cacheAgeMin', 1440); // 24h default

    if (!this.host) {
      console.log(this.section, '- ERROR: host is required');
      return;
    }

    for (const [label, path] of [['certFile', this.certFile], ['keyFile', this.keyFile]]) {
      if (!fs.existsSync(path)) {
        console.log(this.section, `- ERROR: ${label} not found: ${path}`);
        return;
      }
    }

    this.baseUrl = `https://${this.host}:${this.port}`;
    this.certBuf = fs.readFileSync(this.certFile);
    this.keyBuf = fs.readFileSync(this.keyFile);
    this.caBuf = (this.caFile && fs.existsSync(this.caFile))
      ? fs.readFileSync(this.caFile) : undefined;

    // Reusable mTLS HTTPS agent for control-plane and query calls
    this.agent = new https.Agent({
      cert:               this.certBuf,
      key:                this.keyBuf,
      ca:                 this.caBuf,
      rejectUnauthorized: true
    });

    // Axios instance for control-plane (AccountActivate, ServiceLookup, AccessSecret).
    // pxGrid requires HTTP Basic auth (nodeName : empty-password) even in cert-only mode;
    // the TLS mTLS handshake provides proof of identity, Basic is just the HTTP layer.
    this.ctrl = axios.create({
      baseURL:    this.baseUrl,
      httpsAgent: this.agent,
      headers:    { 'Content-Type': 'application/json', Accept: 'application/json' },
      auth:       { username: this.nodeName, password: '' },
      timeout:    10000
    });

    // ---- Service-plane endpoints & secrets (populated by activate()) -------
    this.sessionTopic = null;
    this.sessionRestUrl = null;  // restBaseUrl of com.cisco.ise.session
    this.sessionNodeName = null;
    this.sessionSecret = null;

    this.pubsubWsUrl = null;  // wsUrl of com.cisco.ise.pubsub
    this.pubsubNodeName = null;
    this.pubsubSecret = null;

    // ---- Subscribe-mode state ----------------------------------------------
    this.sessionMap = new Map();   // ip → session object
    this.sidMap = new Map();   // auditSessionId → Set<ip>
    this._stomp = null;
    this._wsReady = false;
    this._reconnectDelay = 5000;
    this._reconnectTimer = null;
    this._refreshTimer = null;

    // ---- Field definitions -------------------------------------------------
    this.userField = api.addField(
      'field:ise.user;db:ise.user;kind:termfield;' +
      'friendly:ISE User;help:Active directory user or username from ISE session;count:false'
    );
    this.macField = api.addField(
      'field:ise.mac;db:ise.mac;kind:termfield;' +
      'friendly:ISE MAC;help:Endpoint MAC address from ISE session;count:false'
    );
    this.osField = api.addField(
      'field:ise.os;db:ise.os;kind:termfield;' +
      'friendly:ISE OS;help:Endpoint OS or profile from ISE session;count:false'
    );
    this.postureField = api.addField(
      'field:ise.posture;db:ise.posture;kind:termfield;' +
      'friendly:ISE Posture;help:Endpoint posture compliance status from ISE;count:false'
    );
    this.nasipField = api.addField(
      'field:ise.nasip;db:ise.nasip;kind:termfield;' +
      'friendly:ISE NAS IP;help:Network node IP (switch/AP) the endpoint authenticated through;count:false'
    );

    api.addSource(section, this, ['ip']);

    console.log(this.section,
      `- Starting, pxGrid ${this.baseUrl} as ${this.nodeName}`);
    this._start();
  }

  // -------------------------------------------------------------------------
  // Startup
  // -------------------------------------------------------------------------

  async _start () {
    try {
      await this.activate();
      this._connect();
    } catch (err) {
      console.log(this.section, '- ERROR during startup:', err.message);
      this._scheduleReconnect();
    }
  }

  // -------------------------------------------------------------------------
  // Control-plane: AccountActivate → ServiceLookup × 2 → AccessSecret × 2
  // -------------------------------------------------------------------------

  async activate () {
    // 1. AccountActivate — cert-only mode, ISE auto-creates + enables account
    const actResp = await this.ctrl.post('/pxgrid/control/AccountActivate',
      { description: 'Arkime WISE pxGrid plugin' });
    const state = actResp.data?.accountState;
    if (state !== 'ENABLED') {
      throw new Error(`pxGrid account state is "${state}" — expected ENABLED`);
    }
    console.log(this.section,
      `- pxGrid account ENABLED (ISE ${actResp.data?.version})`);

    // 2. ServiceLookup: com.cisco.ise.session
    const sessResp = await this.ctrl.post('/pxgrid/control/ServiceLookup',
      { name: 'com.cisco.ise.session' });
    const sessNodes = sessResp.data?.services;
    if (!sessNodes?.length) throw new Error('com.cisco.ise.session not found');
    this.sessionRestUrl = sessNodes[0].properties?.restBaseUrl;
    this.sessionNodeName = sessNodes[0].nodeName;
    this.sessionTopic = sessNodes[0].properties?.sessionTopic ||
                           '/topic/com.cisco.ise.session';
    console.log(this.section,
      `- Session service: ${this.sessionNodeName} @ ${this.sessionRestUrl}`);
    console.log(this.section, `- Session topic: ${this.sessionTopic}`);

    // 3. ServiceLookup: com.cisco.ise.pubsub
    const pubResp = await this.ctrl.post('/pxgrid/control/ServiceLookup',
      { name: 'com.cisco.ise.pubsub' });
    const pubNodes = pubResp.data?.services;
    if (!pubNodes?.length) throw new Error('com.cisco.ise.pubsub not found');
    this.pubsubWsUrl = pubNodes[0].properties?.wsUrl;
    this.pubsubNodeName = pubNodes[0].nodeName;
    console.log(this.section,
      `- Pubsub service: ${this.pubsubNodeName} @ ${this.pubsubWsUrl}`);

    // 4a. AccessSecret for pubsub node (used for STOMP auth)
    const psecResp = await this.ctrl.post('/pxgrid/control/AccessSecret',
      { peerNodeName: this.pubsubNodeName });
    this.pubsubSecret = psecResp.data?.secret;
    if (!this.pubsubSecret) throw new Error('Empty pubsub AccessSecret');

    // 4b. AccessSecret for session node (used for REST getSessions bulk load)
    const ssecResp = await this.ctrl.post('/pxgrid/control/AccessSecret',
      { peerNodeName: this.sessionNodeName });
    this.sessionSecret = ssecResp.data?.secret;
    if (!this.sessionSecret) throw new Error('Empty session AccessSecret');
  }

  // -------------------------------------------------------------------------
  // Subscribe mode: WebSocket + STOMP connection
  // -------------------------------------------------------------------------

  _connect () {
    if (this._reconnectTimer) {
      clearTimeout(this._reconnectTimer);
      this._reconnectTimer = null;
    }

    console.log(this.section, `- Opening WebSocket to ${this.pubsubWsUrl}`);
    const authHeader = 'Basic ' +
      Buffer.from(`${this.nodeName}:${this.pubsubSecret}`).toString('base64');

    // ws v5 throws "Server sent no subprotocol" if protocols are passed as the second
    // arg and ISE doesn't echo them back.  Pass v12.stomp manually as a raw header so
    // ISE accepts the handshake but ws does not enforce the response subprotocol check.
    const ws = new WSClient(this.pubsubWsUrl, {
      cert:               this.certBuf,
      key:                this.keyBuf,
      ca:                 this.caBuf,
      rejectUnauthorized: true,
      headers:            {
        Authorization:          authHeader,
        'Sec-WebSocket-Protocol': 'v12.stomp'
      }
    });

    const stomp = new StompClient(ws);
    this._stomp = stomp;

    ws.on('error', (err) => {
      console.log(this.section, '- WebSocket error:', err.message);
    });

    ws.on('close', (code) => {
      this._wsReady = false;
      this._stomp = null;
      if (this._refreshTimer) { clearTimeout(this._refreshTimer); this._refreshTimer = null; }
      console.log(this.section, `- WebSocket closed (code ${code}), will reconnect`);
      this._scheduleReconnect();
    });

    ws.once('open', async () => {
      try {
        await stomp.connect(this.nodeName, this.pubsubSecret);
        this._wsReady = true;
        this._reconnectDelay = 5000;
        console.log(this.section, '- STOMP connected, subscribing to', this.sessionTopic);

        // Subscribe first so no events are missed during the bulk load.
        stomp.subscribe(this.sessionTopic, (headers, body) => {
          try {
            this._onSessionMessage(body);
          } catch (e) {
            console.log(this.section, '- ERROR processing session message:', e.message);
          }
        });

        // Bulk-load all active sessions so the Map is immediately complete.
        // Subscribe first (above) so no events are missed during the load.
        this._loadAllSessions().catch(err =>
          console.log(this.section, '- WARN: initial session load failed:', err.message));
      } catch (err) {
        console.log(this.section, '- STOMP connect error:', err.message);
        ws.close();
      }
    });
  }

  // -------------------------------------------------------------------------
  // Bulk-load all currently active sessions from ISE REST on startup
  // -------------------------------------------------------------------------

  async _loadAllSessions () {
    const doRequest = (secret) => {
      const creds = Buffer.from(`${this.nodeName}:${secret}`).toString('base64');
      return axios.post(
        `${this.sessionRestUrl}/getSessions`,
        {},
        {
          httpsAgent: this.agent,
          headers: {
            'Content-Type': 'application/json',
            Accept:          'application/json',
            Authorization:   `Basic ${creds}`
          },
          timeout: 30000
        }
      );
    };

    let resp;
    try {
      resp = await doRequest(this.sessionSecret);
    } catch (err) {
      if (err.response?.status === 401) {
        console.log(this.section, '- Session AccessSecret expired during bulk load, refreshing…');
        const secResp = await this.ctrl.post('/pxgrid/control/AccessSecret',
          { peerNodeName: this.sessionNodeName });
        this.sessionSecret = secResp.data?.secret;
        resp = await doRequest(this.sessionSecret);
      } else {
        throw err;
      }
    }

    const data = resp.data || {};
    const sessions = data.sessions ?? (Array.isArray(data) ? data : []);
    let count = 0;
    let skipped = 0;
    for (const s of sessions) {
      if (!s) continue;
      // ISE returns DISCONNECTED/TERMINATED sessions in getSessions — skip them
      if (s.state === 'DISCONNECTED' || s.state === 'TERMINATED') { skipped++; continue; }
      const ips = [];
      if (s.ipAddresses) ips.push(...(Array.isArray(s.ipAddresses) ? s.ipAddresses : [s.ipAddresses]));
      const sid = s.auditSessionId || '';
      for (const ip of ips) {
        // Only set if not already written by a concurrent STOMP event
        if (!this.sessionMap.has(ip)) {
          this.sessionMap.set(ip, s);
          count++;
        }
        if (sid) {
          if (!this.sidMap.has(sid)) this.sidMap.set(sid, new Set());
          this.sidMap.get(sid).add(ip);
        }
      }
    }
    console.log(this.section, `- Loaded ${count} sessions from getSessions (${sessions.length} total, ${skipped} disconnected skipped)`);
    this._scheduleRefresh();
  }

  _scheduleRefresh () {
    if (this._refreshTimer) clearTimeout(this._refreshTimer);
    const ms = this.cacheAgeMin * 60 * 1000;
    this._refreshTimer = setTimeout(() => this._refreshCache(), ms);
    this._refreshTimer.unref(); // don't hold the process open
  }

  async _refreshCache () {
    this._refreshTimer = null;
    console.log(this.section, `- Cache age reached ${this.cacheAgeMin}min, refreshing from ISE…`);
    this.sessionMap.clear();
    this.sidMap.clear();
    try {
      await this._loadAllSessions();
    } catch (err) {
      console.log(this.section, '- WARN: cache refresh failed:', err.message);
      this._scheduleRefresh(); // retry on next interval
    }
  }

  _scheduleReconnect () {
    if (this._reconnectTimer) return;
    const delay = this._reconnectDelay;
    this._reconnectDelay = Math.min(this._reconnectDelay * 2, 300000); // max 5 min
    console.log(this.section, `- Reconnecting in ${delay / 1000}s…`);
    this._reconnectTimer = setTimeout(async () => {
      this._reconnectTimer = null;
      try {
        // Re-activate to get fresh secrets before reconnecting
        await this.activate();
        this._connect();
      } catch (err) {
        console.log(this.section, '- ERROR during reconnect activate:', err.message);
        this._scheduleReconnect();
      }
    }, delay);
  }

  // -------------------------------------------------------------------------
  // Handle a STOMP message body (JSON) from ISE session topic
  // -------------------------------------------------------------------------

  _onSessionMessage (body) {
    if (!body) return;
    let msg;
    try { msg = JSON.parse(body); } catch (e) { return; }

    // ISE sends either {sessions:[...]} or a bare session object
    const sessions = msg.sessions ?? (Array.isArray(msg) ? msg : [msg]);

    for (const s of sessions) {
      if (!s) continue;
      const newIps = [];
      if (s.ipAddresses) {
        newIps.push(...(Array.isArray(s.ipAddresses) ? s.ipAddresses : [s.ipAddresses]));
      }

      const sid = s.auditSessionId || '';
      const user = s.adUserResolvedIdentities || s.userName || '';

      if (s.state === 'DISCONNECTED' || s.state === 'TERMINATED') {
        // Use sidMap to purge ALL IPs ever associated with this session
        const knownIps = (sid && this.sidMap.has(sid))
          ? [...this.sidMap.get(sid)]
          : newIps;
        for (const ip of knownIps) this.sessionMap.delete(ip);
        if (sid) this.sidMap.delete(sid);

        if (this.logEvents) {
          console.log(this.section, `- REMOVE [${knownIps.join(', ')}] state=${s.state} user=${user} sid=${sid}`);
        }
      } else {
        if (!newIps.length) continue;

        // Remove any STALE IPs for this session that are no longer in the new set
        const stalePurged = [];
        if (sid && this.sidMap.has(sid)) {
          for (const ip of this.sidMap.get(sid)) {
            if (!newIps.includes(ip)) {
              this.sessionMap.delete(ip);
              stalePurged.push(ip);
            }
          }
        }

        const isUpdate = this.logEvents && newIps.some(ip => this.sessionMap.has(ip));
        for (const ip of newIps) this.sessionMap.set(ip, s);
        if (sid) this.sidMap.set(sid, new Set(newIps));

        if (this.logEvents) {
          const staleNote = stalePurged.length ? ` (purged stale: ${stalePurged.join(', ')})` : '';
          console.log(this.section,
            `- ${isUpdate ? 'UPDATE' : 'ADD'} [${newIps.join(', ')}] state=${s.state} user=${user} sid=${sid}${staleNote}`);
        }
      }
    }
  }

  // -------------------------------------------------------------------------
  // Build WISE result from a session object
  // -------------------------------------------------------------------------

  _buildResult (session) {
    const args = [];

    const user = session.adUserResolvedIdentities || session.userName;
    if (user) args.push(this.userField, user);

    if (session.macAddress) args.push(this.macField, session.macAddress);

    const os = session.endpointOperatingSystem || session.endpointProfile;
    if (os) args.push(this.osField, os);

    if (session.postureStatus) args.push(this.postureField, session.postureStatus);

    if (session.nasIpAddress) args.push(this.nasipField, session.nasIpAddress);

    if (args.length === 0) return undefined;
    return WISESource.encodeResult.apply(null, args);
  }

  // -------------------------------------------------------------------------
  // WISE IP lookup
  // -------------------------------------------------------------------------

  async getIp (ip, cb) {
    try {
      const session = this.sessionMap.get(ip);
      return cb(null, session ? this._buildResult(session) : undefined);
    } catch (err) {
      console.log(this.section, '- ERROR looking up IP', ip, ':', err.message);
      return cb(null, undefined);
    }
  }

  // -------------------------------------------------------------------------
  // Debug dump endpoint
  // -------------------------------------------------------------------------

  dump (res) {
    const out = {};
    for (const [ip, s] of this.sessionMap) {
      out[ip] = {
        user:   s.adUserResolvedIdentities || s.userName,
        mac:    s.macAddress,
        os:     s.endpointOperatingSystem || s.endpointProfile,
        posture: s.postureStatus,
        nasip:  s.nasIpAddress,
        state:  s.state
      };
    }
    res.json({ wsReady: this._wsReady, sessions: out });
  }
}

// ---------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('isepxgrid', {
    singleton: false,
    name: 'isepxgrid',
    description: 'Enriches IPs with user identity, MAC, OS, and posture from Cisco ISE pxGrid 2.0 sessions.',
    types: ['ip'],
    fields: [
      { name: 'host', required: true, help: 'ISE pxGrid node hostname' },
      { name: 'port', required: false, help: 'pxGrid port (default 8910)' },
      { name: 'certFile', required: true, help: 'Path to mTLS client cert PEM' },
      { name: 'keyFile', required: true, help: 'Path to mTLS client key PEM' },
      { name: 'caFile', required: false, help: 'Path to ISE CA cert for server verification' },
      { name: 'nodeName', required: true, help: 'pxGrid account name' },
      { name: 'cacheAgeMin', required: false, help: 'How often (minutes) to fully refresh the session cache from ISE getSessions. Default: 1440 (24h).' },
      { name: 'logEvents', required: false, help: 'Set to true to log each STOMP session ADD/UPDATE/REMOVE event to the WISE log. Default: false.' }
    ]
  });

  const sections = api.getConfigSections().filter(s => s === 'isepxgrid' || s.startsWith('isepxgrid:'));
  sections.forEach(s => new ISEPxGridSource(api, s));
};
