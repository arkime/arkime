/* apiMcp.js  -- Viewer MCP tools
 *
 * Every tool runs an existing viewer api handler chain in process via
 * MCPServer.callApi, rather than reimplementing queries. That keeps the tools
 * thin and, more importantly, means the handler's own permission middleware
 * (User.checkPermissions), the per user forced search expression, and history
 * logging all still apply - an MCP caller can never see more than the same user
 * could see in the UI.
 *
 * Copyright Andy Wick
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const MCPServer = require('../common/mcpServer');
const ArkimeConfig = require('../common/arkimeConfig');
const User = require('../common/user');
const Auth = require('../common/auth');
const BuildQuery = require('./buildQuery');
const { MCPToolError } = MCPServer;

// The standard SessionsQuery parameters, see apiSessions.js SessionsQuery
const SESSION_QUERY_PROPS = {
  expression: {
    type: 'string',
    description: "Arkime search expression, eg 'ip.dst == 1.2.3.4 && port.dst == 443'. Call arkime_fields first to discover valid field names. Operators: == != < <= > >= , && || ! and parentheses. Strings support * wildcards and /regex/. IPs support CIDR. Use 'field == EXISTS!' to test presence. OR lists are field == [a,b], AND lists are field == ]a,b[."
  },
  date: {
    type: 'number',
    description: 'Search the last N hours. -1 means all data. Defaults to 1. Ignored when startTime/stopTime are given.'
  },
  startTime: { type: 'number', description: 'Start of the time window, seconds since epoch. Use with stopTime instead of date.' },
  stopTime: { type: 'number', description: 'End of the time window, seconds since epoch.' },
  view: { type: 'string', description: 'Name of a saved Arkime view to AND with the expression.' },
  bounding: {
    type: 'string',
    enum: ['first', 'last', 'both', 'either', 'database'],
    description: "Which session timestamp the time window applies to. Defaults to 'last'."
  }
};

function sessionQuerySchema (extra = {}, required = []) {
  return {
    type: 'object',
    properties: { ...SESSION_QUERY_PROPS, ...extra },
    required
  };
}

class MCPViewerAPIs {
  static #mw;
  static #tools;

  /**
   * @param {object} options.middleware viewer.js module local middleware we reuse
   *        (logAction, expToField, getSettingUserCache, sanitizeViewName)
   * @param {object} options.apis the api classes (SessionAPIs, StatsAPIs, ...)
   */
  static initialize (options) {
    MCPViewerAPIs.#mw = options.middleware;
    MCPViewerAPIs.#tools = MCPViewerAPIs.#buildTools(options.apis);
  }

  static get tools () { return MCPViewerAPIs.#tools; }

  // --------------------------------------------------------------------------
  /* Pull only the defined session query params out of the tool arguments, so a
   * model can't smuggle arbitrary query params into the underlying handler */
  static #sessionQuery (args, extraKeys = []) {
    const query = {};
    for (const key of [...Object.keys(SESSION_QUERY_PROPS), ...extraKeys]) {
      if (args[key] !== undefined) { query[key] = args[key]; }
    }
    query.date ??= 1;
    MCPViewerAPIs.#checkQueryDays(query);
    return query;
  }

  // --------------------------------------------------------------------------
  /**
   * Cap how much time an MCP query may span, mcpMaxQueryDays, default 7 days,
   * -1 for no limit. This is the width of the window, not how far back it
   * reaches.
   */
  static #checkQueryDays (query) {
    const maxDays = parseFloat(ArkimeConfig.get('mcpMaxQueryDays', 7));
    if (isNaN(maxDays) || maxDays < 0) { return; } // -1 means no limit

    const limit = `mcpMaxQueryDays is ${maxDays}`;

    // Resolve the window exactly how the query itself will, so string dates,
    // date=-1, segments=all and bad input can't slip past a hand rolled check
    const [startSec, stopSec] = BuildQuery.determineQueryTimes({ ...query });

    if (startSec === null || stopSec === null || isNaN(startSec) || isNaN(stopSec)) {
      throw new MCPToolError(`Searching all data is not allowed, ${limit}. Use date or startTime/stopTime.`);
    }

    const spanDays = (stopSec - startSec) / 86400;
    if (spanDays > maxDays) {
      throw new MCPToolError(`Time range of ${spanDays.toFixed(2)} days is too large, ${limit}`);
    }
  }

  // --------------------------------------------------------------------------
  static #buildTools (apis) {
    const { SessionAPIs, StatsAPIs, MiscAPIs, ConnectionAPIs, ViewAPIs, HuntAPIs, ShortcutAPIs, HistoryAPIs } = apis;
    const mw = MCPViewerAPIs.#mw;

    // Note: the write tools below deliberately omit checkCookieToken. That is a
    // CSRF defense for browser cookie sessions; MCP requests are authenticated
    // by bearer token or trusted proxy header and are never issued ambiently by
    // a browser. The permission middleware is kept in every case.
    return [
      // ---------------------------------------------------------------- read
      {
        name: 'arkime_fields',
        title: 'List Arkime fields',
        description: 'List every searchable Arkime field: its expression name (use in expression strings), dbField (use in fields/order/spi parameters), type and help text. Call this first to learn what can be queried.',
        annotations: { readOnlyHint: true },
        inputSchema: { type: 'object', properties: {} },
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          method: 'GET',
          url: '/api/fields',
          query: { array: 'true' },
          handlers: [MiscAPIs.getFields]
        })
      },
      {
        name: 'arkime_sessions',
        title: 'Search sessions',
        description: 'Search network sessions (connections) and return matching records. This is the primary search tool.',
        annotations: { readOnlyHint: true },
        inputSchema: sessionQuerySchema({
          length: { type: 'number', description: 'How many sessions to return, default 100.' },
          start: { type: 'number', description: 'Offset for paging, default 0.' },
          fields: { type: 'string', description: 'Comma separated dbField names to return. Defaults to a common set.' },
          order: { type: 'string', description: "Comma separated dbField names to sort by, each optionally :asc or :desc, eg 'firstPacket:desc'." }
        }),
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          url: '/api/sessions',
          query: MCPViewerAPIs.#sessionQuery(args, ['length', 'start', 'fields', 'order']),
          handlers: [mw.logAction('sessions'), SessionAPIs.getSessions]
        })
      },
      {
        name: 'arkime_session_detail',
        title: 'Get one session',
        description: 'Fetch the full metadata (SPI data) for a single session by its Arkime session id.',
        annotations: { readOnlyHint: true },
        inputSchema: {
          type: 'object',
          properties: { id: { type: 'string', description: 'The Arkime session id, as returned by arkime_sessions.' } },
          required: ['id']
        },
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          method: 'GET',
          url: `/api/session/${args.id}`,
          params: { id: args.id },
          handlers: [mw.logAction(), SessionAPIs.getSessionById]
        })
      },
      {
        name: 'arkime_spigraph',
        title: 'Top values over time',
        description: 'Aggregate one field across matching sessions: returns the top values with session/packet/byte counts plus a time series for each. Use to answer "what are the top N x" questions.',
        annotations: { readOnlyHint: true },
        inputSchema: sessionQuerySchema({
          exp: { type: 'string', description: "Field expression name to aggregate on, eg 'ip.dst' or 'http.host'." },
          size: { type: 'number', description: 'How many distinct values to return, default 20.' }
        }, ['exp']),
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          url: '/api/spigraph',
          query: MCPViewerAPIs.#sessionQuery(args, ['exp', 'size']),
          handlers: [mw.logAction('spigraph'), mw.expToField, SessionAPIs.getSPIGraph]
        })
      },
      {
        name: 'arkime_spiview',
        title: 'Field value summary',
        description: 'Return the top values for several fields at once for the matching sessions. Cheaper than many arkime_spigraph calls when you just want an overview.',
        annotations: { readOnlyHint: true },
        inputSchema: sessionQuerySchema({
          spi: { type: 'string', description: "Comma separated dbField names, each optionally :count, eg 'destination.ip:10,http.host:5'." }
        }, ['spi']),
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          url: '/api/spiview',
          query: MCPViewerAPIs.#sessionQuery(args, ['spi']),
          handlers: [mw.logAction('spiview'), SessionAPIs.getSPIView]
        })
      },
      {
        name: 'arkime_unique',
        title: 'Unique field values',
        description: 'Return the distinct values of a single field across matching sessions, optionally with counts. Returns plain text, one value per line.',
        annotations: { readOnlyHint: true },
        inputSchema: sessionQuerySchema({
          exp: { type: 'string', description: "Field expression name, eg 'dns.host'." },
          counts: { type: 'number', description: 'Set to 1 to include a count next to each value.' }
        }, ['exp']),
        handler: async (args, req) => {
          const result = await MCPServer.callApi(req, {
            url: '/api/unique',
            query: MCPViewerAPIs.#sessionQuery(args, ['exp', 'counts']),
            handlers: [mw.logAction('unique'), mw.expToField, SessionAPIs.getUnique]
          });
          if (result.status >= 400) { throw new MCPToolError(result.body?.text ?? 'unique failed'); }
          return { values: result.text.split('\n').filter(l => l !== '') };
        }
      },
      {
        name: 'arkime_multiunique',
        title: 'Unique field combinations',
        description: 'Return the distinct combinations of several fields across matching sessions, with counts. Returns plain text, one combination per line.',
        annotations: { readOnlyHint: true },
        inputSchema: sessionQuerySchema({
          exp: { type: 'string', description: "Comma separated field expression names, eg 'source.ip,destination.ip'." },
          counts: { type: 'number', description: 'Set to 1 to include counts.' }
        }, ['exp']),
        handler: async (args, req) => {
          const result = await MCPServer.callApi(req, {
            url: '/api/multiunique',
            query: MCPViewerAPIs.#sessionQuery(args, ['exp', 'counts']),
            handlers: [mw.logAction('multiunique'), SessionAPIs.getMultiunique]
          });
          if (result.status >= 400) { throw new MCPToolError(result.body?.text ?? 'multiunique failed'); }
          return { values: result.text.split('\n').filter(l => l !== '') };
        }
      },
      {
        name: 'arkime_connections',
        title: 'Connection graph',
        description: 'Build a node/link graph of who talked to whom for the matching sessions. Returns nodes and links with session, packet and byte totals.',
        annotations: { readOnlyHint: true },
        inputSchema: sessionQuerySchema({
          srcField: { type: 'string', description: "Field expression for the source node, default 'ip.src'." },
          dstField: { type: 'string', description: "Field expression for the destination node, default 'ip.dst:port'." }
        }),
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          url: '/api/connections',
          query: MCPViewerAPIs.#sessionQuery(args, ['srcField', 'dstField']),
          handlers: [mw.logAction('connections'), ConnectionAPIs.getConnections]
        })
      },
      {
        name: 'arkime_buildquery',
        title: 'Validate an expression',
        description: 'Translate an Arkime search expression into the OpenSearch/Elasticsearch query it would run, without running it. Use to check an expression is valid, and to see the forced expression applied to the user.',
        annotations: { readOnlyHint: true },
        inputSchema: sessionQuerySchema(),
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          url: '/api/buildquery',
          query: MCPViewerAPIs.#sessionQuery(args),
          handlers: [mw.logAction('query'), SessionAPIs.getQuery]
        })
      },
      {
        name: 'arkime_views',
        title: 'List saved views',
        description: 'List the saved search views available to the current user. A view name can be passed as the view parameter of the search tools.',
        annotations: { readOnlyHint: true },
        inputSchema: { type: 'object', properties: {} },
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          method: 'GET',
          url: '/api/views',
          handlers: [mw.getSettingUserCache, ViewAPIs.apiGetViews]
        })
      },
      {
        name: 'arkime_shortcuts',
        title: 'List shortcuts',
        description: 'List saved value shortcuts (named lists of IPs, strings or numbers) that can be referenced by name inside a search expression.',
        annotations: { readOnlyHint: true },
        inputSchema: {
          type: 'object',
          properties: {
            searchTerm: { type: 'string', description: 'Filter shortcuts by name.' },
            length: { type: 'number', description: 'How many to return, default 100.' }
          }
        },
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          method: 'GET',
          url: '/api/shortcuts',
          query: { length: args.length ?? 100, ...(args.searchTerm ? { searchTerm: args.searchTerm } : {}) },
          handlers: [mw.getSettingUserCache, ShortcutAPIs.getShortcuts]
        })
      },
      {
        name: 'arkime_hunts',
        title: 'List packet hunts',
        description: 'List packet search (hunt) jobs and their status. Requires the packetSearch permission.',
        annotations: { readOnlyHint: true },
        inputSchema: {
          type: 'object',
          properties: {
            searchTerm: { type: 'string', description: 'Filter hunts by name.' },
            length: { type: 'number', description: 'How many to return, default 100.' },
            history: { type: 'boolean', description: 'Set true to list finished hunts instead of running ones.' }
          }
        },
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          method: 'GET',
          url: '/api/hunts',
          query: {
            length: args.length ?? 100,
            ...(args.searchTerm ? { searchTerm: args.searchTerm } : {}),
            ...(args.history !== undefined ? { history: args.history } : {})
          },
          handlers: [User.checkPermissions(['packetSearch']), HuntAPIs.getHunts]
        })
      },
      {
        name: 'arkime_stats',
        title: 'Capture node stats',
        description: 'Statistics for each Arkime capture node: packets/sec, dropped packets, disk usage and so on. Denied if the user has hideStats set.',
        annotations: { readOnlyHint: true },
        inputSchema: {
          type: 'object',
          properties: {
            filter: { type: 'string', description: 'Filter by node name.' },
            length: { type: 'number', description: 'How many nodes to return, default 500.' }
          }
        },
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          method: 'GET',
          url: '/api/stats',
          query: { length: args.length ?? 500, ...(args.filter ? { filter: args.filter } : {}) },
          handlers: [User.checkPermissions(['hideStats']), StatsAPIs.getStats]
        })
      },
      {
        name: 'arkime_esindices',
        title: 'List database indices',
        description: 'List the OpenSearch/Elasticsearch indices backing Arkime with their document counts and sizes. Denied if the user has hideStats set.',
        annotations: { readOnlyHint: true },
        inputSchema: { type: 'object', properties: { filter: { type: 'string', description: 'Filter by index name.' } } },
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          method: 'GET',
          url: '/api/esindices',
          query: { ...(args.filter ? { filter: args.filter } : {}) },
          handlers: [User.checkPermissions(['hideStats']), StatsAPIs.getESIndices]
        })
      },
      {
        name: 'arkime_files',
        title: 'List PCAP files',
        description: 'List the PCAP files Arkime knows about, with node, size and time range. Metadata only, never file contents. Denied if the user has hideFiles set.',
        annotations: { readOnlyHint: true },
        inputSchema: {
          type: 'object',
          properties: {
            length: { type: 'number', description: 'How many files to return, default 100.' },
            start: { type: 'number', description: 'Offset for paging.' }
          }
        },
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          method: 'GET',
          url: '/api/files',
          query: { length: args.length ?? 100, start: args.start ?? 0 },
          handlers: [mw.logAction('files'), User.checkPermissions(['hideFiles']), MiscAPIs.getFiles]
        })
      },
      {
        name: 'arkime_histories',
        title: 'Query history',
        description: 'List the history of queries run against Arkime, including who ran them and when.',
        annotations: { readOnlyHint: true },
        inputSchema: {
          type: 'object',
          properties: {
            length: { type: 'number', description: 'How many entries to return, default 100.' },
            searchTerm: { type: 'string', description: 'Filter entries.' }
          }
        },
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          method: 'GET',
          url: '/api/histories',
          query: { length: args.length ?? 100, ...(args.searchTerm ? { searchTerm: args.searchTerm } : {}) },
          handlers: [HistoryAPIs.getHistories]
        })
      },

      // --------------------------------------------------------------- write
      {
        name: 'arkime_add_tags',
        title: 'Tag sessions',
        description: 'Add one or more tags to sessions. Tags only: this does NOT modify packet data, does NOT delete or scrub sessions, and does NOT change any other session field.',
        annotations: { readOnlyHint: false, destructiveHint: false },
        inputSchema: sessionQuerySchema({
          tags: { type: 'string', description: 'Comma separated tags to add.' },
          ids: { type: 'string', description: 'Optional comma separated session ids. When omitted, every session matching the expression is tagged, so always set a narrow expression.' }
        }, ['tags']),
        handler: async (args, req) => {
          // addTags reads tags/ids from the body but the expression from the
          // query, so it needs both
          const q = MCPViewerAPIs.#sessionQuery(args, ['tags', 'ids']);
          return MCPServer.callApiOrThrow(req, {
            url: '/api/sessions/addtags',
            query: q,
            body: q,
            handlers: [mw.checkHeaderToken, mw.logAction('addTags'), SessionAPIs.addTags]
          });
        }
      },
      {
        name: 'arkime_remove_tags',
        title: 'Untag sessions',
        description: 'Remove one or more tags from sessions. Tags only: this does NOT modify packet data, does NOT delete or scrub sessions, and does NOT change any other session field.',
        annotations: { readOnlyHint: false, destructiveHint: false },
        inputSchema: sessionQuerySchema({
          tags: { type: 'string', description: 'Comma separated tags to remove.' },
          ids: { type: 'string', description: 'Optional comma separated session ids. When omitted, every session matching the expression is untagged.' }
        }, ['tags']),
        handler: async (args, req) => {
          const q = MCPViewerAPIs.#sessionQuery(args, ['tags', 'ids']);
          return MCPServer.callApiOrThrow(req, {
            url: '/api/sessions/removetags',
            query: q,
            body: q,
            handlers: [mw.checkHeaderToken, mw.logAction('removeTags'), User.checkPermissions(['removeEnabled']), SessionAPIs.removeTags]
          });
        }
      },
      {
        name: 'arkime_create_view',
        title: 'Save a view',
        description: 'Save a named search expression as a view for the current user. Does NOT run the search, does NOT share the view beyond the user, and does NOT overwrite an existing view.',
        annotations: { readOnlyHint: false, destructiveHint: false },
        inputSchema: {
          type: 'object',
          properties: {
            name: { type: 'string', description: 'Name for the view.' },
            expression: { type: 'string', description: 'The Arkime search expression to save.' }
          },
          required: ['name', 'expression']
        },
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          url: '/api/view',
          body: { name: args.name, expression: args.expression },
          handlers: [mw.logAction(), Auth.getSettingUserDb, mw.sanitizeViewName, ViewAPIs.apiCreateView]
        })
      },
      {
        name: 'arkime_create_hunt',
        title: 'Create a packet hunt',
        description: 'Queue a packet search (hunt) job that scans PCAP payloads for a string or regex. Requires the packetSearch permission. Does NOT return raw packet bytes and does NOT export PCAP; use arkime_hunts to check progress.',
        annotations: { readOnlyHint: false, destructiveHint: false },
        inputSchema: {
          type: 'object',
          properties: {
            expression: SESSION_QUERY_PROPS.expression,
            startTime: { type: 'number', description: 'Start of the hunt window, seconds since epoch. Required.' },
            stopTime: { type: 'number', description: 'End of the hunt window, seconds since epoch. Required.' },
            view: SESSION_QUERY_PROPS.view,
            name: { type: 'string', description: 'Name for the hunt job.' },
            search: { type: 'string', description: 'The string or regex to search packet payloads for.' },
            searchType: { type: 'string', enum: ['ascii', 'asciicase', 'hex', 'regex', 'hexregex'], description: 'How to interpret search.' },
            src: { type: 'boolean', description: 'Search packets sent by the source. Defaults true.' },
            dst: { type: 'boolean', description: 'Search packets sent by the destination. Defaults true.' },
            size: { type: 'number', description: 'Max packets to examine per session, default 50.' }
          },
          required: ['name', 'search', 'searchType', 'startTime', 'stopTime']
        },
        handler: async (args, req) => {
          // Hunts scan an explicit window, there is no relative 'date' form
          if (args.startTime === undefined || args.stopTime === undefined) {
            throw new MCPToolError('startTime and stopTime are required to create a hunt');
          }

          const query = MCPViewerAPIs.#sessionQuery(args);
          delete query.date;

          // createHunt requires the caller to declare up front how many
          // sessions the hunt will scan, so count them first the way the UI does
          const count = await MCPServer.callApiOrThrow(req, {
            url: '/api/sessions',
            query: { ...query, length: 0 },
            handlers: [SessionAPIs.getSessions]
          });

          const totalSessions = count?.recordsFiltered ?? 0;
          if (!totalSessions) {
            throw new MCPToolError('No sessions match that expression and time range, so there is nothing to hunt');
          }

          return MCPServer.callApiOrThrow(req, {
            url: '/api/hunt',
            query,
            body: {
              name: args.name,
              search: args.search,
              searchType: args.searchType,
              type: 'raw',
              size: args.size ?? 50,
              src: args.src ?? true,
              dst: args.dst ?? true,
              totalSessions,
              query: {
                expression: args.expression,
                startTime: args.startTime,
                stopTime: args.stopTime
              }
            },
            handlers: [mw.logAction('hunt'), User.checkPermissions(['packetSearch']), HuntAPIs.createHunt]
          });
        }
      }
    ];
  }
}

module.exports = MCPViewerAPIs;
