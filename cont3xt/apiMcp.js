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
        }
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
        })
      },
      {
        name: 'cont3xt_search',
        title: 'Enrich indicators',
        description: 'Look up one or more indicators (IPs, domains, URLs, emails, hashes) across every enabled intelligence integration and return the combined results. This is the main cont3xt tool. Only integrations the user has configured and is allowed to see are queried.',
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
            skipChildren: { type: 'boolean', description: 'Do not also look up derived indicators (the domain of an email, the host of a url).' },
            skipCache: { type: 'boolean', description: 'Bypass the cache and re-query the integrations.' }
          },
          required: ['query']
        },
        handler: async (args, req) => MCPCont3xtAPIs.#runSearch(req, {
          url: '/api/integration/search',
          body: {
            query: args.query,
            ...(args.doIntegrations ? { doIntegrations: args.doIntegrations } : {}),
            ...(args.skipChildren !== undefined ? { skipChildren: args.skipChildren } : {}),
            ...(args.skipCache !== undefined ? { skipCache: args.skipCache } : {})
          },
          handlers: [Integration.apiSearch]
        })
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
        }
      },
      {
        name: 'cont3xt_views',
        title: 'List views',
        description: 'List the saved cont3xt views available to the current user. A view id can be passed to cont3xt_search to restrict which integrations run.',
        annotations: { readOnlyHint: true },
        inputSchema: { type: 'object', properties: {} },
        handler: async (args, req) => MCPServer.callApiOrThrow(req, {
          method: 'GET',
          url: '/api/views',
          handlers: [View.apiGet]
        })
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
        })
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
        })
      }
    ];
  }
}

module.exports = MCPCont3xtAPIs;
