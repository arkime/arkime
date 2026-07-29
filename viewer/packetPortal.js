/******************************************************************************/
/* packetPortal.js -- second viewer-to-viewer transport
 *
 * A long-lived connection that an (often unreachable) "dialer" viewer opens
 * outbound to an "acceptor" viewer. The acceptor can then send requests back
 * down that connection, in the reverse of the direction it was dialed.
 *
 * Mechanics:
 *   1. The dialer sends an HTTP Upgrade request (Upgrade: arkime-portal) to the
 *      acceptor's normal viewer port, carrying an x-arkime-auth token that
 *      names the dialer's node. This solves reachability + identity and reuses
 *      the existing listener/TLS (Express never sees 'upgrade' requests).
 *   2. On the upgraded socket the roles are reversed relative to who dialed:
 *      the dialer runs an HTTP/2 *server* session, the acceptor runs an HTTP/2
 *      *client* session. Because Node lets either role attach to an existing
 *      socket, the acceptor (which only accepted the TCP connection) can now
 *      open request streams to the dialer.
 *   3. Each logical request is one HTTP/2 stream carrying an ordinary HTTP/1.1
 *      conversation in its DATA frames. Node's http client/server cannot use an
 *      Http2Stream directly as a socket (it hands http core pooled Buffers that
 *      get reused before the stream serializes them, corrupting the bytes), so
 *      each stream is bridged to a real net.Socket loopback pair: http talks to
 *      a genuine socket, and we pipe that socket <-> the Http2Stream. The
 *      dialer feeds its bridged socket into the existing Express app via
 *      http.createServer(app).emit('connection', socket) -- Express, its
 *      middleware, and the existing x-arkime-auth S2S check all run unchanged,
 *      behind a gate (see #gate) that requires a valid s2s token on everything
 *      arriving over a portal, including routes that need no auth normally.
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const net = require('net');
const fs = require('fs');
const streamLib = require('stream');
const http = require('http');
const https = require('https');
const http2 = require('http2');
const cryptoLib = require('crypto');
const basicAuth = require('basic-auth');
const Config = require('./config.js');
const Auth = require('../common/auth');
const ArkimeUtil = require('../common/arkimeUtil');
const ArkimeConfig = require('../common/arkimeConfig');

const PORTAL_PATH = '/packetPortal';
const UPGRADE_TOKEN = 'arkime-portal';
const REQUEST_TIMEOUT = 20 * 60 * 1000;

class PacketPortal {
  // acceptor side: node name -> { session, timer } for every live inbound portal
  static #portals = new Map();
  // acceptor side: node name -> ms timestamp of its last portal activity. Such a
  // node is behind NAT and CANNOT be dialed directly, so while it is known-reverse
  // we never fall back to a direct connection -- we use the portal or wait for it
  // to come back. Entries expire (packetPortalReverseTTLMinutes) so a node that
  // legitimately stops using portals becomes directly dialable again instead of
  // being unreachable until this viewer restarts.
  static #reverseNodes = new Map();
  // acceptor side: node name -> Set of resolve fns waiting for a (re)connect
  static #waiters = new Map();
  // acceptor side: per node { pass, ip } from [packetportal-nodes], falling back
  // to [esproxy-sensors] so a deployment that already authenticates its sensors
  // to esProxy gets the same per node identity here with no new config.
  static #nodes = {};
  // Was either section present? If so it is an allow list: an unlisted node can
  // never open a portal.
  static #nodesConfigured = false;
  // acceptor side: node name -> source ip, pinned on first sight. Only used for
  // nodes with no configured pass/ip (see #checkNode). In memory only, so a
  // restart re-opens the first-sight window for every node at once.
  static #pinnedIps = new Map();
  // dialer side: shared http1 server (wraps the Express app) fed inbound streams
  static #innerServer;
  // dialer side: shared h2 server that accepts inbound streams from acceptors
  static #dialerH2Server;
  // acceptor side: shared agent whose createConnection bridges to a portal
  static #agent;
  static #listenEnabled = false;
  static #initialized = false;
  static #hostnameRE = /^(?:[a-zA-Z0-9._-]+|\[[0-9a-fA-F:]+\])$/;

  // --------------------------------------------------------------------------
  // Called once at viewer startup with the Express app and the main viewer
  // server. Builds the dialer-side servers, starts dialing any configured
  // acceptors, and (for an acceptor) starts listening for inbound portals --
  // either on a dedicated port or, in shared mode, on the main viewer port.
  static init (app, mainServer) {
    if (PacketPortal.#initialized) { return; }
    PacketPortal.#initialized = true;

    // Inner HTTP/1.1 server sharing the same Express app. Never .listen()s; it
    // only ever receives sockets via emit('connection'). Used on the DIALER to
    // serve requests that arrive over a portal, behind the s2s gate.
    PacketPortal.#innerServer = http.createServer(PacketPortal.#gate(app));
    PacketPortal.#innerServer.setTimeout(REQUEST_TIMEOUT);

    // DIALER h2 server: each inbound stream is one HTTP/1.1 request.
    PacketPortal.#dialerH2Server = http2.createServer();
    PacketPortal.#dialerH2Server.on('stream', PacketPortal.#onDialerStream);
    PacketPortal.#dialerH2Server.on('session', (session) => {
      session.on('error', (e) => console.log('packetPortal: dialer session error', e.message));
    });

    for (const target of Config.getArray('packetPortalConnect', '')) {
      const t = target.trim();
      if (t.length > 0) { PacketPortal.#startDialer(t); }
    }

    // Per node credentials for inbound portals. [packetportal-nodes] wins,
    // [esproxy-sensors] is the fallback -- same 'pass:X;ip:A,B' shape, and a
    // deployment locking sensors down at esProxy wants the same list here.
    // Whether the section had any entries at all is what makes it an allow list,
    // so dropping the unusable ones below can never turn the allow list off.
    const nodes = Config.configMap('packetportal-nodes', 'esproxy-sensors');
    PacketPortal.#nodesConfigured = Object.keys(nodes).length > 0;
    PacketPortal.#nodes = {};

    for (const nodeName of Object.keys(nodes)) {
      const entry = nodes[nodeName];
      if (typeof entry.ip === 'string') {
        entry.ip = entry.ip.split(',').map(s => s.trim()).filter(s => s.length > 0);
      }
      // An entry with neither is no better than no entry at all -- it would fall
      // through to trust on first use, silently, for a node the admin believes
      // they configured. Drop it so it is rejected instead.
      if (entry.pass === undefined && (entry.ip === undefined || entry.ip.length === 0)) {
        console.log(`WARNING - packetPortal: ignoring node '${ArkimeUtil.sanitizeStr(nodeName)}', it has no pass or ip; it will not be allowed to open a portal`);
        continue;
      }
      // A name that resolves to something on Object.prototype can never be a real
      // node, and looking it up would find an inherited value instead of an entry
      if (ArkimeUtil.isPP(nodeName)) {
        console.log(`WARNING - packetPortal: ignoring node '${ArkimeUtil.sanitizeStr(nodeName)}', it is not a usable node name`);
        continue;
      }
      PacketPortal.#nodes[nodeName] = entry;
    }

    // ACCEPTOR listener. A dedicated port is recommended when the main viewer
    // port is fronted by a proxy, uses header auth, or is IP locked down --
    // none of which suit dialers coming from behind NAT. The portal is still
    // protected by the serverSecret handshake (and TLS when key/cert are set).
    const portalPort = Config.get('packetPortalPort');
    if (portalPort || Config.get('packetPortalListen', false)) {
      if (PacketPortal.#nodesConfigured) {
        console.log(`packetPortal: authenticating inbound portals against ${Object.keys(PacketPortal.#nodes).length} configured node(s)`);
      } else {
        console.log('packetPortal: no [packetportal-nodes] or [esproxy-sensors] section - each node will be pinned to the source ip it first connects from; add a per node pass to authenticate properly');
      }
    }

    if (portalPort) {
      PacketPortal.#listenEnabled = true;
      PacketPortal.#startAcceptorPort(portalPort, Config.get('packetPortalHost', undefined));
    } else if (Config.get('packetPortalListen', false)) {
      // Shared mode: accept portals on the existing viewer listener.
      PacketPortal.#listenEnabled = true;
      if (mainServer) {
        mainServer.on('upgrade', (req, socket, head) => PacketPortal.handleUpgrade(req, socket, head));
      }
    }
  }

  // --------------------------------------------------------------------------
  // ACCEPTOR: stand up a dedicated listener that only accepts portal upgrades.
  // Reuses the viewer's key/cert for TLS when configured. Independent of the
  // main viewer server, including its own cert watch so a rotated cert is
  // picked up here too instead of being served until the viewer restarts.
  static #startAcceptorPort (port, host) {
    const reject = (req, res) => { res.statusCode = 404; res.end('packet portal endpoint\n'); };

    let server;
    // TLS is optional and independent of the main viewer. Use portal-specific
    // key/cert when given (the usual case: the viewer itself is behind a
    // TLS-terminating proxy and has no cert of its own), else fall back to the
    // viewer's key/cert, else listen plain. Plain leaves pcap in the clear -
    // only use it on a trusted network; the serverSecret handshake still applies.
    const keyFile = Config.get('packetPortalKeyFile', ArkimeConfig.get('keyFile'));
    const certFile = Config.get('packetPortalCertFile', ArkimeConfig.get('certFile'));
    if (keyFile && certFile) {
      server = https.createServer({
        key: fs.readFileSync(keyFile),
        cert: fs.readFileSync(certFile),
        secureOptions: cryptoLib.constants.SSL_OP_NO_TLSv1
      }, reject);
      ArkimeUtil.watchCertFiles(server, keyFile, certFile);
    } else {
      server = http.createServer(reject);
      console.log('packetPortal: dedicated port is PLAIN (no TLS) - set packetPortalKeyFile/packetPortalCertFile for encryption');
    }

    server.setTimeout(REQUEST_TIMEOUT);
    server.on('upgrade', (req, socket, head) => PacketPortal.handleUpgrade(req, socket, head));
    server.on('error', (e) => {
      console.log(`packetPortal: cannot listen for portals on ${host ?? '*'}:${port} -`, e.message);
      process.exit(1);
    });
    server.on('listening', () => {
      console.log(`packetPortal: listening for inbound portals on ${host ?? '*'} port ${port}`);
    });
    server.listen({ port, host });
  }

  // --------------------------------------------------------------------------
  // ACCEPTOR: return a live h2 client session for node, or null. Callers set it
  // on the outbound request options as packetPortalSession and use agent().
  static get (node) {
    if (Array.isArray(node)) { node = node[0]; }
    const t = PacketPortal.#portals.get(node);
    if (!t) { return null; }
    if (t.session.destroyed || t.session.closed) {
      if (t.timer) { clearInterval(t.timer); }
      PacketPortal.#portals.delete(node);
      return null;
    }
    return t.session;
  }

  // --------------------------------------------------------------------------
  // ACCEPTOR: has this node recently connected via a packet portal? If so it is
  // unreachable directly and callers must not fall back to a direct connection.
  // Goes stale after packetPortalReverseTTLMinutes of no portal so a node that
  // moves back to being directly dialable recovers on its own.
  static isReverse (node) {
    if (Array.isArray(node)) { node = node[0]; }
    const last = PacketPortal.#reverseNodes.get(node);
    if (last === undefined) { return false; }
    if (Date.now() - last > PacketPortal.#reverseTTL()) {
      PacketPortal.#reverseNodes.delete(node);
      console.log(`packetPortal: node ${node} has had no portal for ${Config.get('packetPortalReverseTTLMinutes', 60)} minutes, allowing direct connections again`);
      return false;
    }
    return true;
  }

  // --------------------------------------------------------------------------
  static #reverseTTL () {
    const mins = +Config.get('packetPortalReverseTTLMinutes', 60);
    return (isNaN(mins) || mins <= 0 ? 60 : mins) * 60000;
  }

  // --------------------------------------------------------------------------
  // ACCEPTOR: node names with a currently live inbound portal. Used by the
  // regressionTests status endpoint.
  static connectedNodes () {
    const nodes = [];
    for (const [node, entry] of PacketPortal.#portals) {
      if (!entry.session.destroyed && !entry.session.closed) { nodes.push(node); }
    }
    return nodes;
  }

  // --------------------------------------------------------------------------
  // ACCEPTOR: resolve a live session for node, waiting up to timeoutMs for a
  // packet portal to (re)connect. Resolves the session, or null if the node is
  // not a reverse node (caller may then dial directly) or the wait expired.
  static getOrWait (node, timeoutMs) {
    if (Array.isArray(node)) { node = node[0]; }
    const existing = PacketPortal.get(node);
    if (existing) { return Promise.resolve(existing); }
    if (!PacketPortal.isReverse(node)) { return Promise.resolve(null); }

    return new Promise((resolve) => {
      let set = PacketPortal.#waiters.get(node);
      if (!set) { set = new Set(); PacketPortal.#waiters.set(node, set); }

      const waiter = (session) => { clearTimeout(timer); resolve(session); };
      const timer = setTimeout(() => {
        set.delete(waiter);
        if (set.size === 0) { PacketPortal.#waiters.delete(node); }
        resolve(null);
      }, timeoutMs);
      if (timer.unref) { timer.unref(); }
      set.add(waiter);

      // Cover the race where a portal registered between get() and add().
      const now = PacketPortal.get(node);
      if (now) { set.delete(waiter); clearTimeout(timer); resolve(now); }
    });
  }

  // --------------------------------------------------------------------------
  // ACCEPTOR: shared http.Agent that turns each outbound request into a fresh
  // portal stream. The caller puts the portal session on
  // options.packetPortalSession; createConnection bridges it to a real socket.
  static get agent () {
    if (!PacketPortal.#agent) {
      PacketPortal.#agent = new http.Agent({ keepAlive: false, maxSockets: Infinity });
      PacketPortal.#agent.createConnection = (options, cb) => {
        const session = options.packetPortalSession;
        if (!session || session.destroyed || session.closed) {
          cb(new Error('packetPortal session is gone'));
          return;
        }
        const stream = session.request({
          ':method': 'POST',
          ':scheme': 'http',
          ':authority': 'arkime-portal.invalid',
          ':path': '/'
        });
        stream.on('error', () => {});
        PacketPortal.#bridge(stream, cb);
      };
    }
    return PacketPortal.#agent;
  }

  // --------------------------------------------------------------------------
  // ACCEPTOR: handle a viewer-port 'upgrade' event. Validates the dialer's
  // identity, replies 101, and attaches an h2 client session to the socket.
  static handleUpgrade (req, socket, head) {
    // Every path below can destroy the socket, so swallow its errors first
    socket.on('error', () => {});

    // Only the arkime-portal upgrade is ours. Anything else (h2c, websocket,
    // ...) has never been supported by the viewer; Node closed such upgrades by
    // default when there was no 'upgrade' listener, so preserve that exactly.
    // Normal clients (including HTTP/2 via ALPN) never take the upgrade path.
    if ((req.headers.upgrade ?? '').toLowerCase() !== UPGRADE_TOKEN) {
      socket.destroy();
      return;
    }

    if (!PacketPortal.#listenEnabled) { socket.destroy(); return; }

    // The claimed node name travels in the clear so an unknown, unauthorized or
    // wrong-source peer is turned away BEFORE auth2obj, whose pbkdf2 costs ~100ms
    // of blocking event loop per unseen salt. It is only a claim at this point;
    // #checkNode is what decides whether this peer may make it.
    const node = req.headers['x-arkime-node'];
    if (!ArkimeUtil.isString(node) || !PacketPortal.#hostnameRE.test(node) || ArkimeUtil.isPP(node)) {
      console.log('packetPortal: rejecting upgrade, missing or bad x-arkime-node');
      socket.destroy();
      return;
    }

    const nodeError = PacketPortal.#checkNode(node, req);
    if (nodeError) {
      console.log(`packetPortal: rejecting upgrade for node '${node}' -`, nodeError);
      socket.destroy();
      return;
    }

    const { obj, error } = Auth.parseS2SRequest(req);
    if (error) {
      console.log('packetPortal: rejecting upgrade -', error);
      socket.destroy();
      return;
    }

    // The signed token must agree with the cleartext claim we authenticated
    if (obj.node !== node) {
      console.log(`packetPortal: rejecting upgrade, x-arkime-node '${node}' does not match token node '${ArkimeUtil.sanitizeStr(obj.node)}'`);
      socket.destroy();
      return;
    }

    socket.setTimeout(0);
    if (head && head.length > 0) { socket.unshift(head); }

    // Attach h2 only once the 101 has flushed -- a write still queued when http2
    // takes the socket aborts the process on teardown over TLS (nodejs/node#24037)
    socket.write('HTTP/1.1 101 Switching Protocols\r\n' +
                 'Upgrade: ' + UPGRADE_TOKEN + '\r\n' +
                 'Connection: Upgrade\r\n\r\n', (err) => {
      if (err || socket.destroyed) { socket.destroy(); return; }
      PacketPortal.#registerPortal(node, socket, req);
    });
  }

  // --------------------------------------------------------------------------
  // ACCEPTOR: 101 is on the wire, attach the h2 client session and register it.
  static #registerPortal (node, socket, req) {
    const session = http2.connect('http://arkime-portal.invalid', {
      createConnection: () => socket
    });
    session.on('error', (e) => console.log(`packetPortal: client session error for ${node} -`, e.message));
    session.on('close', () => {
      const entry = PacketPortal.#portals.get(node);
      if (entry?.session === session) {
        if (entry.timer) { clearInterval(entry.timer); }
        PacketPortal.#portals.delete(node);
        // Restart the reverse TTL from the disconnect, not from the connect
        PacketPortal.#reverseNodes.set(node, Date.now());
        console.log(`packetPortal: portal for node ${node} closed`);
      }
    });

    const prev = PacketPortal.#portals.get(node);
    if (prev && prev.session !== session) {
      if (prev.timer) { clearInterval(prev.timer); }
      try { prev.session.close(); } catch (e) { /* ignore */ }
    }

    // Periodically call the viewer's existing no-auth /health endpoint over the
    // portal so idle NAT / firewall / proxy mappings don't drop the connection,
    // and so a dead portal is detected -- a failed keepalive tears it down,
    // prompting the dialer to reconnect.
    let timer;
    const keepAliveSecs = Config.get('packetPortalKeepAliveSeconds', 30);
    if (keepAliveSecs > 0) {
      timer = setInterval(() => PacketPortal.#sendKeepAlive(node, session), keepAliveSecs * 1000);
      if (timer.unref) { timer.unref(); }
    }

    PacketPortal.#portals.set(node, { session, timer });
    PacketPortal.#reverseNodes.set(node, Date.now());
    console.log(`packetPortal: registered inbound portal for node ${node} from ${PacketPortal.#peerIp(req)}`);

    // Wake anything waiting for this node's portal to (re)connect.
    const waiters = PacketPortal.#waiters.get(node);
    if (waiters) {
      PacketPortal.#waiters.delete(node);
      for (const waiter of waiters) { waiter(session); }
    }
  }

  // --------------------------------------------------------------------------
  // ACCEPTOR: decide whether this peer may open a portal AS node. Returns an
  // error string to reject with, or undefined to allow.
  //
  // A valid x-arkime-auth token only proves the peer holds serverSecret, which
  // every node in the deployment has -- on its own it would let any one of them
  // claim (and so hijack or deny) any other node's traffic. This is what binds
  // the claim to the peer.
  static #checkNode (node, req) {
    // Own properties only. A plain [] lookup with a name like __proto__ or
    // constructor finds something on Object.prototype, which is not undefined and
    // so would sail past the allow list check below with no pass and no ip.
    const entry = Object.hasOwn(PacketPortal.#nodes, node) ? PacketPortal.#nodes[node] : undefined;

    // A configured section is an allow list, so an unlisted node is never ok
    if (PacketPortal.#nodesConfigured && entry === undefined) {
      return 'not in [packetportal-nodes]/[esproxy-sensors]';
    }

    const ip = PacketPortal.#peerIp(req);
    if (ip === undefined) { return 'no source ip'; }

    let authenticated = false;

    if (entry?.pass !== undefined) {
      const creds = basicAuth(req);
      if (!creds) { return 'no credentials supplied'; }
      if (creds.name !== node) { return `credentials are for '${ArkimeUtil.sanitizeStr(creds.name)}' not '${node}'`; }
      // hmac both sides so timingSafeEqual gets equal length buffers
      const expected = cryptoLib.createHmac('sha256', 'compare').update(entry.pass).digest();
      const got = cryptoLib.createHmac('sha256', 'compare').update(creds.pass).digest();
      if (!cryptoLib.timingSafeEqual(expected, got)) { return 'incorrect password'; }
      authenticated = true;
    }

    if (entry?.ip !== undefined) {
      if (!entry.ip.includes(ip)) { return `source ip ${ip} not allowed`; }
      authenticated = true;
    }

    if (authenticated) { return undefined; }

    // Nothing configured for this node, so trust the source ip we first saw it
    // on and require later portals to match (trust on first use). Weaker than a
    // pass -- an attacker who gets in before the real node does wins the pin --
    // but it keeps a no-config deployment from letting anyone claim any node.
    const pinned = PacketPortal.#pinnedIps.get(node);
    if (pinned === undefined) {
      PacketPortal.#pinnedIps.set(node, ip);
      console.log(`packetPortal: pinning node '${node}' to source ip ${ip}; set a pass for it in [packetportal-nodes] to authenticate properly`);
      return undefined;
    }
    if (pinned !== ip) { return `source ip ${ip} does not match pinned ${pinned}`; }
    return undefined;
  }

  // --------------------------------------------------------------------------
  // Real socket peer, never a header -- an x-forwarded-for is the peer's to set.
  static #peerIp (req) {
    const ip = req.socket?.remoteAddress;
    if (ip === undefined) { return undefined; }
    return ip.startsWith('::ffff:') ? ip.substring(7) : ip;
  }

  // --------------------------------------------------------------------------
  // Strip any user:pass before a packetPortalConnect target reaches the log
  static #redact (target) {
    try {
      const u = new URL(target);
      if (u.username || u.password) { u.username = ''; u.password = ''; return u.toString(); }
    } catch (e) { /* not a url, fall through */ }
    return target;
  }

  // --------------------------------------------------------------------------
  // ACCEPTOR: send one keepalive request over a portal. A failure closes the
  // session, which the dialer notices and reconnects.
  static #sendKeepAlive (node, session) {
    if (session.destroyed || session.closed) { return; }

    const options = {
      method: 'GET',
      agent: PacketPortal.agent,
      packetPortalSession: session,
      timeout: 30000,
      // /health needs no auth over a normal connection, but the dialer requires
      // an s2s token on everything arriving over a portal (see #gate)
      headers: {
        'x-arkime-auth': Auth.obj2auth({
          date: Date.now(),
          user: 'packetPortal',
          node,
          path: '/health',
          method: 'GET'
        })
      }
    };

    const req = http.request('http://arkime-portal.invalid/health', options, (res) => {
      res.resume(); // drain and discard
    });
    req.on('timeout', () => req.destroy(new Error('timeout')));
    req.on('error', (e) => {
      console.log(`packetPortal: keepalive to ${node} failed (${e.message}); closing portal`);
      try { session.close(); } catch (ignore) { /* ignore */ }
    });
    req.end();
  }

  // --------------------------------------------------------------------------
  // DIALER: gate every request arriving over a portal on a valid s2s token,
  // before the app sees it.
  //
  // The peer is remote but reaches us through a loopback bridge socket, so any
  // check based on the source ip (userAuthIps' loopback default, mcpAllowedIps)
  // or on a header a proxy would normally set (userNameHeader) would wrongly
  // trust it. Auth.doAuth knows about this (see arkimePacketPortal), but routes
  // mounted before Auth.app() -- /mcp with its own header auth, pre-auth
  // /plugin/*, /health, the parliament and eshealth endpoints -- never reach it.
  // Nothing legitimately arrives over a portal without a token, so require one
  // for everything, which also keeps the unauthenticated surface identical no
  // matter what a deployment mounts early.
  static #gate (app) {
    return (req, res) => {
      const { error } = Auth.parseS2SRequest(req);
      if (error) {
        console.log(`packetPortal: rejecting portal request for ${ArkimeUtil.sanitizeStr(req.url)} -`, error);
        res.statusCode = 401;
        res.setHeader('Content-Type', 'application/json');
        res.end(JSON.stringify({ success: false, text: 'Packet portal requests require s2s auth' }));
        return;
      }
      app(req, res);
    };
  }

  // --------------------------------------------------------------------------
  // DIALER: an acceptor opened a stream -> serve it as one HTTP/1.1 request
  // against the shared Express app.
  static #onDialerStream (stream) {
    try {
      stream.respond({ ':status': 200 });
    } catch (e) {
      try { stream.destroy(); } catch (ignore) { /* ignore */ }
      return;
    }
    stream.on('error', () => {});
    PacketPortal.#bridge(stream, (err, socket) => {
      if (err) {
        try { stream.destroy(); } catch (ignore) { /* ignore */ }
        return;
      }
      // Mark the request as arriving over a packet portal. It is delivered through
      // a loopback bridge socket, so its 127.0.0.1 source address, any session
      // cookie and any userNameHeader it carries are NOT trustworthy -- Auth
      // authenticates these requests with the s2s token only (see Auth.doAuth),
      // on top of the #gate check every portal request has already passed.
      socket.arkimePacketPortal = true;
      PacketPortal.#innerServer.emit('connection', socket);
    });
  }

  // --------------------------------------------------------------------------
  // Bridge an Http2Stream to a real net.Socket via a one-shot loopback pair,
  // then hand the other end to deliver(err, socket). Real sockets copy writes
  // synchronously, which the http client/server require and an Http2Stream does
  // not provide. deliver receives a still-connecting socket (net.connect), which
  // http core handles natively.
  static #bridge (h2stream, deliver) {
    let settled = false;
    let end;
    const finish = (err, sock) => {
      if (settled) { return; }
      settled = true;
      clearTimeout(timer);
      deliver(err, sock);
    };
    const bridge = net.createServer((pipeEnd) => {
      // Accept ONLY our own connector. The listener is on a random loopback port,
      // but a local process could still race to connect to it first; matching the
      // connector's local port ensures we bridge our own end, not an impostor's.
      if (!end || pipeEnd.remotePort !== end.localPort) { pipeEnd.destroy(); return; }
      clearTimeout(timer);
      try { bridge.close(); } catch (e) { /* ignore */ }
      // Tear the counterpart down only on ABNORMAL termination (error / premature
      // close), never on a clean finish. Destroying on a clean close would drop an
      // h2 stream's still-buffered outbound data whenever the peer reads slowly,
      // truncating large transfers (e.g. a pcap download to a slow client).
      // stream.finished() reports err only on abnormal end, so a normal completion
      // is left to flush and end gracefully via the pipes.
      pipeEnd.on('error', () => {});
      h2stream.on('error', () => {});
      streamLib.finished(pipeEnd, (err) => { if (err) { try { h2stream.destroy(); } catch (e) { /* ignore */ } } });
      streamLib.finished(h2stream, (err) => { if (err) { try { pipeEnd.destroy(); } catch (e) { /* ignore */ } } });
      pipeEnd.pipe(h2stream);
      h2stream.pipe(pipeEnd);
    });
    // Close the listener (and fail) if our own connection never lands, so a
    // failed request cannot leak a listening socket / fd.
    const timer = setTimeout(() => {
      try { bridge.close(); } catch (e) { /* ignore */ }
      finish(new Error('packetPortal bridge timed out'));
    }, 10000);
    bridge.on('error', (e) => finish(e));
    bridge.listen(0, '127.0.0.1', () => {
      end = net.connect(bridge.address().port, '127.0.0.1');
      end.on('error', (e) => { try { bridge.close(); } catch (x) { /* ignore */ } finish(e); });
      end.on('connect', () => finish(null, end));
    });
  }

  // --------------------------------------------------------------------------
  // DIALER: keep one outbound portal to target alive, reconnecting on drop.
  static #startDialer (target) {
    let backoff = 1000;
    let timer;
    // target may carry the node's portal password as user:pass
    const safeTarget = PacketPortal.#redact(target);

    const reconnect = (why) => {
      console.log(`packetPortal: ${safeTarget} ${why}; reconnecting in ${backoff}ms`);
      clearTimeout(timer);
      timer = setTimeout(connect, backoff);
      backoff = Math.min(backoff * 2, 60000);
    };

    const connect = () => {
      let url;
      try {
        url = new URL(PORTAL_PATH, target);
      } catch (e) {
        console.log(`packetPortal: invalid packetPortalConnect url '${safeTarget}'`);
        return;
      }

      const client = url.protocol === 'https:' ? https : http;
      const token = Auth.obj2auth({
        date: Date.now(),
        user: 'packetPortal',
        node: Config.nodeName(),
        path: PORTAL_PATH,
        method: 'GET'
      });

      const options = {
        method: 'GET',
        agent: false,
        headers: {
          Connection: 'Upgrade',
          Upgrade: UPGRADE_TOKEN,
          // In the clear so the acceptor can authenticate us before doing any
          // token crypto. Any user:pass in the url becomes Basic auth (node's
          // urlToHttpOptions), which is how the acceptor proves it really is us.
          'x-arkime-node': Config.nodeName(),
          'x-arkime-auth': token
        }
      };
      if (client === https) {
        options.rejectUnauthorized = !ArkimeConfig.insecure;
        const caTrustFile = Config.get('packetPortalCATrustFile');
        if (caTrustFile) { options.ca = ArkimeUtil.certificateFileToArray(caTrustFile); }
      }

      let settled = false;
      const req = client.request(url, options);

      req.on('upgrade', (res, socket, head) => {
        settled = true;
        backoff = 1000;
        console.log(`packetPortal: connected to ${safeTarget}`);
        socket.on('error', () => {});
        socket.setTimeout(0);
        if (head && head.length > 0) { socket.unshift(head); }
        // Node's h2 server drops a socket with alpnProtocol false/'http/1.1' as
        // unknownProtocol. Can't fix by alpn, shared mode is the main viewer port.
        if (socket.alpnProtocol === false || socket.alpnProtocol === 'http/1.1') {
          socket.alpnProtocol = undefined;
        }
        socket.once('close', () => reconnect('portal closed'));
        PacketPortal.#dialerH2Server.emit('connection', socket);
      });

      req.on('response', (res) => {
        // Server answered instead of upgrading -- treat as a failure.
        res.resume();
        if (!settled) { settled = true; reconnect(`unexpected status ${res.statusCode}`); }
      });

      req.on('error', (e) => {
        if (!settled) { settled = true; reconnect(`error ${e.message}`); }
      });

      req.end();
    };

    connect();
  }
}

module.exports = PacketPortal;
