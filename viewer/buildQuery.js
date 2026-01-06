/******************************************************************************/
/* buildQuery.js -- Everything for building a sessions query
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const Config = require('./config.js');
const Db = require('./db.js');
const ArkimeUtil = require('../common/arkimeUtil');
const arkimeparser = require('./arkimeparser.js');
const util = require('util');
const internals = require('./internals');

class BuildQuery {

  // --------------------------------------------------------------------------
  /**
   * Adds the sort options to the elasticsearch query
   * @ignore
   * @name addSortToQuery
   * @param {object} query - the elasticsearch query that has been partially built already by buildSessionQuery
   * @param {object} info - the query params from the client
   * @param {string} defaultSort - the default sort
   */
  static #addSortToQuery (query, info, defaultSort) {
    function addSortDefault () {
      if (defaultSort) {
        if (!query.sort) {
          query.sort = [];
        }
        const obj = {};
        obj[defaultSort] = { order: 'asc' };
        obj[defaultSort].missing = '_last';
        query.sort.push(obj);
      }
    }

    if (!info) {
      addSortDefault();
      return;
    }

    // New Method
    if (info.order) {
      if (info.order.length === 0) {
        addSortDefault();
        return;
      }

      if (!query.sort) {
        query.sort = [];
      }

      for (const item of info.order.split(',')) {
        const parts = item.split(':');
        const field = parts[0];
        if (ArkimeUtil.isPP(field)) { continue; }

        const obj = {};
        obj[field] = { order: parts[1] };

        let unmappedType = 'string';
        const fieldInfo = Config.getDBFieldsMap()[field];
        if (fieldInfo) {
          if (fieldInfo.type === 'ip') {
            unmappedType = 'ip';
          } else if (fieldInfo.type === 'integer') {
            unmappedType = 'long';
          }
        }
        obj[field].unmapped_type = unmappedType;
        obj[field].missing = (parts[1] === 'asc' ? '_last' : '_first');
        query.sort.push(obj);
      }

      return;
    }

    // Old Method
    if (!info.iSortingCols || parseInt(info.iSortingCols, 10) === 0) {
      addSortDefault();
      return;
    }

    if (!query.sort) {
      query.sort = [];
    }

    const ilen = parseInt(info.iSortingCols, 10);
    for (let i = 0; i < ilen; i++) {
      if (!info['iSortCol_' + i] || !info['sSortDir_' + i] || !info['mDataProp_' + info['iSortCol_' + i]]) {
        continue;
      }

      const obj = {};
      const field = info['mDataProp_' + info['iSortCol_' + i]];
      obj[field] = { order: info['sSortDir_' + i] };
      query.sort.push(obj);

      if (field === 'firstPacket') {
        query.sort.push({ firstPacket: { order: info['sSortDir_' + i] } });
      } else if (field === 'lastPacket') {
        query.sort.push({ lastPacket: { order: info['sSortDir_' + i] } });
      }
    }
  }

  // --------------------------------------------------------------------------
  /**
   * Adds the view search expression to the elasticsearch query
   * @ignore
   * @name addViewToQuery
   * @param {object} req - the client request
   * @param {object} query - the elasticsearch query that has been partially built already by buildSessionQuery
   * @param {boolean} queryOverride=null - override the client query with overriding query
   * @returns {function} - the callback to call once the session query is built or an error occurs
   */
  static async #addViewToQuery (req, query, queryOverride = null) {
    // queryOverride can supersede req.query if specified
    const reqQuery = queryOverride || req.query;

    try {
      const roles = [...await req.user.getRoles()]; // es requires an array for terms search

      const viewQuery = { // search for the shortcut
        size: 1,
        query: {
          bool: {
            filter: [{
              bool: {
                must: [{ // needs to match the id OR name
                  bool: {
                    should: [ // match id OR name
                      { term: { _id: reqQuery.view } }, // matches the id
                      { term: { name: reqQuery.view } } // matches the name
                    ]
                  }
                }, { // AND be shared with the user via role, user, OR creator
                  bool: {
                    should: [
                      { terms: { roles } }, // shared via user role
                      { term: { users: req.user.userId } }, // shared via userId
                      { term: { user: req.user.userId } } // created by this user
                    ]
                  }
                }]
              }
            }]
          }
        }
      };

      const { body: { hits: { hits: views } } } = await Db.searchViews(viewQuery);

      if (!views.length) {
        console.log(`ERROR - User does not have permission to access this view or the view doesn't exist: ${reqQuery.view}`);
        throw 'Can\'t find view';
      }

      const view = views[0]._source;

      try {
        const viewExpression = arkimeparser.parse(view.expression);
        query.query.bool.filter.push(viewExpression);
        return;
      } catch (err) {
        console.log(`ERROR - View expression (%s) doesn't compile -`, ArkimeUtil.sanitizeStr(reqQuery.view), util.inspect(err, false, 50));
        throw err;
      }
    } catch (err) {
      console.log(`ERROR - Can't find view (%s) -`, ArkimeUtil.sanitizeStr(reqQuery.view), util.inspect(err, false, 50));
      throw err;
    }
  }

  // ----------------------------------------------------------------------------
  /* This method collapses cascading bool should/must into one. I couldn't figure
   * out how to do this in jison.
   */
  static #collapseQuery (query) {
    function newArray (items, kind) {
      let newItems = [];
      const len = items.length;

      for (let i = 0; i < len; i++) {
        if (items[i].bool && items[i].bool[kind]) {
          newItems = newItems.concat(items[i].bool[kind]);
        } else {
          newItems.push(items[i]);
        }
      }
      return newItems;
    }

    if (query?.bool?.should?.length === 1) {
      query = query.bool.should[0];
    }

    for (const prop in query) {
      if (prop === 'bool') {
        if (query.bool?.must_not?.bool?.should) {
          // must_not: { - when just NOTing one thing
          query.bool.must_not = query.bool.must_not.bool.should;
          BuildQuery.#collapseQuery(query);
        } else if (query.bool?.must_not?.length > 0) {
          // We can collapse both must_not: must_not: and must_not: should:
          const len = query.bool.must_not.length;
          const newItems = newArray(newArray(query.bool.must_not, 'must_not'), 'should');
          query.bool.must_not = newItems;
          if (newItems.length !== len) {
            BuildQuery.#collapseQuery(query);
          } else {
            BuildQuery.#collapseQuery(query.bool.must_not);
          }
        } else if (query.bool?.should?.length > 0) {
          // Collapse should: should:
          const len = query.bool.should.length;
          const newItems = newArray(query.bool.should, 'should');
          query.bool.should = newItems;
          if (newItems.length !== len) {
            BuildQuery.#collapseQuery(query);
          } else {
            BuildQuery.#collapseQuery(query.bool.should);
          }
        } else if (query.bool?.filter?.length > 0) {
          // Collapse filter: filter:
          const len = query.bool.filter.length;
          const newItems = newArray(query.bool.filter, 'filter');
          query.bool.filter = newItems;
          if (newItems.length !== len) {
            BuildQuery.#collapseQuery(query);
          } else {
            BuildQuery.#collapseQuery(query.bool.filter);
          }
        } else {
          // Just recurse
          BuildQuery.#collapseQuery(query.bool);
        }
      } else if (typeof query[prop] === 'object') {
        BuildQuery.#collapseQuery(query[prop]);
      }
    }
  }

  // ----------------------------------------------------------------------------
  /* This method fixes up parts of the query that jison builds to what ES actually
   * understands.  This includes using the collapse function and the filename mapping.
   */
  static async lookupQueryItems (query) {
    BuildQuery.#collapseQuery(query);
    if (Config.get('multiES', false)) {
      return;
    }

    let err;

    async function doProcess (obj, item) {
      // console.log("\nprocess:\n", item, obj, typeof obj[item], "\n");
      if (item === 'fileand' && typeof obj[item] === 'string') {
        const fileName = obj.fileand;
        delete obj.fileand;
        const files = await Db.fileNameToFiles(fileName);
        if (files === null || files.length === 0) {
          err = "File '" + fileName + "' not found";
        } else if (files.length > 1) {
          obj.bool = { should: [] };
          for (const file of files) {
            obj.bool.should.push({ bool: { filter: [{ term: { node: file.node } }, { term: { fileId: file.num } }] } });
          }
        } else {
          obj.bool = { filter: [{ term: { node: files[0].node } }, { term: { fileId: files[0].num } }] };
        }
      } else if (item === 'field' && obj.field === 'fileand') {
        obj.field = 'fileId';
      } else if (typeof obj[item] === 'object') {
        await convert(obj[item]);
      }
    }

    async function convert (obj) {
      for (const item in obj) {
        await doProcess(obj, item);
      }
    }

    await convert(query);

    return err;
  }

  // --------------------------------------------------------------------------
  /**
   * Builds the session query based on req.query (Promise version)
   * @ignore
   * @name buildSessionQueryPromise
   * @param {object} req - the client request
   * @param {boolean} queryOverride=null - override the client query with overriding query
   * @returns {Promise} - resolves with {query, indices}
   */
  static buildPromise (req, queryOverride = null) {
    return new Promise((resolve, reject) => {
      BuildQuery.build(req, (err, query, indices) => {
        if (err) {
          reject(err);
        } else {
          resolve({ query, indices });
        }
      }, queryOverride);
    });
  }

  // --------------------------------------------------------------------------
  /**
   * Builds the session query based on req.query
   * @ignore
   * @name buildSessionQuery
   * @param {object} req - the client request
   * @param {function} buildCb - the callback to call when building the query is complete
   * @param {boolean} queryOverride=null - override the client query with overriding query
   * @returns {function} - the callback to call once the session query is built or an error occurs
   */
  static async build (req, buildCb, queryOverride = null) {
    // validate time limit is not exceeded
    let timeLimitExceeded = false;

    // queryOverride can supercede req.query if specified
    const reqQuery = queryOverride || req.query;

    // determineQueryTimes calculates startTime, stopTime, and interval from reqQuery
    const startAndStopParams = BuildQuery.determineQueryTimes(reqQuery);
    if (startAndStopParams[0] !== undefined) {
      reqQuery.startTime = startAndStopParams[0];
    }
    if (startAndStopParams[1] !== undefined) {
      reqQuery.stopTime = startAndStopParams[1];
    }

    const interval = startAndStopParams[2];

    if ((parseFloat(reqQuery.date) > parseFloat(req.user.timeLimit)) ||
      ((reqQuery.date === '-1') && req.user.timeLimit)) {
      timeLimitExceeded = true;
    } else if ((reqQuery.startTime) && (reqQuery.stopTime) && (req.user.timeLimit) &&
               ((reqQuery.stopTime - reqQuery.startTime) / 3600 > req.user.timeLimit)) {
      timeLimitExceeded = true;
    }

    if (timeLimitExceeded) {
      console.log(`${req.user.userName} trying to exceed time limit: ${req.user.timeLimit} hours`);
      return buildCb(`User time limit (${req.user.timeLimit} hours) exceeded`, {});
    }

    const limit = Math.min(+Config.get('maxSessionsQueried', 2000000), +reqQuery.length || 100);

    const query = {
      from: reqQuery.start || 0,
      size: limit,
      timeout: internals.esQueryTimeout,
      query: { bool: { filter: [] } }
    };

    if (query.from === 0) {
      delete query.from;
    }

    if (reqQuery.strictly === 'true') {
      reqQuery.bounding = 'both';
    }

    if ((reqQuery.date && reqQuery.date === '-1') ||
        (reqQuery.segments && reqQuery.segments === 'all')) {
      // interval is already assigned above from result of determineQueryTimes

    } else if (reqQuery.startTime !== undefined && reqQuery.stopTime) {
      switch (reqQuery.bounding) {
      case 'first':
        query.query.bool.filter.push({ range: { firstPacket: { gte: reqQuery.startTime * 1000, lte: reqQuery.stopTime * 1000 } } });
        break;
      default:
      case 'last':
        query.query.bool.filter.push({ range: { lastPacket: { gte: reqQuery.startTime * 1000, lte: reqQuery.stopTime * 1000 } } });
        break;
      case 'both':
        query.query.bool.filter.push({ range: { firstPacket: { gte: reqQuery.startTime * 1000 } } });
        query.query.bool.filter.push({ range: { lastPacket: { lte: reqQuery.stopTime * 1000 } } });
        break;
      case 'either':
        query.query.bool.filter.push({ range: { firstPacket: { lte: reqQuery.stopTime * 1000 } } });
        query.query.bool.filter.push({ range: { lastPacket: { gte: reqQuery.startTime * 1000 } } });
        break;
      case 'database':
        query.query.bool.filter.push({ range: { '@timestamp': { gte: reqQuery.startTime * 1000, lte: reqQuery.stopTime * 1000 } } });
        break;
      }
    } else {
      switch (reqQuery.bounding) {
      case 'first':
        query.query.bool.filter.push({ range: { firstPacket: { gte: reqQuery.startTime * 1000 } } });
        break;
      default:
      case 'both':
      case 'last':
        query.query.bool.filter.push({ range: { lastPacket: { gte: reqQuery.startTime * 1000 } } });
        break;
      case 'either':
        query.query.bool.filter.push({ range: { firstPacket: { lte: reqQuery.stopTime * 1000 } } });
        query.query.bool.filter.push({ range: { lastPacket: { gte: reqQuery.startTime * 1000 } } });
        break;
      case 'database':
        query.query.bool.filter.push({ range: { '@timestamp': { gte: reqQuery.startTime * 1000 } } });
        break;
      }
    }

    if (reqQuery.facets === 'true' || parseInt(reqQuery.facets) === 1 || reqQuery.map === 'true' || reqQuery.map === true) {
      query.aggregations = {};

      // only add map aggregations if requested
      if (reqQuery.map === 'true' || reqQuery.map) {
        query.aggregations = {
          mapG1: { terms: { field: 'source.geo.country_iso_code', size: 1000, min_doc_count: 1 } },
          mapG2: { terms: { field: 'destination.geo.country_iso_code', size: 1000, min_doc_count: 1 } },
          mapG3: { terms: { field: 'http.xffGEO', size: 1000, min_doc_count: 1 } }
        };
      }

      // add the dbHisto aggregation for timeline data if requested
      if (reqQuery.facets === 'true' || parseInt(reqQuery.facets) === 1) {
        query.aggregations.dbHisto = { aggregations: {} };

        const filters = req.user.settings.timelineDataFilters || internals.settingDefaults.timelineDataFilters;
        for (let i = 0; i < filters.length; i++) {
          const filter = filters[i];

          // Will also grab src/dst of these options instead to show on the timeline
          switch (filter) {
          case 'network.packets':
          case 'totPackets':
            query.aggregations.dbHisto.aggregations['source.packets'] = { sum: { field: 'source.packets' } };
            query.aggregations.dbHisto.aggregations['destination.packets'] = { sum: { field: 'destination.packets' } };
            break;
          case 'network.bytes':
          case 'totBytes':
            query.aggregations.dbHisto.aggregations['source.bytes'] = { sum: { field: 'source.bytes' } };
            query.aggregations.dbHisto.aggregations['destination.bytes'] = { sum: { field: 'destination.bytes' } };
            break;
          case 'totDataBytes':
            query.aggregations.dbHisto.aggregations['client.bytes'] = { sum: { field: 'client.bytes' } };
            query.aggregations.dbHisto.aggregations['server.bytes'] = { sum: { field: 'server.bytes' } };
            break;
          default:
            query.aggregations.dbHisto.aggregations[filter] = { sum: { field: filter } };
          }
        }

        switch (reqQuery.bounding) {
        case 'first':
          query.aggregations.dbHisto.histogram = { field: 'firstPacket', interval: interval * 1000, min_doc_count: 1 };
          break;
        case 'database':
          query.aggregations.dbHisto.histogram = { field: '@timestamp', interval: interval * 1000, min_doc_count: 1 };
          break;
        default:
          query.aggregations.dbHisto.histogram = { field: 'lastPacket', interval: interval * 1000, min_doc_count: 1 };
          break;
        }

        if (reqQuery.spanning === 'true' || reqQuery.spanning === true) {
          query.aggregations.dbHisto.histogram = { field: 'packetRange', interval: interval * 1000, min_doc_count: 1 };
        }
      }
    }

    BuildQuery.#addSortToQuery(query, reqQuery, 'firstPacket');

    let shortcuts;
    try { // try to fetch shortcuts
      shortcuts = await Db.getShortcutsCache(req.user);
    } catch (err) { // don't need to do anything, there will just be no
      // shortcuts sent to the parser. but still log the error.
      console.log('ERROR - fetching shortcuts cache when building sessions query', util.inspect(err, false, 50));
    }

    // always complete building the query regardless of shortcuts
    let err;
    arkimeparser.parser.yy = {
      views: req.user.views,
      fieldsMap: Config.getFieldsMap(),
      dbFieldsMap: Config.getDBFieldsMap(),
      prefix: internals.prefix,
      emailSearch: req.user.emailSearch === true,
      shortcuts: shortcuts || {},
      shortcutTypeMap: internals.shortcutTypeMap
    };

    if (reqQuery.expression) {
      if (!ArkimeUtil.isString(reqQuery.expression)) {
        err = 'Expression needs to be a string';
      } else {
        // reqQuery.expression = reqQuery.expression.replace(/\\/g, "\\\\");
        try {
          query.query.bool.filter.push(arkimeparser.parse(reqQuery.expression));
        } catch (e) {
          err = e;
        }
      }
    }

    if (!err && reqQuery.view) {
      try {
        await BuildQuery.#addViewToQuery(req, query, queryOverride);
      } catch (e) {
        err = e;
      }
    }

    if (!err && req.user.getExpression()) {
      try {
        // Expression was set by admin, so assume email search ok
        arkimeparser.parser.yy.emailSearch = true;
        const userExpression = arkimeparser.parse(req.user.getExpression());
        query.query.bool.filter.push(userExpression);
      } catch (e) {
        console.log(`ERROR - Forced expression (${req.user.getExpression()}) doesn't compile -`, e);
        err = e;
      }
    }

    const lerr = await BuildQuery.lookupQueryItems(query.query.bool.filter);
    req._arkimeESQuery = JSON.stringify(query);

    if (reqQuery.date === '-1' || // An all query
        Config.get('queryAllIndices', Config.get('multiES', false))) { // queryAllIndices (default: multiES)
      req._arkimeESQueryIndices = Db.fixIndex(Db.getSessionIndices());
      return buildCb(err ?? lerr, query, req._arkimeESQueryIndices); // Then we just go against all indices for a slight overhead
    }

    const indices = await Db.getIndices(reqQuery.startTime, reqQuery.stopTime, reqQuery.bounding, Config.get('rotateIndex', 'daily'), Config.getArray('queryExtraIndices', ''));
    if (indices.length > 3000) { // Will url be too long
      req._arkimeESQueryIndices = Db.fixIndex(Db.getSessionIndices());
    } else {
      req._arkimeESQueryIndices = indices;
    }

    return buildCb(err ?? lerr, query, req._arkimeESQueryIndices);
  }

  // -------------------------------------------------------------------------
  // determineQueryTimes(reqQuery)
  //
  // Returns [startTimeSec, stopTimeSec, interval] using values from
  // reqQuery.date, reqQuery.startTime, reqQuery.stopTime, reqQuery.interval,
  // and reqQuery.segments.
  //
  // This code was factored out from buildSessionQuery.
  // -------------------------------------------------------------------------
  static determineQueryTimes (reqQuery) {
    let startTimeSec = null;
    let stopTimeSec = null;
    let interval;

    if (Config.debug) {
      console.log('determineQueryTimes <-', reqQuery);
    }

    if (reqQuery.date && isNaN(parseFloat(reqQuery.date))) {
      reqQuery.date = '-1';
    }

    function toInterval (diff) {
      if (diff < 1000) {
        return 1; // second
      } else if (diff <= 60 * 10000) {
        return 60; // minute
      } else if (diff <= 60 * 60 * 10000) {
        return 60 * 60; // hour
      } else {
        return 24 * 60 * 60; // day
      }
    }

    if ((reqQuery.date && parseFloat(reqQuery.date) === -1) ||
        (reqQuery.segments && reqQuery.segments === 'all')) {
      if (reqQuery.spanning === 'true' || reqQuery.spanning === true) {
        interval = 60 * 60 * 24; // Day to be safe
      } else {
        interval = 60 * 60; // Hour to be safe
      }
    } else if ((reqQuery.startTime !== undefined) && (reqQuery.stopTime !== undefined)) {
      if (!/^-?[0-9]+$/.test(reqQuery.startTime)) {
        startTimeSec = Date.parse(reqQuery.startTime.replace('+', ' ')) / 1000;
      } else {
        startTimeSec = parseInt(reqQuery.startTime, 10);
      }

      if (!/^-?[0-9]+$/.test(reqQuery.stopTime)) {
        stopTimeSec = Date.parse(reqQuery.stopTime.replace('+', ' ')) / 1000;
      } else {
        stopTimeSec = parseInt(reqQuery.stopTime, 10);
      }

      interval = toInterval(stopTimeSec - startTimeSec);
    } else {
      const queryDate = reqQuery.date || 1;
      startTimeSec = (Math.floor(Date.now() / 1000) - 60 * 60 * parseFloat(queryDate));
      stopTimeSec = Date.now() / 1000;

      interval = toInterval(60 * parseFloat(queryDate));
    }

    switch (reqQuery.interval) {
    case 'second':
      interval = 1;
      break;
    case 'minute':
      interval = 60;
      break;
    case 'hour':
      interval = 60 * 60;
      break;
    case 'day':
      interval = 60 * 60 * 24;
      break;
    case 'week':
      interval = 60 * 60 * 24 * 7;
      break;
    }

    if (Config.debug) {
      console.log('determineQueryTimes ->', 'startTimeSec', startTimeSec, 'stopTimeSec', stopTimeSec, 'interval', interval);
    }

    return [startTimeSec, Math.floor(stopTimeSec), interval];
  }
}

module.exports = BuildQuery;
