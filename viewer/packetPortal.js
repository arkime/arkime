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
 *      middleware, and the existing x-arkime-auth S2S check all run unchanged.
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
  // acceptor side: nodes ever seen via a packet portal. Such a node is behind
  // NAT and CANNOT be dialed directly, so once known-reverse we never fall back
  // to a direct connection -- we use the portal or wait for it to come back.
  static #reverseNodes = new Set();
  // acceptor side: node name -> Set of resolve fns waiting for a (re)connect
  static #waiters = new Map();
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
    // serve requests that arrive over a portal.
    PacketPortal.#innerServer = http.createServer(app);
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

    // ACCEPTOR listener. A dedicated port is recommended when the main viewer
    // port is fronted by a proxy, uses header auth, or is IP locked down --
    // none of which suit dialers coming from behind NAT. The portal is still
    // protected by the serverSecret handshake (and TLS when key/cert are set).
    const portalPort = Config.get('packetPortalPort');
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
  // main viewer server (its cert hot-reload is unaffected).
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
  // ACCEPTOR: has this node ever connected via a packet portal? If so it is
  // unreachable directly and callers must not fall back to a direct connection.
  static isReverse (node) {
    if (Array.isArray(node)) { node = node[0]; }
    return PacketPortal.#reverseNodes.has(node);
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
    if (!PacketPortal.#reverseNodes.has(node)) { return Promise.resolve(null); }

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
    // Only the arkime-portal upgrade is ours. Anything else (h2c, websocket,
    // ...) has never been supported by the viewer; Node closed such upgrades by
    // default when there was no 'upgrade' listener, so preserve that exactly.
    // Normal clients (including HTTP/2 via ALPN) never take the upgrade path.
    if ((req.headers.upgrade ?? '').toLowerCase() !== UPGRADE_TOKEN) {
      socket.destroy();
      return;
    }

    if (!PacketPortal.#listenEnabled) { socket.destroy(); return; }

    let obj;
    try {
      obj = Auth.auth2obj(req.headers['x-arkime-auth']);
    } catch (e) {
      console.log('packetPortal: rejecting upgrade, bad auth -', e.message);
      socket.destroy();
      return;
    }

    const s2sError = Auth.validateS2SObj(obj, req);
    if (s2sError) {
      console.log('packetPortal: rejecting upgrade -', s2sError);
      socket.destroy();
      return;
    }

    const node = obj.node;
    if (!ArkimeUtil.isString(node) || !PacketPortal.#hostnameRE.test(node)) {
      console.log('packetPortal: rejecting upgrade, bad node name');
      socket.destroy();
      return;
    }

    // Optional allowlist: even a valid serverSecret token may only claim a node
    // name that is expected to use portals. Without this, any secret-holder could
    // claim (and thereby hijack or deny) an arbitrary node's traffic. When unset,
    // any node with a valid token is accepted (recommended: set an allowlist).
    const allowed = Config.getArray('packetPortalAllowedNodes', '');
    if (allowed.length > 0 && !allowed.includes(node)) {
      console.log(`packetPortal: rejecting upgrade, node '${node}' not in packetPortalAllowedNodes`);
      socket.destroy();
      return;
    }

    socket.on('error', () => {});
    socket.setTimeout(0);
    socket.write('HTTP/1.1 101 Switching Protocols\r\n' +
                 'Upgrade: ' + UPGRADE_TOKEN + '\r\n' +
                 'Connection: Upgrade\r\n\r\n');
    if (head && head.length > 0) { socket.unshift(head); }

    const session = http2.connect('http://arkime-portal.invalid', {
      createConnection: () => socket
    });
    session.on('error', (e) => console.log(`packetPortal: client session error for ${node} -`, e.message));
    session.on('close', () => {
      const entry = PacketPortal.#portals.get(node);
      if (entry?.session === session) {
        if (entry.timer) { clearInterval(entry.timer); }
        PacketPortal.#portals.delete(node);
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
    PacketPortal.#reverseNodes.add(node);
    console.log(`packetPortal: registered inbound portal for node ${node}`);

    // Wake anything waiting for this node's portal to (re)connect.
    const waiters = PacketPortal.#waiters.get(node);
    if (waiters) {
      PacketPortal.#waiters.delete(node);
      for (const waiter of waiters) { waiter(session); }
    }
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
      timeout: 30000
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
      // a loopback bridge socket, so its 127.0.0.1 source address and any
      // userNameHeader it carries are NOT trustworthy -- Auth forces these
      // requests to authenticate with the s2s token only (see Auth.doAuth).
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

    const reconnect = (why) => {
      console.log(`packetPortal: ${target} ${why}; reconnecting in ${backoff}ms`);
      clearTimeout(timer);
      timer = setTimeout(connect, backoff);
      backoff = Math.min(backoff * 2, 60000);
    };

    const connect = () => {
      let url;
      try {
        url = new URL(PORTAL_PATH, target);
      } catch (e) {
        console.log(`packetPortal: invalid packetPortalConnect url '${target}'`);
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
        console.log(`packetPortal: connected to ${target}`);
        socket.on('error', () => {});
        socket.setTimeout(0);
        if (head && head.length > 0) { socket.unshift(head); }
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
