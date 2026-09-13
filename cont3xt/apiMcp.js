/* apiMcp.js  -- Cont3xt MCP tools
 *
 * Like the viewer tools, each tool runs an existing cont3xt api handler in
 * process through MCPServer.callApi, so per user integration keys, disabled
 * integrations and viewRoles gating all keep working untouched.
 *
 * Copyright Andy Wick
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const MCPServer = require('../common/mcpServer');
const ArkimeConfig = require('../common/arkimeConfig');
const ArkimeUtil = require('../common/arkimeUtil');
const { MCPToolError } = MCPServer;

class MCPCont3xtAPIs {
  static #tools;

  /**
   * @param {object} options.apis { Integration, View, Overview, LinkGroup, Audit }
   */
  static initialize (options) {
    MCPCont3xtAPIs.#tools = MCPCont3xtAPIs.#buildTools(options.apis);
  }

  static get tools () { return MCPCont3xtAPIs.#tools; }

  // --------------------------------------------------------------------------
  /**
   * Run the streaming search handler and fold its chunks into one result.
   *
   * apiSearch writes a newline delimited json array as each integration
   * finishes and only ends after the last one, so we buffer to completion. It
   * also has no timeout of its own - a hung integration would never call
   * finishWrite - so we always impose one and report what did arrive.
   */
  static async #runSearch (req, options) {
    const timeout = +ArkimeConfig.get('mcpCont3xtTimeout', 60000);
    const result = await MCPServer.callApi(req, { ...options, timeout });

    let chunks;
    try {
      chunks = JSON.parse(result.text);
    } catch (err) {
      // A validation failure is sent as a bare object with no array wrapper,
      // and a timeout leaves us with a truncated array
      if (result.timedOut) {
        throw new MCPToolError(`Search timed out after ${timeout}ms with no complete result`);
      }
      throw new MCPToolError('Search returned an unparseable response');
    }

    if (!Array.isArray(chunks)) {
      throw new MCPToolError(chunks?.text ?? 'Search failed');
    }

    const results = [];
    const failed = [];
    let indicators;

    for (const chunk of chunks) {
      switch (chunk?.purpose) {
      case 'init':
        indicators = chunk.indicators;
        break;
      case 'data':
        results.push({ integration: chunk.name, indicator: chunk.indicator, data: chunk.data });
        break;
      case 'fail':
        failed.push({ integration: chunk.name, indicator: chunk.indicator });
        break;
      case 'error':
        throw new MCPToolError(chunk.text ?? 'Search failed');
      default:
        break;
      }
    }

    return {
      indicators,
      results,
      failed,
      partial: result.timedOut || undefined
    };
  }

  // --------------------------------------------------------------------------
  /* Search page link, see Cont3xt.vue; undefined when btoa can't encode the query */
  static #searchUiQuery (query, options = {}) {
    if (/[^\u0000-\u00ff]/.test(query)) { return undefined; }
    return ['', {
      b: Buffer.from(query, 'latin1').toString('base64'),
      submit: options.submit ? 'y' : undefined,
      view: options.view,
      skipChildren: options.skipChildren ? 'true' : undefined
    }];
  }

  // --------------------------------------------------------------------------
  /* Look up a view by id, then by exact name, among the views the user can see */
  static async #findView (req, View, view) {
    const body = await MCPServer.callApiOrThrow(req, {
      method: 'GET',
      url: '/api/views',
      handlers: [View.apiGet]
    });
    const byId = body.views?.find(v => v._id === view);
    if (byId !== undefined) { return byId; }

    const byName = body.views?.filter(v => v.name === view) ?? [];
    if (byName.length > 1) {
      throw new MCPToolError(`${byName.length} views are named ${ArkimeUtil.safeStr(view)}, use the id instead`);
    }
    if (byName.length === 0) {
      throw new MCPToolError(`No view with id or name ${ArkimeUtil.safeStr(view)}`);
    }
    return byName[0];
  }

  // --------------------------------------------------------------------------
  static #buildTools (apis) {
    const { Integration, View, Overview, LinkGroup } = apis;

    return [
      {
        name: 'cont3xt_classify',
        title: 'Classify an indicator',
        description: 'Work out what kind of indicator a string is (ip, domain, url, email, hash, phone or text) without querying anything. Cheap, no network calls.',
        annotations: { readOnlyHint: true },
        inputSchema: {
          type: 'object',
          properties: { query: { type: 'string', description: 'The indicator to classify.' } },
          required: ['query']
        },
        handler: async (args) => {
          if (typeof args.query !== 'string' || args.query.trim() === '') {
            throw new MCPToolError('query must be a non empty string');
          }
          return Integration.classify(args.query.trim());
        },
        ui: (args) => MCPCont3xtAPIs.#searchUiQuery(args.query.trim())
      },
      {
        name: 'cont3xt_list_integrations',
        title: 'List integrations',
        description: 'List the intelligence integrations available to the current user, including which indicator types each supports. Use to decide what to pass as doIntegrations.',
        annotations: { readOnlyHint: true },
        inputSchema: { type: 'object', properties: {} },
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          method: 'GET',
          url: '/api/integration',
          handlers: [Integration.apiList]
        }),
        ui: () => ['settings#integrations']
      },
      {
        name: 'cont3xt_search',
        title: 'Enrich indicators',
        description: 'Look up one or more indicators (IPs, domains, URLs, emails, hashes) across every enabled intelligence integration and return the combined results. This is the main cont3xt tool. Only integrations the user has configured and is allowed to see are queried. The result includes uiUrl, a link that runs the same search in the Cont3xt web UI.',
        annotations: { readOnlyHint: true },
        inputSchema: {
          type: 'object',
          properties: {
            query: { type: 'string', description: 'One or more indicators, separated by spaces, commas or tabs.' },
            doIntegrations: {
              type: 'array',
              items: { type: 'string' },
              description: 'Restrict to these integration names. Fewer integrations means a faster answer.'
            },
            view: { type: 'string', description: 'Id or name of a saved view, as listed by cont3xt_views. Runs only the integrations in the view, unless doIntegrations is also given.' },
            skipChildren: { type: 'boolean', description: 'Do not also look up derived indicators (the domain of an email, the host of a url).' },
            skipCache: { type: 'boolean', description: 'Bypass the cache and re-query the integrations.' }
          },
          required: ['query']
        },
        handler: async (args, req) => {
          let doIntegrations = args.doIntegrations;
          let view;
          if (args.view !== undefined) {
            const found = await MCPCont3xtAPIs.#findView(req, View, args.view);
            view = { id: found._id, name: found.name };
            doIntegrations ??= found.integrations ?? [];
          }

          const data = await MCPCont3xtAPIs.#runSearch(req, {
            url: '/api/integration/search',
            body: {
              query: args.query,
              ...(doIntegrations ? { doIntegrations } : {}),
              ...(view ? { viewId: view.id } : {}),
              ...(args.skipChildren !== undefined ? { skipChildren: args.skipChildren } : {}),
              ...(args.skipCache !== undefined ? { skipCache: args.skipCache } : {})
            },
            handlers: [Integration.apiSearch]
          });
          if (view) { data.view = view; }
          return data;
        },
        ui: (args, data) => MCPCont3xtAPIs.#searchUiQuery(args.query, { submit: true, view: data.view?.id, skipChildren: args.skipChildren })
      },
      {
        name: 'cont3xt_integration_search',
        title: 'Query one integration',
        description: 'Look up a single indicator against a single named integration. Use when cont3xt_search is more than you need, or to re-check one source.',
        annotations: { readOnlyHint: true },
        inputSchema: {
          type: 'object',
          properties: {
            query: { type: 'string', description: 'The indicator to look up.' },
            itype: { type: 'string', enum: ['ip', 'domain', 'url', 'email', 'hash', 'phone', 'text'], description: 'The indicator type, must match what cont3xt_classify returns for the query.' },
            integration: { type: 'string', description: 'The integration name, as listed by cont3xt_list_integrations.' }
          },
          required: ['query', 'itype', 'integration']
        },
        handler: async (args, req) => {
          const body = await MCPServer.callApiOrThrow(req, {
            url: `/api/integration/${args.itype}/${args.integration}/search`,
            params: { itype: args.itype, integration: args.integration },
            body: { query: args.query },
            timeout: +ArkimeConfig.get('mcpCont3xtTimeout', 60000),
            handlers: [Integration.apiSingleSearch]
          });

          if (body?.purpose === 'error' || body?.purpose === 'fail') {
            throw new MCPToolError(body.text ?? 'Integration search failed');
          }
          return body;
        },
        ui: (args) => MCPCont3xtAPIs.#searchUiQuery(args.query)
      },
      {
        name: 'cont3xt_views',
        title: 'List views',
        description: 'List the saved cont3xt views available to the current user. A view id or name can be passed as the view parameter of cont3xt_search to restrict which integrations run.',
        annotations: { readOnlyHint: true },
        inputSchema: { type: 'object', properties: {} },
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          method: 'GET',
          url: '/api/views',
          handlers: [View.apiGet]
        }),
        ui: () => ['settings#views']
      },
      {
        name: 'cont3xt_overviews',
        title: 'List overviews',
        description: 'List the saved cont3xt overviews, which define which fields are summarised for each indicator type.',
        annotations: { readOnlyHint: true },
        inputSchema: { type: 'object', properties: {} },
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          method: 'GET',
          url: '/api/overview',
          handlers: [Overview.apiGet]
        }),
        ui: () => ['settings#overviews']
      },
      {
        name: 'cont3xt_link_groups',
        title: 'List link groups',
        description: 'List the configured cont3xt link groups, the pivot links shown for an indicator.',
        annotations: { readOnlyHint: true },
        inputSchema: { type: 'object', properties: {} },
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          method: 'GET',
          url: '/api/linkGroup',
          handlers: [LinkGroup.apiGet]
        }),
        ui: () => ['settings#linkgroups']
      },
      {
        name: 'cont3xt_ui_link',
        title: 'Link to the web UI',
        description: 'Build a link that opens the Cont3xt web UI with the given indicators filled in, without querying anything. Use when the user wants to continue in the browser, or to share a search. cont3xt_search already returns uiUrl for the search it ran.',
        annotations: { readOnlyHint: true },
        inputSchema: {
          type: 'object',
          properties: {
            query: { type: 'string', description: 'One or more indicators, separated by spaces, commas or tabs.' },
            view: { type: 'string', description: 'Id or name of a saved view to select.' },
            skipChildren: { type: 'boolean', description: 'Do not look up derived indicators.' },
            submit: { type: 'boolean', description: 'Run the search as soon as the page opens, instead of only filling it in. Defaults to false.' }
          },
          required: ['query']
        },
        handler: async (args) => {
          if (typeof args.query !== 'string' || args.query.trim() === '') {
            throw new MCPToolError('query must be a non empty string');
          }
          const link = MCPCont3xtAPIs.#searchUiQuery(args.query.trim(), { submit: args.submit, view: args.view, skipChildren: args.skipChildren });
          if (link === undefined) { throw new MCPToolError('The web UI can only link to indicators made of latin1 characters'); }
          const url = MCPServer.webUrl(...link);
          if (url === undefined) { throw new MCPToolError('No web UI url is configured'); }
          return { uiUrl: url };
        }
      }
    ];
  }
}

module.exports = MCPCont3xtAPIs;
