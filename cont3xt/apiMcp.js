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
const { normalizeCardField } = require('./normalizeCardField.js');
const { MCPToolError } = MCPServer;

ArkimeConfig.registerValidated({
  mcpMaxResultBytes: { type: 'int', min: 1024, unlimited: -1 }
});

// Rows kept per array/table value in an overview, the full data is one
// cont3xt_integration_search away
const OVERVIEW_MAX_ROWS = 25;

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
      // the ui splits this on commas, the same way the history page rebuilds it
      tags: options.tags?.length ? options.tags.join(',') : undefined,
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
  /* The integration cards the user sees, by integration name, fields normalized */
  static async #cards (req, Integration) {
    const body = await MCPServer.callApiOrThrow(req, {
      method: 'GET',
      url: '/api/integration',
      handlers: [Integration.apiList]
    });

    const cards = new Map();
    for (const [integrationName, integration] of Object.entries(body.integrations ?? {})) {
      if (!Array.isArray(integration.card?.fields)) { continue; }
      cards.set(integrationName, integration.card.fields.map(f => MCPCont3xtAPIs.#normalizeField(f)).filter(f => f !== undefined));
    }
    return cards;
  }

  // --------------------------------------------------------------------------
  /* normalizeCardField throws on a malformed field (user card overrides are free form json), skip those */
  static #normalizeField (field) {
    try {
      const f = normalizeCardField(field);
      return ArkimeUtil.isString(f?.label) ? f : undefined;
    } catch (err) {
      return undefined;
    }
  }

  // --------------------------------------------------------------------------
  /**
   * Pick the overview for each itype: the one asked for, else the user's
   * selection for that itype, else the itype default. Same order as the ui.
   */
  static async #overviews (req, Overview, wanted) {
    const body = await MCPServer.callApiOrThrow(req, {
      method: 'GET',
      url: '/api/overview',
      handlers: [Overview.apiGet]
    });
    const all = (body.overviews ?? []).filter(o => o._viewable !== false);

    let chosen;
    if (wanted !== undefined) {
      chosen = all.find(o => o._id === wanted);
      if (chosen === undefined) {
        const byName = all.filter(o => o.name === wanted);
        if (byName.length > 1) {
          throw new MCPToolError(`${byName.length} overviews are named ${ArkimeUtil.safeStr(wanted)}, use the id instead`);
        }
        if (byName.length === 0) {
          throw new MCPToolError(`No overview with id or name ${ArkimeUtil.safeStr(wanted)}`);
        }
        chosen = byName[0];
      }
    }

    const selected = req.user.cont3xt?.selectedOverviews ?? {};
    return (itype) => {
      if (chosen?.iType === itype) { return chosen; }
      return all.find(o => o._id === selected[itype] && o.iType === itype) ?? all.find(o => o._id === itype);
    };
  }

  // --------------------------------------------------------------------------
  /**
   * The value a card field selects from an integration's data, following
   * formatValue.js in the ui minus the display only parts (defang, post
   * processing). Tables become rows of label:value, dates become ISO strings.
   */
  static #fieldValue (data, field, depth = 0) {
    let value = data;
    for (const p of field.path ?? []) {
      // own properties only, a path from an overview must not reach prototypes
      if (value === null || typeof value !== 'object' || !Object.hasOwn(value, p)) { return undefined; }
      value = value[p];
    }
    if (value === undefined || value === null) { return undefined; }

    if (field.type === 'array' || field.type === 'table') {
      if (!Array.isArray(value)) { value = [value]; }
      for (const p of field.fieldRootPath ?? []) {
        value = value.map(e => (e !== null && typeof e === 'object' && Object.hasOwn(e, p)) ? e[p] : undefined);
      }
      if (field.filterEmpty) {
        value = value.filter(e => e !== undefined && e !== null && e !== '' && !(Array.isArray(e) && e.length === 0));
      }
      if (field.type === 'table' && depth < 2 && Array.isArray(field.fields)) {
        value = value.map((row) => {
          const out = {};
          for (const sub of field.fields) {
            const v = MCPCont3xtAPIs.#fieldValue(row, sub, depth + 1);
            if (v !== undefined) { out[sub.label] = v; }
          }
          return out;
        });
      }
    }

    if (value === '' || (Array.isArray(value) && value.length === 0)) { return undefined; }

    switch (field.type) {
    case 'ms':
    case 'seconds': {
      const date = new Date(field.type === 'ms' ? value : value * 1000);
      return Number.isNaN(date.getTime()) ? value : date.toISOString();
    }
    default:
      return value;
    }
  }

  // --------------------------------------------------------------------------
  /* A card field as an overview entry, rows capped, undefined when empty */
  static #fieldEntry (label, integration, field, data) {
    const value = MCPCont3xtAPIs.#fieldValue(data, field);
    if (value === undefined) { return undefined; }

    const entry = { label, integration, value };
    if (Array.isArray(value) && value.length > OVERVIEW_MAX_ROWS) {
      entry.total = value.length;
      entry.value = value.slice(0, OVERVIEW_MAX_ROWS);
    }
    return entry;
  }

  // --------------------------------------------------------------------------
  /**
   * Distill one indicator's results the way the ui's overview card does: each
   * overview field names an integration and one of its card fields (or its
   * own custom field), and takes that value from the integration's data.
   */
  static #buildOverview (overview, cards, dataByIntegration) {
    const fields = [];
    const warnings = new Set();

    for (const ref of overview.fields ?? []) {
      const data = dataByIntegration.get(ref.from);
      let field;
      let label;

      if (ref.type === 'custom' || (ref.type === undefined && ref.custom != null)) {
        if (!ArkimeUtil.isString(ref.custom) && !ArkimeUtil.isPlainObject(ref.custom)) {
          warnings.add(`Custom field from ${ref.from} is invalid`);
          continue;
        }
        field = MCPCont3xtAPIs.#normalizeField(ref.custom);
        if (field?.path === undefined) {
          warnings.add(`Custom field from ${ref.from} has no field or label to read`);
          continue;
        }
        label = field.label;
      } else {
        field = cards.get(ref.from)?.find(f => f.label === ref.field);
        if (field === undefined) {
          warnings.add(`Unable to find linked field '${ref.field}' in integration '${ref.from}'`);
          continue;
        }
        label = ref.alias ?? field.label;
      }

      if (data === undefined) { continue; } // that integration returned nothing for this indicator
      const entry = MCPCont3xtAPIs.#fieldEntry(label, ref.from, field, data);
      if (entry !== undefined) { fields.push(entry); }
    }

    return { fields, warnings: warnings.size ? [...warnings] : undefined };
  }

  // --------------------------------------------------------------------------
  /**
   * Overview mode: replace the raw per integration data with the overview for
   * each indicator, plus which integrations answered so the model knows what
   * cont3xt_integration_search could still fetch.
   */
  static async #summarize (req, apis, data, wantedOverview) {
    const [cards, overviewFor] = await Promise.all([
      MCPCont3xtAPIs.#cards(req, apis.Integration),
      MCPCont3xtAPIs.#overviews(req, apis.Overview, wantedOverview)
    ]);

    // results per indicator, top level indicators first then children
    const byIndicator = new Map();
    const key = (i) => JSON.stringify([i.itype, i.query]);
    for (const indicator of data.indicators ?? []) {
      byIndicator.set(key(indicator), { indicator: { query: indicator.query, itype: indicator.itype }, data: new Map() });
    }
    for (const r of data.results) {
      const k = key(r.indicator);
      if (!byIndicator.has(k)) {
        byIndicator.set(k, { indicator: { query: r.indicator.query, itype: r.indicator.itype }, data: new Map() });
      }
      byIndicator.get(k).data.set(r.integration, r.data);
    }

    data.overviews = [...byIndicator.values()].map(({ indicator, data: dataByIntegration }) => {
      const overview = overviewFor(indicator.itype);
      if (overview === undefined) {
        return { indicator, fields: [], warnings: [`No overview is configured for the ${indicator.itype} itype`] };
      }
      const { fields, warnings } = MCPCont3xtAPIs.#buildOverview(overview, cards, dataByIntegration);
      return { indicator, overview: { id: overview._id, name: overview.name }, fields, warnings };
    });

    data.results = data.results.map(r => ({
      integration: r.integration,
      indicator: r.indicator,
      count: r.data?._cont3xt?.count
    }));
  }

  // --------------------------------------------------------------------------
  static #bytes (value) {
    return Buffer.byteLength(JSON.stringify(value) ?? '');
  }

  /**
   * Keep the result under mcpMaxResultBytes by emptying the largest of
   * `entries[key]` first, marking each one omitted rather than silently
   * shrinking. Sets `truncated` on the result when anything went.
   */
  static #fit (data, entries, key) {
    const limit = ArkimeConfig.getInt('mcpMaxResultBytes', 100000);
    if (limit < 0) { return data; }

    let size = MCPCont3xtAPIs.#bytes(data);
    if (size <= limit) { return data; }

    // Added before shrinking so the note itself is inside the budget
    data.truncated = {
      bytes: size,
      limit,
      omitted: 0,
      hint: 'Entries marked omitted were too large for one result. Fetch one with cont3xt_integration_search, or narrow the search with a view or doIntegrations.'
    };

    const candidates = entries
      .filter(e => e[key] !== undefined)
      .map(e => ({ entry: e, bytes: MCPCont3xtAPIs.#bytes(e[key]) }))
      .sort((a, b) => b.bytes - a.bytes);

    // Track the size as we go instead of re-serializing the whole result per
    // entry, a bulk search can have thousands. Each removal swaps the value
    // for the omitted marker, so account for both.
    size += MCPCont3xtAPIs.#bytes(data.truncated) + ',"truncated":'.length;
    for (const { entry, bytes } of candidates) {
      if (size <= limit) { break; }
      delete entry[key];
      entry.omitted = true;
      entry.bytes = bytes;
      data.truncated.omitted++;
      size += `,"omitted":true,"bytes":${bytes}`.length - `"${key}":`.length - bytes;
    }
    return data;
  }

  // --------------------------------------------------------------------------
  /* Resolve the view to search with: the argument, else mcpDefaultView */
  static async #searchView (req, View, args) {
    if (args.view !== undefined) { return MCPCont3xtAPIs.#findView(req, View, args.view); }
    if (args.doIntegrations !== undefined) { return undefined; }

    const viewName = ArkimeConfig.get('mcpDefaultView');
    if (viewName === undefined || viewName === '') { return undefined; }
    try {
      return await MCPCont3xtAPIs.#findView(req, View, viewName);
    } catch (err) {
      throw new MCPToolError(`mcpDefaultView is set but unusable: ${err.message}`);
    }
  }

  // --------------------------------------------------------------------------
  static #buildTools (apis) {
    const { Integration, View, Overview, LinkGroup } = apis;

    // Tools that cap their result size attach the link themselves before
    // capping, mcpServer only adds one when the handler didn't
    const attachLink = (data, link) => {
      const url = link && MCPServer.webUrl(...link);
      if (url !== undefined) { data.uiUrl = url; }
      return data;
    };

    // An explicit doIntegrations restriction with no view has nowhere to go
    // in the URL (the web UI only reads a view from the query string), so a
    // link built without it would search a different, usually wider, set of
    // integrations than what was actually run - omit it instead.
    const searchLink = (args, data) => {
      if (args.doIntegrations && !data.view) { return undefined; }
      return MCPCont3xtAPIs.#searchUiQuery(args.query, { submit: true, view: data.view?.id, tags: args.tags, skipChildren: args.skipChildren });
    };

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
        description: 'Look up one or more indicators (IPs, domains, URLs, emails, hashes) across the enabled intelligence integrations. This is the main cont3xt tool. ' +
          'By default (detail=overview) the answer is the same distilled overview the web UI shows for each indicator, plus the list of integrations that had data, so it stays small. ' +
          'Use cont3xt_integration_search to pull the full data of one integration from that list. ' +
          'detail=full returns every integration\'s raw data and can be very large; results over mcpMaxResultBytes have their biggest entries marked omitted. ' +
          'Every integration is queried unless a view or doIntegrations narrows it, so prefer a view for routine lookups. ' +
          'The result includes uiUrl, a link that runs the same search in the Cont3xt web UI.',
        annotations: { readOnlyHint: true },
        inputSchema: {
          type: 'object',
          properties: {
            query: { type: 'string', description: 'One or more indicators, separated by spaces, commas or tabs.' },
            detail: {
              type: 'string',
              enum: ['overview', 'full'],
              description: 'overview (default) returns the overview fields per indicator and which integrations answered. full returns the raw data of every integration.'
            },
            overview: { type: 'string', description: 'Id or name of an overview, as listed by cont3xt_overviews, to use instead of the default one for its indicator type. Only applies with detail=overview.' },
            doIntegrations: {
              type: 'array',
              items: { type: 'string' },
              description: 'Restrict to these integration names. Fewer integrations means a faster, smaller answer.'
            },
            view: { type: 'string', description: 'Id or name of a saved view, as listed by cont3xt_views. Runs only the integrations in the view, unless doIntegrations is also given.' },
            tags: {
              type: 'array',
              items: { type: 'string' },
              description: 'Tags recorded with this search in the cont3xt history, for example a case or ticket id.'
            },
            skipChildren: { type: 'boolean', description: 'Do not also look up derived indicators (the domain of an email, the host of a url).' },
            skipCache: { type: 'boolean', description: 'Bypass the cache and re-query the integrations.' }
          },
          required: ['query']
        },
        handler: async (args, req) => {
          if (args.detail !== undefined && args.detail !== 'overview' && args.detail !== 'full') {
            throw new MCPToolError('detail must be overview or full');
          }

          let doIntegrations = args.doIntegrations;
          let view;
          const found = await MCPCont3xtAPIs.#searchView(req, View, args);
          if (found !== undefined) {
            view = { id: found._id, name: found.name };
            doIntegrations ??= found.integrations;
          }

          const data = await MCPCont3xtAPIs.#runSearch(req, {
            url: '/api/integration/search',
            body: {
              query: args.query,
              ...(doIntegrations ? { doIntegrations } : {}),
              ...(view ? { viewId: view.id } : {}),
              ...(args.tags !== undefined ? { tags: args.tags } : {}),
              ...(args.skipChildren !== undefined ? { skipChildren: args.skipChildren } : {}),
              ...(args.skipCache !== undefined ? { skipCache: args.skipCache } : {})
            },
            handlers: [Integration.apiSearch]
          });
          if (view) { data.view = view; }
          attachLink(data, searchLink(args, data));

          if (args.detail === 'full') {
            return MCPCont3xtAPIs.#fit(data, data.results, 'data');
          }

          await MCPCont3xtAPIs.#summarize(req, apis, data, args.overview);
          return MCPCont3xtAPIs.#fit(data, data.overviews.flatMap(o => o.fields), 'value');
        },
        ui: searchLink
      },
      {
        name: 'cont3xt_integration_search',
        title: 'Query one integration',
        description: 'Look up a single indicator against a single named integration and return its full data. Use to drill into an integration that cont3xt_search listed, or to re-check one source. ' +
          'When the data is larger than mcpMaxResultBytes, the integration\'s card fields are returned instead and truncated is set.',
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

          const limit = ArkimeConfig.getInt('mcpMaxResultBytes', 100000);
          const bytes = MCPCont3xtAPIs.#bytes(body); // decided on and reported as the same size
          if (limit < 0 || bytes <= limit) { return body; }

          // Too big to hand over whole: fall back to what the ui's card shows
          const cards = await MCPCont3xtAPIs.#cards(req, Integration);
          const fields = cards.get(body.name) ?? [];
          const summary = {
            purpose: body.purpose,
            indicator: body.indicator,
            name: body.name,
            fields: fields.map(field => MCPCont3xtAPIs.#fieldEntry(field.label, undefined, field, body.data)).filter(e => e !== undefined)
          };
          summary.fields.forEach(e => delete e.integration);
          attachLink(summary, MCPCont3xtAPIs.#searchUiQuery(args.query));
          MCPCont3xtAPIs.#fit(summary, summary.fields, 'value');
          summary.truncated = {
            bytes,
            limit,
            omitted: summary.truncated?.omitted ?? 0,
            hint: 'The raw data was too large for one result, these are the integration\'s card fields instead.'
          };
          return summary;
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
        description: 'List the saved cont3xt overviews, which define which fields are summarised for each indicator type. An overview id or name can be passed as the overview parameter of cont3xt_search.',
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
            tags: { type: 'array', items: { type: 'string' }, description: 'Tags to record with the search when it is submitted.' },
            skipChildren: { type: 'boolean', description: 'Do not look up derived indicators.' },
            submit: { type: 'boolean', description: 'Run the search as soon as the page opens, instead of only filling it in. Defaults to false.' }
          },
          required: ['query']
        },
        handler: async (args) => {
          if (typeof args.query !== 'string' || args.query.trim() === '') {
            throw new MCPToolError('query must be a non empty string');
          }
          if (args.tags !== undefined && !ArkimeUtil.isStringArray(args.tags)) {
            throw new MCPToolError('tags must be an array of strings');
          }
          const link = MCPCont3xtAPIs.#searchUiQuery(args.query.trim(), { submit: args.submit, view: args.view, tags: args.tags, skipChildren: args.skipChildren });
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
