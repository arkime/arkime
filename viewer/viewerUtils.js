/******************************************************************************/
/* viewerUtils.js -- shared util functions
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const Config = require('./config.js');
const Db = require('./db.js');
const http = require('http');
const https = require('https');
const util = require('util');
const ArkimeUtil = require('../common/arkimeUtil');
const Auth = require('../common/auth');
const User = require('../common/user');
const internals = require('./internals');
const PacketPortal = require('./packetPortal');

class ViewerUtils {
  static METRIC_AGGS = ['sum', 'avg', 'min', 'max', 'cardinality'];
  static #oldDBFields = new Map();
  static #caTrustCerts = new Map();
  static #hostnameRE = /^(?:[a-zA-Z0-9._-]+|\[[0-9a-fA-F:]+\])$/;

  // ----------------------------------------------------------------------------
  static addCaTrust (options, node) {
    if (!Config.isHTTPS(node)) {
      return;
    }

    let certs = ViewerUtils.#caTrustCerts.get(node);
    if (certs === null) { return; }
    if (!certs) {
      const caTrustFile = Config.getFull(node, 'caTrustFile');
      if (!caTrustFile) {
        ViewerUtils.#caTrustCerts.set(node, null);
        return;
      }
      certs = ArkimeUtil.certificateFileToArray(caTrustFile);
      ViewerUtils.#caTrustCerts.set(node, certs);
    }

    if (certs && certs.length > 0) {
      options.ca = certs;
    }
  }

  // ----------------------------------------------------------------------------
  static queryValueToArray (val) {
    if (val === undefined || val === null) {
      return [];
    }
    if (!Array.isArray(val)) {
      val = [val];
    }
    return val.join(',').split(',');
  }

  // ----------------------------------------------------------------------------
  static mapMerge (aggregations) {
    const map = { src: {}, dst: {}, xffGeo: {} };

    if (!aggregations || !aggregations.mapG1) {
      return {};
    }

    for (const item of aggregations.mapG1.buckets) {
      map.src[item.key] = item.doc_count;
    }

    for (const item of aggregations.mapG2.buckets) {
      map.dst[item.key] = item.doc_count;
    }

    for (const item of aggregations.mapG3.buckets) {
      map.xffGeo[item.key] = item.doc_count;
    }

    return map;
  }

  // ----------------------------------------------------------------------------
  // Resolve a dashboard metric field exp to its dbField, but only when it names a
  // numeric (integer) field whose per-value sum is meaningful — matching the metric
  // set the widget editor and timeline data filters offer. Returns undefined for
  // 'sessions', unknown, or non-numeric fields so callers fall back to a
  // session-count metric instead of building an invalid sum aggregation.
  // A cardinality (unique count) aggregation is meaningful on any field, so that
  // agg accepts every field with a dbField.
  static metricDbField (metricExp, agg) {
    if (!ArkimeUtil.isString(metricExp) || metricExp === 'sessions') { return undefined; }
    const field = Config.getFieldsMap()[metricExp];
    if (!field?.dbField) { return undefined; }
    if (agg === 'cardinality') { return field.dbField; }
    return field.type === 'integer' ? field.dbField : undefined;
  }

  // ----------------------------------------------------------------------------
  // The aggregation a dashboard metric uses per bucket; anything unknown is a sum.
  static metricAgg (agg) {
    return ViewerUtils.METRIC_AGGS.includes(agg) ? agg : 'sum';
  }

  // ----------------------------------------------------------------------------
  // A requested count (eg a terms size) as an integer from 1 to max (if given);
  // anything else is the fallback. A fractional size reaches array lengths in
  // multies.
  static clampInt (val, fallback, max = Infinity) {
    const n = parseInt(val);
    if (!Number.isFinite(n) || n <= 0) { return fallback; }
    return Math.min(n, max);
  }

  // ----------------------------------------------------------------------------
  // A dashboard widget can request an extra metric field (already resolved to a
  // dbField via metricDbField); append it to the timeline data filters so its
  // per-bucket sum / <dbField>Histo series is built alongside the user's filters.
  static withMetricField (filters, reqQuery) {
    const metricField = reqQuery?.metricField;
    if (metricField && !filters.includes(metricField)) {
      return [...filters, metricField];
    }
    return filters;
  }

  // ----------------------------------------------------------------------------
  // the hour of week fold (interval=hourOfWeek): a weekday and hour reader per
  // timezone, made once and kept (keyed by the zone's canonical name, so the
  // map is bounded by the zones the runtime knows)
  static #hourOfWeekFormats = new Map();
  static #WEEKDAYS = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];

  /**
   * The timezone an hour of week fold places buckets in: the request's IANA
   * zone name when the runtime can use it, else UTC. Checked by making a
   * formatter rather than against Intl.supportedValuesOf, which only lists one
   * name per zone (Asia/Calcutta but not Asia/Kolkata, which browsers send).
   * @param {string} timezone - the request's timezone parameter
   * @returns {string}
   */
  static hourOfWeekTimezone (timezone) {
    if (!ArkimeUtil.isString(timezone) || timezone.length > 64) { return 'UTC'; }
    if (/^(utc|gmt|etc\/utc|etc\/gmt)$/i.test(timezone)) { return 'UTC'; }
    try {
      return new Intl.DateTimeFormat('en-US', { timeZone: timezone }).resolvedOptions().timeZone;
    } catch (err) {
      return 'UTC';
    }
  }

  /**
   * How far past UTC's hours a timezone's hours start, in ms (0 .. 1 hour), so
   * hour buckets can start on its hours: 30 minutes for Asia/Kolkata (+05:30,
   * its 16:00 is 10:30 UTC) or America/St_Johns (-03:30), 15 for
   * Asia/Kathmandu (+05:45), 0 for most zones. Taken
   * at one time; the few zones whose part hour changes with DST
   * (Australia/Lord_Howe) are off by it for part of the year.
   * @param {string} timezone - a zone hourOfWeekTimezone returned
   * @param {number} ms - the time to take the offset at
   * @returns {number}
   */
  static hourOfWeekOffsetMs (timezone, ms) {
    const fmt = new Intl.DateTimeFormat('en-US', { timeZone: timezone, timeZoneName: 'longOffset' });
    const offsetName = fmt.formatToParts(new Date(ms)).find(p => p.type === 'timeZoneName')?.value ?? '';
    const m = offsetName.match(/([+-])(\d{2}):(\d{2})/); // GMT+05:30, plain GMT for UTC
    if (!m) { return 0; }
    const minutes = (m[1] === '-' ? -1 : 1) * (parseInt(m[2], 10) * 60 + parseInt(m[3], 10));
    // local hh:00 is UTC hh:00 minus the offset
    return (((-minutes % 60) + 60) % 60) * 60 * 1000;
  }

  /**
   * The hour of week slot (0 = Monday 00:00 .. 167 = Sunday 23:00) a time
   * falls in, in a timezone.
   * @param {number} ms - the time
   * @param {string} timezone - a zone hourOfWeekTimezone returned
   * @returns {number}
   */
  static hourOfWeekSlot (ms, timezone) {
    let fmt = ViewerUtils.#hourOfWeekFormats.get(timezone);
    if (!fmt) {
      fmt = new Intl.DateTimeFormat('en-US', { timeZone: timezone, weekday: 'short', hour: 'numeric', hourCycle: 'h23' });
      ViewerUtils.#hourOfWeekFormats.set(timezone, fmt);
    }
    const parts = fmt.formatToParts(new Date(ms));
    const day = ViewerUtils.#WEEKDAYS.indexOf(parts.find(p => p.type === 'weekday')?.value);
    const hour = parseInt(parts.find(p => p.type === 'hour')?.value, 10) % 24;
    return Math.max(0, day) * 24 + hour;
  }

  /**
   * Folds a graph's hour buckets into a week: every <name>Histo series
   * becomes [[slot, value], ...] by slot (0 = Monday 00:00 .. 167 = Sunday
   * 23:00) in the timezone, values summed, and the graph is marked
   * hourOfWeek. hourOfWeekHours is [[slot, hours], ...], the hours with
   * sessions folded into each slot. The totals are left as they are, and
   * missing bounds (an all time search) are taken from the buckets before they
   * are folded, so they stay times rather than slots.
   * @param {object} graph - a graphMerge graph (hour interval)
   * @param {string} timezone - the request's timezone parameter
   * @param {Map} [slotCache] - bucket time -> slot, shared by the graphs of
   *   one request so each bucket is only placed once
   */
  static foldHourOfWeek (graph, timezone, slotCache = new Map()) {
    const tz = ViewerUtils.hourOfWeekTimezone(timezone);
    let min = Infinity;
    let max = -Infinity;
    for (const key of Object.keys(graph)) {
      if (!key.endsWith('Histo') || !Array.isArray(graph[key])) { continue; }
      const slots = new Map();
      const hours = new Map();
      for (const [ms, value] of graph[key]) {
        if (ms < min) { min = ms; }
        if (ms > max) { max = ms; }
        let slot = slotCache.get(ms);
        if (slot === undefined) {
          slot = ViewerUtils.hourOfWeekSlot(ms, tz);
          slotCache.set(ms, slot);
        }
        slots.set(slot, (slots.get(slot) || 0) + value);
        hours.set(slot, (hours.get(slot) || 0) + 1);
      }
      graph[key] = [...slots.entries()].sort((a, b) => a[0] - b[0]);
      // every bucket has sessions (min_doc_count 1), so sessionsHisto has one per hour
      if (key === 'sessionsHisto') { graph.hourOfWeekHours = [...hours.entries()].sort((a, b) => a[0] - b[0]); }
    }
    if (graph.xmin === null && min !== Infinity) { graph.xmin = min; }
    if (graph.xmax === null && max !== -Infinity) { graph.xmax = max; }
    graph.hourOfWeek = true;
    // the zone as the request named it (Asia/Kolkata, not the runtime's
    // Asia/Calcutta), UTC when the request's wasn't one
    graph.timezone = tz === 'UTC' ? 'UTC' : timezone;
  }

  // ----------------------------------------------------------------------------
  static graphMerge (req, query, aggregations) {
    const filterNameMap = { totPackets: 'network.packets', totBytes: 'network.bytes' };
    let filters = req.user.settings.timelineDataFilters || internals.settingDefaults.timelineDataFilters;

    // Convert old names to new names locally
    filters = filters.map(x => filterNameMap[x] ?? x);

    filters = ViewerUtils.withMetricField(filters, req.query);

    const graph = {
      xmin: req.query.startTime * 1000 || null,
      xmax: req.query.stopTime * 1000 || null,
      interval: query?.aggregations?.dbHisto ? query.aggregations.dbHisto.histogram.interval / 1000 || 60 : 60,
      sessionsHisto: [],
      sessionsTotal: 0
    };

    // allowed tot* data map
    const filtersMap = {
      totPackets: ['source.packets', 'destination.packets'],
      'network.packets': ['source.packets', 'destination.packets'],
      totBytes: ['source.bytes', 'destination.bytes'],
      'network.bytes': ['source.bytes', 'destination.bytes'],
      totDataBytes: ['client.bytes', 'server.bytes']
    };

    for (const filter of filters) {
      if (filtersMap[filter] !== undefined) {
        for (const j of filtersMap[filter]) {
          graph[j + 'Histo'] = [];
        }
      } else {
        graph[filter + 'Histo'] = [];
      }

      graph[filter + 'Total'] = 0;
    }

    // the hour of week fold: hour buckets become a week of slots, placed once
    // per request (a SPI Graph request merges a graph per value)
    const hourOfWeek = req.query.interval === 'hourOfWeek';
    if (hourOfWeek) { req.hourOfWeekSlots ??= new Map(); }

    if (!aggregations || !aggregations.dbHisto) {
      if (hourOfWeek) { ViewerUtils.foldHourOfWeek(graph, req.query.timezone, req.hourOfWeekSlots); }
      return graph;
    }

    for (const item of aggregations.dbHisto.buckets) {
      const key = item.key;

      // always add session information
      graph.sessionsHisto.push([key, item.doc_count]);
      graph.sessionsTotal += item.doc_count;

      for (const prop in item) {
        // excluding every item prop that isn't a summed up aggregate collection (ie. es keys)
        // tot* filters are exceptions: they will pass src/dst histo [], but keep a *Total count for filtered total
        // ie. totPackets selected filter => {srcPacketsHisto: [], dstPacketsHisto:[], totPacketsTotal: n, ...}
        if (filters.includes(prop) ||
          prop === 'source.packets' || prop === 'destination.packets' || prop === 'source.bytes' ||
          prop === 'destination.bytes' || prop === 'client.bytes' || prop === 'server.bytes') {
          // Note: prop will never be one of the chosen tot* exceptions
          graph[prop + 'Histo'].push([key, item[prop].value]);

          // Need to specify for when src/dst AND tot* filters are chosen
          if (filters.includes(prop)) {
            graph[prop + 'Total'] += item[prop].value;
          }

          // Add src/dst to tot* counters.
          if ((prop === 'source.packets' || prop === 'destination.packets') && filters.includes('network.packets')) {
            graph['network.packetsTotal'] += item[prop].value;
          } else if ((prop === 'source.bytes' || prop === 'destination.bytes') && filters.includes('network.bytes')) {
            graph['network.bytesTotal'] += item[prop].value;
          } else if ((prop === 'client.bytes' || prop === 'server.bytes') && filters.includes('totDataBytes')) {
            graph.totDataBytesTotal += item[prop].value;
          }
        }
      }
    }

    if (hourOfWeek) { ViewerUtils.foldHourOfWeek(graph, req.query.timezone, req.hourOfWeekSlots); }
    return graph;
  }

  // ----------------------------------------------------------------------------
  static errorString (err, result) {
    let str;
    if (err && typeof err === 'string') {
      str = err;
    } else if (err && typeof err.message === 'string') {
      str = err.message;
    } else if (result && result.error) {
      str = result.error;
    } else {
      str = 'Unknown issue, check logs';
      console.log(err, result);
    }

    if (str.includes('IndexMissingException')) {
      return "Arkime's OpenSearch/Elasticsearch database has no matching session indices for the timeframe selected.";
    } else {
      return 'OpenSearch/Elasticsearch error: ' + str;
    }
  }

  // ----------------------------------------------------------------------------
  static async loadFields () {
    try {
      let data = await Db.loadFields();
      // Field docs can be written by sensors, drop a bad one before anything reads it
      data = data.hits.hits.filter((field) => {
        if (Config.isValidFieldDoc(field)) { return true; }
        console.log('WARNING - skipping invalid field definition', ArkimeUtil.sanitizeStr(JSON.stringify(field?._id)));
        return false;
      });

      // Everything will use fieldECS or dbField2 as dbField
      for (let i = 0, ilen = data.length; i < ilen; i++) {
        if (data[i]._source.fieldECS) {
          ViewerUtils.#oldDBFields.set(data[i]._source.dbField, data[i]._source);
          ViewerUtils.#oldDBFields.set(data[i]._source.dbField2, data[i]._source);
          data[i]._source.dbField = data[i]._source.fieldECS;
        } else {
          ViewerUtils.#oldDBFields.set(data[i]._source.dbField, data[i]._source);
          data[i]._source.dbField = data[i]._source.dbField2;
        }

        if (data[i]._source.portFieldECS) {
          data[i]._source.portField = data[i]._source.portFieldECS;
        } else if (data[i]._source.portField2) {
          data[i]._source.portField = data[i]._source.portField2;
        } else {
          delete data[i]._source.portField;
        }
        delete data[i]._source.rawField;
      }

      Config.loadFields(data);

      return {
        fieldsMap: JSON.stringify(Config.getFieldsMap()),
        fieldsArr: Config.getFields().sort((a, b) => {
          return a.exp.localeCompare(b.exp);
        })
      };
    } catch (err) {
      return { fieldsMap: '{}', fieldsArr: [] };
    }
  }

  // ----------------------------------------------------------------------------
  static oldDB2newDB (x) {
    const old = ViewerUtils.#oldDBFields.get(x);

    if (old === undefined) { return x; }
    return old.fieldECS ?? old.dbField2;
  }

  // ----------------------------------------------------------------------------
  static mergeUnarray (to, from) {
    for (const key in from) {
      if (!Object.prototype.hasOwnProperty.call(from, key)) { continue; }
      if (Array.isArray(from[key])) {
        to[key] = from[key][0];
      } else {
        to[key] = from[key];
      }
    }
  }

  // ----------------------------------------------------------------------------
  // https://medium.com/dailyjs/rewriting-javascript-converting-an-array-of-objects-to-an-object-ec579cafbfc7
  static arrayToObject (array, key) {
    return array.reduce((obj, item) => {
      obj[item[key]] = item;
      return obj;
    }, {});
  }

  // ----------------------------------------------------------------------------
  static async getViewUrl (node) {
    if (Array.isArray(node)) {
      node = node[0];
    }

    const url = Config.getFull(node, 'viewUrl');
    let isHttps, client;
    if (url) {
      if (Config.debug > 1) {
        console.log(`DEBUG: node:${node} is using ${url} because viewUrl was set for ${node} in config file`);
      }
      isHttps = url.startsWith('https');
      client = isHttps ? https : http;
      return { viewUrl: url, client };
    }

    const stat = await Db.arkimeNodeStatsCache(node);

    if (Config.debug > 1) {
      console.log(`DEBUG: node:${node} is using ${stat.hostname} from OpenSearch/Elasticsearch stats index`);
    }

    if (!stat.hostname || !ViewerUtils.#hostnameRE.test(stat.hostname)) {
      throw new Error(`Invalid hostname for node ${ArkimeUtil.sanitizeStr(node)} from stats index: ${ArkimeUtil.sanitizeStr(stat.hostname)}`);
    }

    isHttps = Config.isHTTPS(node);
    const protocol = isHttps ? 'https' : 'http';
    client = isHttps ? https : http;
    const result = protocol + '://' + stat.hostname + ':' + Config.getFull(node, 'viewPort', '8005');
    return { viewUrl: result, client };
  }

  // ----------------------------------------------------------------------------
  static getClusterSecret (cluster) {
    if (cluster && internals.remoteClusters) {
      const sobj = internals.remoteClusters[cluster];
      if (sobj) {
        return sobj.serverSecret || sobj.passwordSecret;
      }
    }
    return undefined;
  }

  // ----------------------------------------------------------------------------
  // When a packet portal to node is live, route the request over it instead of
  // dialing the node directly. Returns { viewUrl, client, options } ready to use,
  // or null when the caller should getViewUrl() as usual.
  //
  // A node that has connected via packet portal is behind NAT and cannot be
  // dialed directly, so if its portal is momentarily down we wait up to
  // packetPortalWaitSeconds for it to come back rather than falling back to a
  // (doomed) direct connection; if it never comes back we throw a clear error.
  static async portalRequest (node) {
    let session = PacketPortal.get(node);
    if (!session && PacketPortal.isReverse(node)) {
      const waitSecs = +Config.get('packetPortalWaitSeconds', 5);
      const waitMs = (isNaN(waitSecs) || waitSecs < 0 ? 5 : waitSecs) * 1000;
      session = await PacketPortal.getOrWait(node, waitMs);
      if (!session) {
        const err = new Error(`Packet portal to node '${node}' is not connected`);
        err.portalUnreachable = true;
        throw err;
      }
    }
    if (!session) { return null; }
    return {
      viewUrl: 'http://arkime-portal.invalid/',
      client: http, // inner protocol carried over the portal is plaintext HTTP/1.1
      options: {
        timeout: 20 * 60 * 1000,
        agent: PacketPortal.agent,
        packetPortalSession: session
      }
    };
  }

  // ----------------------------------------------------------------------------
  // new URL() collapses dot segments (also %2e, \ and tab/newline forms), so the
  // path we sign for the peer would not be the one routed here, never allow them
  static #hasDotSegment (path) {
    return path.replace(/[\t\n\r]/g, '').split('?')[0].split(/[/\\]/).some((segment) => {
      segment = segment.toLowerCase().replaceAll('%2e', '.');
      return segment === '.' || segment === '..';
    });
  }

  // ----------------------------------------------------------------------------
  static async makeRequest (node, path, user, cb, cluster) {
    try {
      if (ViewerUtils.#hasDotSegment(path)) {
        throw new Error(`Bad path ${path}`);
      }

      let viewUrl, client, options;
      const portal = await ViewerUtils.portalRequest(node);
      if (portal) {
        ({ viewUrl, client, options } = portal);
      } else {
        ({ viewUrl, client } = await ViewerUtils.getViewUrl(node));
        options = {
          timeout: 20 * 60 * 1000,
          agent: client === http ? internals.httpAgent : internals.httpsAgent
        };
      }

      const nodePath = encodeURI(path);
      let url;
      if (nodePath.startsWith('/')) {
        url = new URL(nodePath.substring(1), viewUrl);
      } else {
        url = new URL(nodePath, viewUrl);
      }

      // Sign the path exactly as the remote node will see it in req.url, query
      // string included, otherwise any call with parameters fails s2s auth
      Auth.addS2SAuth(options, user, node, url.pathname + (url.search ?? ''), ViewerUtils.getClusterSecret(cluster));
      if (!portal) { ViewerUtils.addCaTrust(options, node); }

      function responseFunc (pres) {
        let response = '';
        pres.on('data', function (chunk) {
          response += chunk;
        });
        pres.on('end', function () {
          cb(null, response);
        });
      }

      const preq = client.request(url, options, responseFunc);
      preq.on('error', (err) => {
        // Try a second time on errors
        console.log(`Retry ${url.pathname}${url.search} on remote viewer: ${err}`);
        const preq2 = client.request(url, options, responseFunc);
        preq2.on('error', (err) => {
          console.log(`Error with ${url.pathname}${url.search} on remote viewer: ${err}`);
          cb(err);
        });
        preq2.end();
      });
      preq.end();
    } catch (err) {
      cb(err);
    }
  }

  // --------------------------------------------------------------------------
  // Like makeRequest but invokes onResponse(IncomingMessage) so the caller can
  // stream/pipe the body instead of buffering. No retry on error.
  static async makeStreamRequest (node, path, user, onResponse, onError, cluster) {
    try {
      // A fragment is dropped by new URL() and never signed, so the peer would
      // route a different path than the caller built
      if (ViewerUtils.#hasDotSegment(path) || path.includes('#')) {
        throw new Error(`Bad path ${path}`);
      }

      const { viewUrl, client } = await ViewerUtils.getViewUrl(node);
      const nodePath = encodeURI(path);
      let url;
      if (nodePath.startsWith('/')) {
        url = new URL(nodePath.substring(1), viewUrl);
      } else {
        url = new URL(nodePath, viewUrl);
      }
      const options = {
        timeout: 20 * 60 * 1000,
        agent: client === http ? internals.httpAgent : internals.httpsAgent
      };
      Auth.addS2SAuth(options, user, node, url.pathname + (url.search ?? ''), ViewerUtils.getClusterSecret(cluster));
      ViewerUtils.addCaTrust(options, node);
      const preq = client.request(url, options, onResponse);
      preq.on('error', (err) => onError(err));
      preq.end();
    } catch (err) {
      onError(err);
    }
  }

  // --------------------------------------------------------------------------
  static async proxyRequest (req, res) {
    ArkimeUtil.noCache(req, res);

    if (ViewerUtils.#hasDotSegment(req.url)) {
      console.log('ERROR - proxyRequest bad path', ArkimeUtil.sanitizeStr(req.url));
      return res.status(400).send('Bad path');
    }

    try {
      let viewUrl, client, options;
      const portal = await ViewerUtils.portalRequest(req.params.nodeName);
      if (portal) {
        ({ viewUrl, client, options } = portal);
      } else {
        ({ viewUrl, client } = await ViewerUtils.getViewUrl(req.params.nodeName));
        options = {
          timeout: 20 * 60 * 1000,
          agent: client === http ? internals.httpAgent : internals.httpsAgent
        };
      }

      let url;
      if (req.url.startsWith('/')) {
        url = new URL(req.url.substring(1), viewUrl);
      } else {
        url = new URL(req.url, viewUrl);
      }

      const urlPath = url.pathname + (url.search ?? '');
      Auth.addS2SAuth(options, req.user, req.params.nodeName, urlPath, ViewerUtils.getClusterSecret(req.query.cluster));
      if (!portal) { ViewerUtils.addCaTrust(options, req.params.nodeName); }

      const preq = client.request(url, options, (pres) => {
        if (pres.headers['content-type']) {
          res.setHeader('content-type', pres.headers['content-type']);
        }
        if (pres.headers['content-disposition']) {
          res.setHeader('content-disposition', pres.headers['content-disposition']);
        }
        pres.on('data', (chunk) => {
          res.write(chunk);
        });
        pres.on('end', () => {
          res.end();
        });
      });

      preq.on('error', (e) => {
        console.log("ERROR - Couldn't proxy request=", url, '\nerror=', util.inspect(e, false, 50), '\nYou might want to run viewer with two --debug for more info');
        res.send(`Error talking to node '${ArkimeUtil.safeStr(req.params.nodeName)}' using host '${url.host}' check viewer logs on '${Config.hostName()}'`);
      });
      preq.end();
    } catch (err) {
      if (err?.portalUnreachable) {
        console.log('ERROR - packet portal not connected for node:', req.params.nodeName);
        return res.send(`Packet portal to node '${ArkimeUtil.safeStr(req.params.nodeName)}' is not connected, try again later`);
      }
      console.log('ERROR - getViewUrl in proxyRequest - node:', req.params.nodeName, 'err:', util.inspect(err, false, 50));
      res.send(`Can't find view url for '${ArkimeUtil.safeStr(req.params.nodeName)}' check viewer logs on '${Config.hostName()}'`);
    }
  }

  // ----------------------------------------------------------------------------
  static addCluster (cluster, options = {}) {
    if (cluster && Config.getBool('multiES', false)) {
      options.cluster = cluster;
    }
    return options;
  }

  // ----------------------------------------------------------------------------
  // check for anonymous mode before fetching user cache and return anonymous
  // user or the user requested by the userId
  static async getUserCacheIncAnon (userId) {
    if (Auth.isAnonymousMode()) { // user is anonymous
      const anonUser = await User.getUserCache('anonymous');
      const anon = Object.assign(new User(), internals.anonymousUser);

      if (anonUser) {
        anon.settings = anonUser.settings || {};
      }

      await anon.expandFromRoles();

      return anon;
    } else {
      return await User.getUserCache(userId);
    }
  }

  // --------------------------------------------------------------------------
  static async isLocalView (node, yesCb, noCb) {
    if (internals.isLocalViewRegExp && node.match(internals.isLocalViewRegExp)) {
      if (Config.debug > 1) {
        console.log(`DEBUG: node:${node} is local view because matches ${internals.isLocalViewRegExp}`);
      }
      return yesCb ? yesCb() : true;
    }

    const pcapWriteMethod = Config.getFull(node, 'pcapWriteMethod');
    const writer = internals.writers.get(pcapWriteMethod);
    if (writer && writer.localNode === false) {
      if (Config.debug > 1) {
        console.log(`DEBUG: node:${node} is local view because of writer`);
      }
      return yesCb ? yesCb() : true;
    }

    if (await Db.isLocalView(node)) {
      return yesCb ? yesCb() : true;
    } else {
      return noCb ? noCb() : false;
    }
  }
}

module.exports = ViewerUtils;
