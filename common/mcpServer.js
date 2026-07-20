/* mcpServer.js  -- Shared MCP (Model Context Protocol) server
 *
 * Implements the streamable HTTP transport of the MCP spec over JSON-RPC 2.0,
 * stateless: every POST /mcp is self contained, there are no sessions and no
 * SSE stream. That is all an MCP gateway needs and it keeps us out of the
 * session bookkeeping the official SDK requires.
 *
 * Copyright Andy Wick
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const express = require('express');
const { EventEmitter } = require('events');
const ArkimeConfig = require('./arkimeConfig');
const ArkimeUtil = require('./arkimeUtil');

// Newest first, we answer with the client's version when we know it
const PROTOCOL_VERSIONS = ['2025-06-18', '2025-03-26', '2024-11-05'];

// JSON-RPC 2.0 error codes
const PARSE_ERROR = -32700;
const INVALID_REQUEST = -32600;
const METHOD_NOT_FOUND = -32601;
const INVALID_PARAMS = -32602;
const INTERNAL_ERROR = -32603;

/******************************************************************************/
/**
 * Thrown by a tool handler to fail the tool call without failing the http
 * request. The MCP spec wants tool errors reported as a normal 200 result with
 * isError set, so the model can see and react to them.
 */
class MCPToolError extends Error {
  constructor (text) {
    super(text);
    this.name = 'MCPToolError';
  }
}

/******************************************************************************/
class MCPServer {
  /**
   * Build an express router implementing POST /mcp
   *
   * @param {string} options.serviceName Used as the WWW-Authenticate realm and serverInfo.name
   * @param {string} options.version Reported as serverInfo.version
   * @param {object[]} options.tools The tools to expose, see #tool shape below
   * @param {function} [options.authenticate] async (req) => user. Defaults to the
   *        mcpAuthMode driven authenticator, built lazily on first request.
   * @param {function} [options.enabled] () => bool, checked per request
   * @param {string} [options.serviceRole] The role needed to use this service at
   *        all (eg arkimeUser). The caller must hold this AND the MCP role.
   */
  static router (options) {
    const router = express.Router();
    const tools = new Map(options.tools.map(t => [t.name, t]));

    // Routes are registered before the config is loaded (viewer.js builds the
    // whole app at require time and only awaits Config.initialize() later), so
    // anything config driven has to be resolved per request, not here.
    let authenticate = options.authenticate;
    const enabled = options.enabled ?? (() => true);

    router.use(ArkimeUtil.jsonParser);

    // jsonParser rejects malformed/prototype polluting json, turn that into a
    // JSON-RPC parse error instead of the express default html error page
    router.use((err, req, res, next) => {
      if (err) { return MCPServer.#error(res, 200, null, PARSE_ERROR, 'Parse error'); }
      return next();
    });

    // We only implement the POST half of streamable http. Be explicit rather
    // than letting these fall through to the app's catch all vue route.
    router.all('/', (req, res, next) => {
      if (req.method === 'POST') { return next(); }
      res.setHeader('Allow', 'POST');
      return MCPServer.#error(res, 405, null, INVALID_REQUEST, 'Only POST is supported');
    });

    router.post('/', async (req, res) => {
      if (!enabled()) {
        return MCPServer.#error(res, 404, null, INVALID_REQUEST, 'MCP is not enabled');
      }

      if (!MCPServer.#checkIps(req)) {
        return MCPServer.#error(res, 403, null, INVALID_REQUEST, 'Forbidden');
      }

      authenticate ??= MCPServer.authenticator();

      // Auth failure MUST be 401 + WWW-Authenticate and MUST NOT redirect, MCP
      // clients don't follow redirects. This is why /mcp does its own auth
      // instead of using Auth.doAuth, which answers 403.
      let user;
      try {
        user = await authenticate(req);
      } catch (err) {
        if (ArkimeConfig.debug > 0) { console.log('MCP: auth failed', err); }
      }

      if (!user) {
        return MCPServer.#unauthorized(res, options.serviceName);
      }

      // Two gates: the service's own role, plus mcpUser
      const missing = [options.serviceRole, 'mcpUser'].filter(r => r && !user.hasRole(r));
      if (missing.length) {
        return MCPServer.#error(res, 403, null, INVALID_REQUEST, `Requires ${missing.join(' and ')} role`);
      }

      req.user = user;

      const body = req.body;

      // Batching was removed from streamable http, and we never need it
      if (Array.isArray(body)) {
        return MCPServer.#error(res, 200, null, INVALID_REQUEST, 'Batch requests are not supported');
      }

      // jsonParser already rejected anything prototype polluting
      if (body?.jsonrpc !== '2.0' || !ArkimeUtil.isString(body?.method)) {
        return MCPServer.#error(res, 200, body?.id, INVALID_REQUEST, 'Invalid request');
      }

      // No id means a notification: acknowledge, never answer
      if (body.id === undefined || body.id === null) {
        res.status(202);
        return res.end();
      }

      try {
        const result = await MCPServer.#dispatch(req, body, tools, options);
        return MCPServer.#result(res, body.id, result);
      } catch (err) {
        if (err instanceof MCPRPCError) {
          return MCPServer.#error(res, 200, body.id, err.code, err.message);
        }
        console.log('MCP ERROR -', ArkimeUtil.sanitizeStr(body.method), err);
        return MCPServer.#error(res, 200, body.id, INTERNAL_ERROR, 'Internal error');
      }
    });

    return router;
  }

  // ----------------------------------------------------------------------------
  static async #dispatch (req, body, tools, options) {
    const params = body.params ?? {};

    switch (body.method) {
    case 'initialize': {
      const wanted = params.protocolVersion;
      return {
        protocolVersion: PROTOCOL_VERSIONS.includes(wanted) ? wanted : PROTOCOL_VERSIONS[0],
        capabilities: { tools: { listChanged: false } },
        serverInfo: { name: options.serviceName, version: options.version }
      };
    }
    case 'ping':
      return {};
    case 'tools/list':
      return {
        tools: [...tools.values()].map(t => ({
          name: t.name,
          title: t.title,
          description: t.description,
          inputSchema: t.inputSchema ?? { type: 'object', properties: {} },
          annotations: t.annotations
        }))
      };
    case 'tools/call':
      return await MCPServer.#callTool(req, params, tools);
    case 'resources/list':
      return { resources: [] };
    case 'resources/templates/list':
      return { resourceTemplates: [] };
    case 'prompts/list':
      return { prompts: [] };
    default:
      throw new MCPRPCError(METHOD_NOT_FOUND, `Unknown method ${ArkimeUtil.safeStr(body.method)}`);
    }
  }

  // ----------------------------------------------------------------------------
  static async #callTool (req, params, tools) {
    const tool = tools.get(params.name);
    if (tool === undefined) {
      throw new MCPRPCError(INVALID_PARAMS, `Unknown tool ${ArkimeUtil.safeStr(params.name)}`);
    }

    const args = params.arguments ?? {};
    if (typeof args !== 'object' || Array.isArray(args)) {
      throw new MCPRPCError(INVALID_PARAMS, 'arguments must be an object');
    }

    try {
      const data = await tool.handler(args, req);
      return {
        content: [{ type: 'text', text: JSON.stringify(data) }],
        structuredContent: data,
        isError: false
      };
    } catch (err) {
      // A failing tool is a successful http request with isError set, so the
      // model can see what went wrong instead of the client seeing a 500
      const text = err instanceof MCPToolError ? err.message : 'Tool call failed';
      if (!(err instanceof MCPToolError)) {
        console.log('MCP ERROR - tool', ArkimeUtil.sanitizeStr(tool.name), err);
      }
      return { content: [{ type: 'text', text }], isError: true };
    }
  }

  // ----------------------------------------------------------------------------
  static #result (res, id, result) {
    res.status(200);
    res.setHeader('Content-Type', 'application/json');
    return res.send({ jsonrpc: '2.0', id, result });
  }

  // ----------------------------------------------------------------------------
  static #error (res, httpStatus, id, code, message) {
    res.status(httpStatus);
    res.setHeader('Content-Type', 'application/json');
    return res.send({ jsonrpc: '2.0', id: id ?? null, error: { code, message } });
  }

  // ----------------------------------------------------------------------------
  static #unauthorized (res, serviceName) {
    res.setHeader('WWW-Authenticate', `Bearer realm="${serviceName}"`);
    return MCPServer.#error(res, 401, null, INVALID_REQUEST, 'Unauthorized');
  }

  // ----------------------------------------------------------------------------
  /**
   * Express error middleware turning a json body parse failure into a JSON-RPC
   * parse error. Apps that parse json app wide (viewer does) reject a malformed
   * body long before the mcp router sees it, so this has to be installed right
   * after that parser, not inside the router.
   */
  static parseErrorMiddleware (mountPath = '/mcp') {
    return (err, req, res, next) => {
      if (err && req.url.split('?')[0].replace(/\/$/, '').endsWith(mountPath)) {
        return MCPServer.#error(res, 200, null, PARSE_ERROR, 'Parse error');
      }
      return next(err);
    };
  }

  // ----------------------------------------------------------------------------
  /**
   * Build the authenticate function from the mcpAuthMode config: an ordered
   * list of verifiers, the first one to produce a user wins.
   *
   *   header - trust a username header set by a proxy that already verified the
   *            caller (our Apache/mod_auth_openidc deployment does the gateway
   *            JWT verification and passes the short_id through)
   *   jwt    - verify the Bearer JWT ourselves against a remote JWKS
   *
   * Kept as an array so another verifier can be added later without touching
   * the router.
   */
  static authenticator () {
    const Auth = require('./auth');
    const verifiers = [];

    for (const mode of ArkimeConfig.getArray('mcpAuthMode', 'header')) {
      switch (mode) {
      case 'header':
        verifiers.push(req => Auth.headerAuth(req));
        break;
      case 'jwt':
        verifiers.push(req => Auth.jwtAuth(req));
        break;
      case 'regressionTests':
        // Takes the user from a query param, so it must never be reachable
        // outside the test harness
        if (!Auth.regressionTests) {
          console.log('ERROR - mcpAuthMode=regressionTests requires --regressionTests');
          process.exit(1);
        }
        verifiers.push(req => Auth.regressionTestsAuth(req));
        break;
      default:
        console.log('ERROR - unknown mcpAuthMode', ArkimeUtil.sanitizeStr(mode), '- must be one of: header, jwt');
        process.exit(1);
      }
    }

    if (verifiers.length === 0) {
      console.log('ERROR - mcpAuthMode is empty, /mcp would reject everything');
      process.exit(1);
    }

    return async (req) => {
      for (const verifier of verifiers) {
        try {
          const user = await verifier(req);
          if (user) { return user; }
        } catch (err) {
          // Try the next verifier, a 401 is only sent once they all decline
          if (ArkimeConfig.debug > 0) { console.log('MCP: verifier declined -', err.message); }
        }
      }
      return undefined;
    };
  }

  // ----------------------------------------------------------------------------
  static #ips;

  /**
   * Optional source ip allow list, mcpAllowedIps. Empty means allow everything,
   * the gateway presents no client cert so ip is the only network level check
   * available to us.
   */
  static #checkIps (req) {
    if (MCPServer.#ips === undefined) {
      const iptrie = require('arkime-iptrie');
      const list = ArkimeConfig.getArray('mcpAllowedIps', '');
      MCPServer.#ips = null;
      if (list.length > 0 && list[0] !== '') {
        MCPServer.#ips = new iptrie.IPTrie();
        for (const cidr of list) {
          const parts = cidr.split('/');
          if (parts[0].includes(':')) {
            MCPServer.#ips.add(parts[0], +(parts[1] ?? 128), 1);
          } else {
            MCPServer.#ips.add(`::ffff:${parts[0]}`, 96 + +(parts[1] ?? 32), 1);
          }
        }
      }
    }

    if (MCPServer.#ips === null) { return true; }

    const ip = req.ip.includes(':') ? req.ip : `::ffff:${req.ip}`;
    return !!MCPServer.#ips.find(ip);
  }

  // ----------------------------------------------------------------------------
  /**
   * Run an existing express api handler chain in process and return its
   * response, instead of making an http request back to ourselves.
   *
   * Passing the real authenticated req.user through means the handler's own
   * permission middleware (User.checkPermissions, User.checkRole) and the per
   * user forced search expression all still apply, so an MCP caller can never
   * see more than they could in the UI.
   *
   * @param {object} req The authenticated MCP request, used for user/headers
   * @param {function[]} options.handlers The middleware + handler chain to run
   * @param {object} [options.query] req.query for the synthetic request
   * @param {object} [options.params] req.params for the synthetic request
   * @param {object} [options.body] req.body for the synthetic request
   * @param {string} [options.method=POST] req.method for the synthetic request
   * @param {string} [options.url=/] req.url for the synthetic request
   * @param {number} [options.timeout=0] ms before giving up, 0 means no timeout
   * @returns {Promise<object>} { status, body, text, timedOut }
   */
  static callApi (req, options) {
    return new Promise((resolve, reject) => {
      const url = options.url ?? '/';

      // Inherit from the real request so handlers still see headers, ip, etc,
      // but define our overrides as own data properties. Plain assignment would
      // hit express's getter-only accessors (path, query, ...) and throw.
      const own = (value) => ({ value, writable: true, enumerable: true, configurable: true });
      const areq = Object.create(req, {
        method: own(options.method ?? 'POST'),
        url: own(url),
        originalUrl: own(url),
        path: own(url),
        _parsedUrl: own({ pathname: url }),
        query: own(options.query ?? {}),
        params: own(options.params ?? {}),
        body: own(options.body ?? {}),
        user: own(req.user),
        settingUser: own(req.settingUser ?? req.user)
      });

      let finished = false;
      let timer;

      const chunks = [];
      let resStatus = 200;

      function done (timedOut) {
        if (finished) { return; }
        finished = true;
        clearTimeout(timer);

        // Real express handlers hang audit logging off the finish event
        // (viewer's logAction does), so this must actually fire
        try {
          ares.emit('finish');
        } catch (e) {
          console.log('MCP ERROR - finish listener', e);
        }

        const text = chunks.join('');
        let body;
        try {
          body = text.length ? JSON.parse(text) : undefined;
        } catch (err) {
          body = undefined;
        }
        return resolve({ status: resStatus, body, text, timedOut: !!timedOut });
      }

      if (options.timeout) {
        timer = setTimeout(() => done(true), options.timeout);
      }

      const ares = new EventEmitter();
      Object.assign(ares, {
        locals: {},
        headersSent: false,
        status: (s) => { resStatus = s; return ares; },
        sendStatus: (s) => { resStatus = s; return done(); },
        setHeader: () => ares,
        header: () => ares,
        set: () => ares,
        type: () => ares,
        vary: () => ares,
        cookie: () => ares,
        write: (chunk) => { if (chunk !== undefined) { chunks.push(chunk.toString()); } return true; },
        json: (obj) => { chunks.push(JSON.stringify(obj)); return done(); },
        send: (obj) => {
          if (obj !== undefined) {
            chunks.push(typeof obj === 'string' || Buffer.isBuffer(obj) ? obj.toString() : JSON.stringify(obj));
          }
          return done();
        },
        end: (chunk) => { if (chunk !== undefined) { chunks.push(chunk.toString()); } return done(); }
        // NB: no on/once/removeListener overrides here, the real EventEmitter
        // ones must survive so 'finish' listeners (audit logging) actually run
      });
      ares.serverError = ArkimeUtil.serverError;

      // Walk the handler chain the way express would
      const handlers = options.handlers;
      let i = 0;
      function next (err) {
        if (err) { return reject(err); }
        if (finished) { return; }
        if (i >= handlers.length) { return done(); }
        const handler = handlers[i++];
        try {
          const ret = handler(areq, ares, next);
          if (ret?.catch) { ret.catch(reject); }
        } catch (e) {
          return reject(e);
        }
      }
      next();
    });
  }

  // ----------------------------------------------------------------------------
  /**
   * callApi, but fail the tool call when the api answered with an error. Most
   * tools want this, the ones that need the raw response use callApi directly.
   */
  static async callApiOrThrow (req, options) {
    let result;
    try {
      result = await MCPServer.callApi(req, options);
    } catch (err) {
      throw new MCPToolError(`Request failed: ${err.message}`);
    }

    if (result.status >= 400 || result.body?.success === false) {
      throw new MCPToolError(result.body?.text ?? `Request failed with status ${result.status}`);
    }

    // Session queries answer 200 with an `error` string when the search
    // expression won't parse. Left alone that reads as "no matching traffic",
    // which is a far worse answer than "your query was malformed".
    if (ArkimeUtil.isString(result.body?.error)) {
      throw new MCPToolError(result.body.error);
    }

    return result.body;
  }
}

/******************************************************************************/
/* Internal: a JSON-RPC level failure, as opposed to a tool level one */
class MCPRPCError extends Error {
  constructor (code, message) {
    super(message);
    this.code = code;
  }
}

module.exports = MCPServer;
module.exports.MCPToolError = MCPToolError;
module.exports.PROTOCOL_VERSIONS = PROTOCOL_VERSIONS;
