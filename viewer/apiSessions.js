'use strict';

module.exports = (async, util, Db, Config, ViewerUtils, molochparser, internals) => {
  let module = {};

  // HELPERS ----------------------------------------------------------------- //
  /**
   * Adds the sort options to the elasticsearch query
   * @ignore
   * @name addSortToQuery
   * @param {object} query - the elasticsearch query that has been partially built already by buildSessionQuery
   * @param {object} info - the query params from the client
   * @param {string} defaultSort - the default sort
   */
  function addSortToQuery (query, info, defaultSort) {
    function addSortDefault () {
      if (defaultSort) {
        if (!query.sort) {
          query.sort = [];
        }
        let obj = {};
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

      info.order.split(',').forEach(function (item) {
        const parts = item.split(':');
        const field = parts[0];

        let obj = {};
        if (field === 'firstPacket') {
          obj.firstPacket = { order: parts[1] };
        } else if (field === 'lastPacket') {
          obj.lastPacket = { order: parts[1] };
        } else {
          obj[field] = { order: parts[1] };
        }

        obj[field].unmapped_type = 'string';
        const fieldInfo = Config.getDBFieldsMap()[field];
        if (fieldInfo) {
          if (fieldInfo.type === 'ip') {
            obj[field].unmapped_type = 'ip';
          } else if (fieldInfo.type === 'integer') {
            obj[field].unmapped_type = 'long';
          }
        }
        obj[field].missing = (parts[1] === 'asc' ? '_last' : '_first');
        query.sort.push(obj);
      });

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

    for (let i = 0, ilen = parseInt(info.iSortingCols, 10); i < ilen; i++) {
      if (!info['iSortCol_' + i] || !info['sSortDir_' + i] || !info['mDataProp_' + info['iSortCol_' + i]]) {
        continue;
      }

      let obj = {};
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

  /**
   * Adds the view search expression to the elasticsearch query
   * @ignore
   * @name addViewToQuery
   * @param {object} req - the client request
   * @param {object} query - the elasticsearch query that has been partially built already by buildSessionQuery
   * @param {function} continueBuildQueryCb - the callback to call when adding the view is complete
   * @param {function} finalCb - the callback to pass to continueBuildQueryCb that is called when building the sessions query is complete
   * @param {boolean} queryOverride=null - override the client query with overriding query
   * @returns {function} - the callback to call once the session query is built or an error occurs
   */
  function addViewToQuery (req, query, continueBuildQueryCb, finalCb, queryOverride = null) {
    let err;
    let viewExpression;

    // queryOverride can supercede req.body if specified
    let reqQuery = queryOverride || req.body;

    if (req.user.views && req.user.views[reqQuery.view]) { // it's a user's view
      try {
        viewExpression = molochparser.parse(req.user.views[reqQuery.view].expression);
        query.query.bool.filter.push(viewExpression);
      } catch (e) {
        console.log(`ERROR - User expression (${reqQuery.view}) doesn't compile -`, e);
        err = e;
      }
      return continueBuildQueryCb(req, query, err, finalCb, queryOverride);
    } else { // it's a shared view
      Db.getUser('_moloch_shared', (err, sharedUser) => {
        if (sharedUser && sharedUser.found) {
          sharedUser = sharedUser._source;
          sharedUser.views = sharedUser.views || {};
          for (let viewName in sharedUser.views) {
            if (viewName === reqQuery.view) {
              viewExpression = sharedUser.views[viewName].expression;
              break;
            }
          }
          if (sharedUser.views[reqQuery.view]) {
            try {
              viewExpression = molochparser.parse(sharedUser.views[reqQuery.view].expression);
              query.query.bool.filter.push(viewExpression);
            } catch (e) {
              console.log(`ERROR - Shared user expression (${reqQuery.view}) doesn't compile -`, e);
              err = e;
            }
          }
          return continueBuildQueryCb(req, query, err, finalCb, queryOverride);
        }
      });
    }
  }

  /**
   * Builds the session query based on req.body
   * @ignore
   * @name buildSessionQuery
   * @param {object} req - the client request
   * @param {function} buildCb - the callback to call when building the query is complete
   * @param {boolean} queryOverride=null - override the client query with overriding query
   * @returns {function} - the callback to call once the session query is built or an error occurs
   */
  module.buildSessionQuery = (req, buildCb, queryOverride = null) => {
    // validate time limit is not exceeded
    let timeLimitExceeded = false;
    let interval;

    // queryOverride can supercede req.body if specified
    let reqQuery = queryOverride || req.body;

    // determineQueryTimes calculates startTime, stopTime, and interval from reqQuery
    let startAndStopParams = ViewerUtils.determineQueryTimes(reqQuery);
    if (startAndStopParams[0] !== undefined) {
      reqQuery.startTime = startAndStopParams[0];
    }
    if (startAndStopParams[1] !== undefined) {
      reqQuery.stopTime = startAndStopParams[1];
    }
    interval = startAndStopParams[2];

    if ((parseInt(reqQuery.date) > parseInt(req.user.timeLimit)) ||
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

    let limit = Math.min(2000000, +reqQuery.length || 100);

    let query = { from: reqQuery.start || 0,
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
        query.query.bool.filter.push({ range: { timestamp: { gte: reqQuery.startTime * 1000, lte: reqQuery.stopTime * 1000 } } });
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
        query.query.bool.filter.push({ range: { timestamp: { gte: reqQuery.startTime * 1000 } } });
        break;
      }
    }

    if (parseInt(reqQuery.facets) === 1) {
      query.aggregations = {};
      // only add map aggregations if requested
      if (reqQuery.map === 'true') {
        query.aggregations = {
          mapG1: { terms: { field: 'srcGEO', size: 1000, min_doc_count: 1 } },
          mapG2: { terms: { field: 'dstGEO', size: 1000, min_doc_count: 1 } },
          mapG3: { terms: { field: 'http.xffGEO', size: 1000, min_doc_count: 1 } }
        };
      }

      query.aggregations.dbHisto = { aggregations: {} };

      let filters = req.user.settings.timelineDataFilters || internals.settingDefaults.timelineDataFilters;
      for (let i = 0; i < filters.length; i++) {
        let filter = filters[i];

        // Will also grap src/dst of these options instead to show on the timeline
        if (filter === 'totPackets') {
          query.aggregations.dbHisto.aggregations.srcPackets = { sum: { field: 'srcPackets' } };
          query.aggregations.dbHisto.aggregations.dstPackets = { sum: { field: 'dstPackets' } };
        } else if (filter === 'totBytes') {
          query.aggregations.dbHisto.aggregations.srcBytes = { sum: { field: 'srcBytes' } };
          query.aggregations.dbHisto.aggregations.dstBytes = { sum: { field: 'dstBytes' } };
        } else if (filter === 'totDataBytes') {
          query.aggregations.dbHisto.aggregations.srcDataBytes = { sum: { field: 'srcDataBytes' } };
          query.aggregations.dbHisto.aggregations.dstDataBytes = { sum: { field: 'dstDataBytes' } };
        } else {
          query.aggregations.dbHisto.aggregations[filter] = { sum: { field: filter } };
        }
      }

      switch (reqQuery.bounding) {
      case 'first':
        query.aggregations.dbHisto.histogram = { field: 'firstPacket', interval: interval * 1000, min_doc_count: 1 };
        break;
      case 'database':
        query.aggregations.dbHisto.histogram = { field: 'timestamp', interval: interval * 1000, min_doc_count: 1 };
        break;
      default:
        query.aggregations.dbHisto.histogram = { field: 'lastPacket', interval: interval * 1000, min_doc_count: 1 };
        break;
      }
    }

    addSortToQuery(query, reqQuery, 'firstPacket');

    let err = null;

    molochparser.parser.yy = {
      views: req.user.views,
      fieldsMap: Config.getFieldsMap(),
      dbFieldsMap: Config.getDBFieldsMap(),
      prefix: internals.prefix,
      emailSearch: req.user.emailSearch === true,
      lookups: req.lookups,
      lookupTypeMap: internals.lookupTypeMap
    };

    if (reqQuery.expression) {
      // reqQuery.expression = reqQuery.expression.replace(/\\/g, "\\\\");
      try {
        query.query.bool.filter.push(molochparser.parse(reqQuery.expression));
      } catch (e) {
        err = e;
      }
    }

    if (!err && reqQuery.view) {
      addViewToQuery(req, query, ViewerUtils.continueBuildQuery, buildCb, queryOverride);
    } else {
      ViewerUtils.continueBuildQuery(req, query, err, buildCb, queryOverride);
    }
  }

  // APIs -------------------------------------------------------------------- //
  /**
   * POST/GET (preferred method is POST)
   *
   * Builds an elasticsearch session query. Gets a list of sessions and returns them to the client.
   * @name /api/sessions
   * @param {number} date=1	- The number of hours of data to return (-1 means all data). Defaults to 1
   * @param {string} expression - The search expression string
   * @param {number} facets=0 - 1 = include the aggregation information for maps and timeline graphs. Defaults to 0
   * @param {number} length=100 - The number of items to return. Defaults to 100, Max is 2,000,000
   * @param {number} start=0	- The entry to start at. Defaults to 0
   * @param {number} startTime - If the date parameter is not set, this is the start time of data to return. Format is seconds since Unix EPOC.
   * @param {number} stopTimeI’m  - If the date parameter is not set, this is the stop time of data to return. Format is seconds since Unix EPOC.
   * @param {string} view -	The view name to apply before the expression.
   * @param {string} order - Comma separated list of db field names to sort on. Data is sorted in order of the list supplied. Optionally can be followed by :asc or :desc for ascending or descending sorting.
   * @param {string} fields - Comma separated list of db field names to return.
     Default is ipProtocol,rootId,totDataBytes,srcDataBytes,dstDataBytes,firstPacket,lastPacket,srcIp,srcPort,dstIp,dstPort,totPackets,srcPackets,dstPackets,totBytes,srcBytes,dstBytes,node,http.uri,srcGEO,dstGEO,email.subject,email.src,email.dst,email.filename,dns.host,cert,irc.channel
   * @param {string} bounding=last - Query sessions based on different aspects of a session's time. Options include:
     'first' - First Packet: the timestamp of the first packet received for the session.
     'last' - Last Packet: The timestamp of the last packet received for the session.
     'both' - Bounded: Both the first and last packet timestamps for the session must be inside the time window.
     'either' - Session Overlaps: The timestamp of the first packet must be before the end of the time window AND the timestamp of the last packet must be after the start of the time window.
     'database' - Database: The timestamp the session was written to the database. This can be up to several minutes AFTER the last packet was received.
   * @param {boolean} strictly=false - When set the entire session must be inside the date range to be observed, otherwise if it overlaps it is displayed. Overwrites the bounding parameter, sets bonding to 'both'
   * @returns {object} Sends the response to the client
   */
  module.getSessions = (req, res) => {
    let map = {};
    let graph = {};

    let options;
    if (req.body.cancelId) {
      options = {
        cancelId: `${req.user.userId}::${req.body.cancelId}`
      };
    }

    let response = {
      data: [],
      map: {},
      graph: {},
      recordsTotal: 0,
      recordsFiltered: 0,
      health: Db.healthCache()
    };

    module.buildSessionQuery(req, (bsqErr, query, indices) => {
      if (bsqErr) {
        response.error = bsqErr.toString();
        return res.send(response);
      }

      let addMissing = false;
      if (req.body.fields) {
        query._source = ViewerUtils.queryValueToArray(req.body.fields);
        ['node', 'srcIp', 'srcPort', 'dstIp', 'dstPort'].forEach((item) => {
          if (query._source.indexOf(item) === -1) {
            query._source.push(item);
          }
        });
      } else {
        addMissing = true;
        query._source = [
          'ipProtocol', 'rootId', 'totDataBytes', 'srcDataBytes',
          'dstDataBytes', 'firstPacket', 'lastPacket', 'srcIp', 'srcPort',
          'dstIp', 'dstPort', 'totPackets', 'srcPackets', 'dstPackets',
          'totBytes', 'srcBytes', 'dstBytes', 'node', 'http.uri', 'srcGEO',
          'dstGEO', 'email.subject', 'email.src', 'email.dst', 'email.filename',
          'dns.host', 'cert', 'irc.channel', 'http.xffGEO'
        ];
      }

      if (query.aggregations && query.aggregations.dbHisto) {
        graph.interval = query.aggregations.dbHisto.histogram.interval;
      }

      if (Config.debug) {
        console.log(`sessions.json ${indices} query`, JSON.stringify(query, null, 1));
      }

      Promise.all([
        Db.searchPrimary(indices, 'session', query, options),
        Db.numberOfDocuments('sessions2-*'),
        Db.healthCachePromise()
      ]).then(([sessions, total, health]) => {
        if (Config.debug) {
          console.log('sessions.json result', util.inspect(sessions, false, 50));
        }

        if (sessions.error) { throw sessions.err; }

        map = ViewerUtils.mapMerge(sessions.aggregations);
        graph = ViewerUtils.graphMerge(req, query, sessions.aggregations);

        let results = { total: sessions.hits.total, results: [] };
        async.each(sessions.hits.hits, (hit, hitCb) => {
          let fields = hit._source || hit.fields;
          if (fields === undefined) {
            return hitCb(null);
          }

          fields.id = Db.session2Sid(hit);

          if (parseInt(req.body.flatten) === 1) {
            fields = ViewerUtils.flattenFields(fields);
          }

          if (addMissing) {
            ['srcPackets', 'dstPackets', 'srcBytes', 'dstBytes', 'srcDataBytes', 'dstDataBytes'].forEach((item) => {
              if (fields[item] === undefined) {
                fields[item] = -1;
              }
            });
            results.results.push(fields);
            return hitCb();
          } else {
            ViewerUtils.fixFields(fields, function () {
              results.results.push(fields);
              return hitCb();
            });
          }
        }, function () {
          try {
            response.map = map;
            response.graph = graph;
            response.health = health;
            response.data = (results ? results.results : []);
            response.recordsTotal = total.count;
            response.recordsFiltered = (results ? results.total : 0);
            res.logCounts(response.data.length, response.recordsFiltered, response.recordsTotal);
            return res.send(response);
          } catch (e) {
            console.trace('fetch sessions error', e.stack);
            response.error = e.toString();
            return res.send(response);
          }
        });
      }).catch((err) => {
        console.log('ERROR - sessions error', err);
        response.error = err.toString();
        return res.send(response);
      });
    });
  }

  return module;
}
