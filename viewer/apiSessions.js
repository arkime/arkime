'use strict';

module.exports = (async, util, Pcap, Db, Config, ViewerUtils, molochparser, internals) => {
  let module = {};

  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------
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

  function csvListWriter (req, res, list, fields, pcapWriter, extension) {
    if (list.length > 0 && list[0].fields) {
      list = list.sort(function (a, b) { return a.fields.lastPacket - b.fields.lastPacket; });
    } else if (list.length > 0 && list[0]._source) {
      list = list.sort(function (a, b) { return a._source.lastPacket - b._source.lastPacket; });
    }

    let fieldObjects = Config.getDBFieldsMap();

    if (fields) {
      let columnHeaders = [];
      for (let i = 0, ilen = fields.length; i < ilen; ++i) {
        if (fieldObjects[fields[i]] !== undefined) {
          columnHeaders.push(fieldObjects[fields[i]].friendlyName);
        }
      }
      res.write(columnHeaders.join(', '));
      res.write('\r\n');
    }

    for (let j = 0, jlen = list.length; j < jlen; j++) {
      let sessionData = ViewerUtils.flattenFields(list[j]._source || list[j].fields);
      sessionData._id = list[j]._id;

      if (!fields) { continue; }

      let values = [];
      for (let k = 0, klen = fields.length; k < klen; ++k) {
        let value = sessionData[fields[k]];
        if (fields[k] === 'ipProtocol' && value) {
          value = Pcap.protocol2Name(value);
        }

        if (Array.isArray(value)) {
          let singleValue = '"' + value.join(', ') + '"';
          values.push(singleValue);
        } else {
          if (value === undefined) {
            value = '';
          } else if (typeof (value) === 'string' && value.includes(',')) {
            if (value.includes('"')) {
              value = value.replace(/"/g, '""');
            }
            value = '"' + value + '"';
          }
          values.push(value);
        }
      }

      res.write(values.join(','));
      res.write('\r\n');
    }

    res.end();
  }

  function sessionsListAddSegments (req, indices, query, list, cb) {
    let processedRo = {};

    // Index all the ids we have, so we don't include them again
    let haveIds = {};
    list.forEach(function (item) {
      haveIds[item._id] = true;
    });

    delete query.aggregations;

    // Do a ro search on each item
    let writes = 0;
    async.eachLimit(list, 10, function (item, nextCb) {
      let fields = item._source || item.fields;
      if (!fields.rootId || processedRo[fields.rootId]) {
        if (writes++ > 100) {
          writes = 0;
          setImmediate(nextCb);
        } else {
          nextCb();
        }
        return;
      }
      processedRo[fields.rootId] = true;

      query.query.bool.filter.push({ term: { rootId: fields.rootId } });
      Db.searchPrimary(indices, 'session', query, null, function (err, result) {
        if (err || result === undefined || result.hits === undefined || result.hits.hits === undefined) {
          console.log('ERROR fetching matching sessions', err, result);
          return nextCb(null);
        }
        result.hits.hits.forEach(function (item) {
          if (!haveIds[item._id]) {
            haveIds[item._id] = true;
            list.push(item);
          }
        });
        return nextCb(null);
      });
      query.query.bool.filter.pop();
    }, function (err) {
      cb(err, list);
    });
  }

  module.sessionsListFromQuery = (req, res, fields, cb) => {
    if (req.query.segments && req.query.segments.match(/^(time|all)$/) && fields.indexOf('rootId') === -1) {
      fields.push('rootId');
    }

    module.buildSessionQuery(req, (err, query, indices) => {
      if (err) {
        return res.send('Could not build query.  Err: ' + err);
      }
      query._source = fields;
      if (Config.debug) {
        console.log('sessionsListFromQuery query', JSON.stringify(query, null, 1));
      }
      Db.searchPrimary(indices, 'session', query, null, function (err, result) {
        if (err || result.error) {
          console.log('ERROR - Could not fetch list of sessions.  Err: ', err, ' Result: ', result, 'query:', query);
          return res.send('Could not fetch list of sessions.  Err: ' + err + ' Result: ' + result);
        }
        let list = result.hits.hits;
        if (req.query.segments && req.query.segments.match(/^(time|all)$/)) {
          sessionsListAddSegments(req, indices, query, list, function (err, list) {
            cb(err, list);
          });
        } else {
          cb(err, list);
        }
      });
    });
  };

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
  };

  module.sessionsListFromIds = (req, ids, fields, cb) => {
    let processSegments = false;
    if (req && ((req.query.segments && req.query.segments.match(/^(time|all)$/)) || (req.body.segments && req.body.segments.match(/^(time|all)$/)))) {
      if (fields.indexOf('rootId') === -1) { fields.push('rootId'); }
      processSegments = true;
    }

    let list = [];
    const nonArrayFields = ['ipProtocol', 'firstPacket', 'lastPacket', 'srcIp', 'srcPort', 'srcGEO', 'dstIp', 'dstPort', 'dstGEO', 'totBytes', 'totDataBytes', 'totPackets', 'node', 'rootId', 'http.xffGEO'];
    let fixFields = nonArrayFields.filter(function (x) { return fields.indexOf(x) !== -1; });

    async.eachLimit(ids, 10, function (id, nextCb) {
      Db.getSession(id, { _source: fields.join(',') }, function (err, session) {
        if (err) {
          return nextCb(null);
        }

        for (let i = 0; i < fixFields.length; i++) {
          let field = fixFields[i];
          if (session._source[field] && Array.isArray(session._source[field])) {
            session._source[field] = session._source[field][0];
          }
        }

        list.push(session);
        nextCb(null);
      });
    }, function (err) {
      if (processSegments) {
        module.buildSessionQuery(req, (err, query, indices) => {
          query._source = fields;
          sessionsListAddSegments(req, indices, query, list, function (err, list) {
            cb(err, list);
          });
        });
      } else {
        cb(err, list);
      }
    });
  };

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  /**
   * POST/GET (preferred method is POST)
   *
   * Builds an elasticsearch session query and returns the query and the elasticsearch indices to the client.
   * @name /api/buildQuery
   * @param {number} date=1 - The number of hours of data to return (-1 means all data). Defaults to 1
   * @param {string} expression - The search expression string
   * @param {number} facets=0 - 1 = include the aggregation information for maps and timeline graphs. Defaults to 0
   * @param {number} length=100 - The number of items to return. Defaults to 100, Max is 2,000,000
   * @param {number} start=0 - The entry to start at. Defaults to 0
   * @param {number} startTime - If the date parameter is not set, this is the start time of data to return. Format is seconds since Unix EPOC.
   * @param {number} stopTime  - If the date parameter is not set, this is the stop time of data to return. Format is seconds since Unix EPOC.
   * @param {string} view - The view name to apply before the expression.
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
  module.getQuery = (req, res) => {
    module.buildSessionQuery(req, (bsqErr, query, indices) => {
      if (bsqErr) {
        return res.send({
          recordsTotal: 0,
          recordsFiltered: 0,
          error: bsqErr.toString()
        });
      }

      if (req.body.fields) {
        query._source = ViewerUtils.queryValueToArray(req.body.fields);
      }

      res.send({ 'esquery': query, 'indices': indices });
    });
  };

  /**
   * POST/GET (preferred method is POST)
   *
   * Builds an elasticsearch session query. Gets a list of sessions and returns them to the client.
   * @name /api/sessions
   * @param {number} date=1 - The number of hours of data to return (-1 means all data). Defaults to 1
   * @param {string} expression - The search expression string
   * @param {number} facets=0 - 1 = include the aggregation information for maps and timeline graphs. Defaults to 0
   * @param {number} length=100 - The number of items to return. Defaults to 100, Max is 2,000,000
   * @param {number} start=0 - The entry to start at. Defaults to 0
   * @param {number} startTime - If the date parameter is not set, this is the start time of data to return. Format is seconds since Unix EPOC.
   * @param {number} stopTime  - If the date parameter is not set, this is the stop time of data to return. Format is seconds since Unix EPOC.
   * @param {string} view - The view name to apply before the expression.
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
  };

  /**
   * POST/GET (preferred method is POST)
   *
   * Builds an elasticsearch session query. Gets a list of sessions and returns them as CSV to the client.
   * @name /api/sessions
   * @param {number} date=1 - The number of hours of data to return (-1 means all data). Defaults to 1
   * @param {string} expression - The search expression string
   * @param {number} facets=0 - 1 = include the aggregation information for maps and timeline graphs. Defaults to 0
   * @param {number} length=100 - The number of items to return. Defaults to 100, Max is 2,000,000
   * @param {number} start=0 - The entry to start at. Defaults to 0
   * @param {number} startTime - If the date parameter is not set, this is the start time of data to return. Format is seconds since Unix EPOC.
   * @param {number} stopTime  - If the date parameter is not set, this is the stop time of data to return. Format is seconds since Unix EPOC.
   * @param {string} view - The view name to apply before the expression.
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
  module.getSessionsCSV = (req, res) => {
    ViewerUtils.noCache(req, res, 'text/csv');

    // default fields to display in csv
    let fields = [
      'ipProtocol', 'firstPacket', 'lastPacket', 'srcIp', 'srcPort', 'srcGEO',
      'dstIp', 'dstPort', 'dstGEO', 'totBytes', 'totDataBytes', 'totPackets', 'node'
    ];

    // save requested fields because sessionsListFromQuery returns fields with
    // "rootId" appended onto the end
    let reqFields = fields;

    if (req.body.fields) {
      fields = reqFields = ViewerUtils.queryValueToArray(req.body.fields);
    }

    if (req.body.ids) {
      const ids = ViewerUtils.queryValueToArray(req.body.ids);
      module.sessionsListFromIds(req, ids, fields, (err, list) => {
        csvListWriter(req, res, list, reqFields);
      });
    } else {
      module.sessionsListFromQuery(req, res, fields, (err, list) => {
        csvListWriter(req, res, list, reqFields);
      });
    }
  };

  /**
   * POST/GET (preferred method is POST)
   *
   * Builds an elasticsearch session query. Gets a list of field values with counts and returns them to the client.
   * @name /api/spiview
   * @param {number} date=1 - The number of hours of data to return (-1 means all data). Defaults to 1
   * @param {string} expression - The search expression string
   * @param {number} facets=0 - 1 = include the aggregation information for maps and timeline graphs. Defaults to 0
   * @param {number} startTime - If the date parameter is not set, this is the start time of data to return. Format is seconds since Unix EPOC.
   * @param {number} stopTime - If the date parameter is not set, this is the stop time of data to return. Format is seconds since Unix EPOC.
   * @param {string} view - The view name to apply before the expression.
   * @param {string} spi - Comma separated list of db fields to return. Optionally can be followed by :{count} to specify the number of values returned for the field (defaults to 100).
   * @param {string} bounding=last - Query sessions based on different aspects of a session's time. Options include:
     'first' - First Packet: the timestamp of the first packet received for the session.
     'last' - Last Packet: The timestamp of the last packet received for the session.
     'both' - Bounded: Both the first and last packet timestamps for the session must be inside the time window.
     'either' - Session Overlaps: The timestamp of the first packet must be before the end of the time window AND the timestamp of the last packet must be after the start of the time window.
     'database' - Database: The timestamp the session was written to the database. This can be up to several minutes AFTER the last packet was received.
   * @param {boolean} strictly=false - When set the entire session must be inside the date range to be observed, otherwise if it overlaps it is displayed. Overwrites the bounding parameter, sets bonding to 'both'
   * @returns {object} Sends the response to the client
   */
  module.getSPIView = (req, res) => {
    if (req.body.spi === undefined) {
      return res.send({ spi: {}, recordsTotal: 0, recordsFiltered: 0 });
    }

    const spiDataMaxIndices = +Config.get('spiDataMaxIndices', 4);

    if (parseInt(req.body.date) === -1 && spiDataMaxIndices !== -1) {
      return res.send({ spi: {}, bsqErr: "'All' date range not allowed for spiview query" });
    }

    let response = {
      spi: {},
      health: Db.healthCache()
    };

    module.buildSessionQuery(req, (bsqErr, query, indices) => {
      if (bsqErr) {
        response.error = bsqErr.toString();
        return res.send(response);
      }

      delete query.sort;

      if (!query.aggregations) {
        query.aggregations = {};
      }

      if (parseInt(req.body.facets) === 1) {
        query.aggregations.protocols = {
          terms: { field: 'protocol', size: 1000 }
        };
      }

      ViewerUtils.queryValueToArray(req.body.spi).forEach(function (item) {
        const parts = item.split(':');
        if (parts[0] === 'fileand') {
          query.aggregations[parts[0]] = { terms: { field: 'node', size: 1000 }, aggregations: { fileId: { terms: { field: 'fileId', size: parts.length > 1 ? parseInt(parts[1], 10) : 10 } } } };
        } else {
          query.aggregations[parts[0]] = { terms: { field: parts[0] } };

          if (parts.length > 1) {
            query.aggregations[parts[0]].terms.size = parseInt(parts[1], 10);
          }
        }
      });

      query.size = 0;

      if (Config.debug) {
        console.log('spiview.json query', JSON.stringify(query), 'indices', indices);
      }

      let graph, map;

      const indicesa = indices.split(',');
      if (spiDataMaxIndices !== -1 && indicesa.length > spiDataMaxIndices) {
        bsqErr = 'To save ES from blowing up, reducing number of spi data indices searched from ' + indicesa.length + ' to ' + spiDataMaxIndices + '.  This can be increased by setting spiDataMaxIndices in the config file.  Indices being searched: ';
        indices = indicesa.slice(-spiDataMaxIndices).join(',');
        bsqErr += indices;
      }

      let protocols;
      let recordsFiltered = 0;

      Promise.all([Db.searchPrimary(indices, 'session', query, null),
        Db.numberOfDocuments('sessions2-*'),
        Db.healthCachePromise()
      ]).then(([sessions, total, health]) => {
        if (Config.debug) {
          console.log('spiview.json result', util.inspect(sessions, false, 50));
        }

        if (sessions.error) {
          bsqErr = ViewerUtils.errorString(null, sessions);
          console.log('spiview.json ERROR', (sessions ? sessions.error : null));
          sendResult();
          return;
        }

        recordsFiltered = sessions.hits.total;

        if (!sessions.aggregations) {
          sessions.aggregations = {};
          for (let spi in query.aggregations) {
            sessions.aggregations[spi] = { sum_other_doc_count: 0, buckets: [] };
          }
        }

        if (sessions.aggregations.ipProtocol) {
          sessions.aggregations.ipProtocol.buckets.forEach(function (item) {
            item.key = Pcap.protocol2Name(item.key);
          });
        }

        if (parseInt(req.body.facets) === 1) {
          protocols = {};
          map = ViewerUtils.mapMerge(sessions.aggregations);
          graph = ViewerUtils.graphMerge(req, query, sessions.aggregations);
          sessions.aggregations.protocols.buckets.forEach(function (item) {
            protocols[item.key] = item.doc_count;
          });

          delete sessions.aggregations.mapG1;
          delete sessions.aggregations.mapG2;
          delete sessions.aggregations.mapG3;
          delete sessions.aggregations.dbHisto;
          delete sessions.aggregations.byHisto;
          delete sessions.aggregations.protocols;
        }

        function sendResult () {
          try {
            response.map = map;
            response.graph = graph;
            response.error = bsqErr;
            response.health = health;
            response.protocols = protocols;
            response.recordsTotal = total.count;
            response.spi = sessions.aggregations;
            response.recordsFiltered = recordsFiltered;
            res.logCounts(response.spi.count, response.recordsFiltered, response.total);
            return res.send(response);
          } catch (e) {
            console.trace('fetch spiview error', e.stack);
            response.error = e.toString();
            return res.send(response);
          }
        }

        if (!sessions.aggregations.fileand) {
          return sendResult();
        }

        let sodc = 0;
        let nresults = [];
        async.each(sessions.aggregations.fileand.buckets, function (nobucket, cb) {
          sodc += nobucket.fileId.sum_other_doc_count;
          async.each(nobucket.fileId.buckets, function (fsitem, cb) {
            Db.fileIdToFile(nobucket.key, fsitem.key, function (file) {
              if (file && file.name) {
                nresults.push({ key: file.name, doc_count: fsitem.doc_count });
              }
              cb();
            });
          }, function () {
            cb();
          });
        }, function () {
          nresults = nresults.sort(function (a, b) {
            if (a.doc_count === b.doc_count) {
              return a.key.localeCompare(b.key);
            }
            return b.doc_count - a.doc_count;
          });
          sessions.aggregations.fileand = { doc_count_error_upper_bound: 0, sum_other_doc_count: sodc, buckets: nresults };
          return sendResult();
        });
      });
    });
  };

  /**
   * POST/GET (preferred method is POST)
   *
   * Builds an elasticsearch session query. Gets a list of values for a field with counts and graph data and returns them to the client.
   * @name /api/spigraph
   * @param {number} date=1 - The number of hours of data to return (-1 means all data). Defaults to 1
   * @param {string} expression - The search expression string
   * @param {number} startTime - If the date parameter is not set, this is the start time of data to return. Format is seconds since Unix EPOC.
   * @param {number} stopTime - If the date parameter is not set, this is the stop time of data to return. Format is seconds since Unix EPOC.
   * @param {string} view - The view name to apply before the expression.
   * @param {string} field=node - The database field to get data for. Defaults to "node".
   * @param {string} bounding=last - Query sessions based on different aspects of a session's time. Options include:
     'first' - First Packet: the timestamp of the first packet received for the session.
     'last' - Last Packet: The timestamp of the last packet received for the session.
     'both' - Bounded: Both the first and last packet timestamps for the session must be inside the time window.
     'either' - Session Overlaps: The timestamp of the first packet must be before the end of the time window AND the timestamp of the last packet must be after the start of the time window.
     'database' - Database: The timestamp the session was written to the database. This can be up to several minutes AFTER the last packet was received.
   * @param {boolean} strictly=false - When set the entire session must be inside the date range to be observed, otherwise if it overlaps it is displayed. Overwrites the bounding parameter, sets bonding to 'both'
   * @returns {object} Sends the response to the client
   */
  module.getSPIGraph = (req, res) => {
    req.body.facets = 1;

    module.buildSessionQuery(req, (bsqErr, query, indices) => {
      let results = { items: [], graph: {}, map: {} };
      if (bsqErr) {
        return res.molochError(403, bsqErr.toString());
      }

      let options;
      if (req.body.cancelId) {
        options = { cancelId: `${req.user.userId}::${req.body.cancelId}` };
      }

      delete query.sort;
      query.size = 0;
      const size = +req.body.size || 20;

      let field = req.body.field || 'node';

      if (req.body.exp === 'ip.dst:port') { field = 'ip.dst:port'; }

      if (field === 'ip.dst:port') {
        query.aggregations.field = { terms: { field: 'dstIp', size: size }, aggregations: { sub: { terms: { field: 'dstPort', size: size } } } };
      } else if (field === 'fileand') {
        query.aggregations.field = { terms: { field: 'node', size: 1000 }, aggregations: { sub: { terms: { field: 'fileId', size: size } } } };
      } else {
        query.aggregations.field = { terms: { field: field, size: size * 2 } };
      }

      Promise.all([
        Db.healthCachePromise(),
        Db.numberOfDocuments('sessions2-*'),
        Db.searchPrimary(indices, 'session', query, options)
      ]).then(([health, total, result]) => {
        if (result.error) { throw result.error; }

        results.health = health;
        results.recordsTotal = total.count;
        results.recordsFiltered = result.hits.total;

        results.graph = ViewerUtils.graphMerge(req, query, result.aggregations);
        results.map = ViewerUtils.mapMerge(result.aggregations);

        if (!result.aggregations) {
          result.aggregations = { field: { buckets: [] } };
        }

        let aggs = result.aggregations.field.buckets;
        let filter = { term: {} };
        let sfilter = { term: {} };
        query.query.bool.filter.push(filter);

        if (field === 'ip.dst:port') {
          query.query.bool.filter.push(sfilter);
        }

        delete query.aggregations.field;

        let queriesInfo = [];
        function endCb () {
          queriesInfo = queriesInfo.sort((a, b) => { return b.doc_count - a.doc_count; }).slice(0, size * 2);
          let queries = queriesInfo.map((item) => { return item.query; });

          Db.msearch(indices, 'session', queries, options, function (err, result) {
            if (!result.responses) {
              return res.send(results);
            }

            result.responses.forEach(function (item, i) {
              let response = {
                name: queriesInfo[i].key,
                count: queriesInfo[i].doc_count
              };

              response.graph = ViewerUtils.graphMerge(req, query, result.responses[i].aggregations);

              let histoKeys = Object.keys(results.graph).filter(i => i.toLowerCase().includes('histo'));
              let xMinName = histoKeys.reduce((prev, curr) => results.graph[prev][0][0] < results.graph[curr][0][0] ? prev : curr);
              let histoXMin = results.graph[xMinName][0][0];
              let xMaxName = histoKeys.reduce((prev, curr) => {
                return results.graph[prev][results.graph[prev].length - 1][0] > results.graph[curr][results.graph[curr].length - 1][0] ? prev : curr;
              });
              let histoXMax = results.graph[xMaxName][results.graph[xMaxName].length - 1][0];

              if (response.graph.xmin === null) {
                response.graph.xmin = results.graph.xmin || histoXMin;
              }

              if (response.graph.xmax === null) {
                response.graph.xmax = results.graph.xmax || histoXMax;
              }

              response.map = ViewerUtils.mapMerge(result.responses[i].aggregations);

              results.items.push(response);
              histoKeys.forEach(item => {
                response[item] = 0.0;
              });

              let graph = response.graph;
              for (let j = 0; j < histoKeys.length; j++) {
                item = histoKeys[j];
                for (let i = 0; i < graph[item].length; i++) {
                  response[item] += graph[item][i][1];
                }
              }

              if (graph.totPacketsTotal !== undefined) {
                response.totPacketsHisto = graph.totPacketsTotal;
              }
              if (graph.totDataBytesTotal !== undefined) {
                response.totDataBytesHisto = graph.totDataBytesTotal;
              }
              if (graph.totBytesTotal !== undefined) {
                response.totBytesHisto = graph.totBytesTotal;
              }

              if (results.items.length === result.responses.length) {
                let s = req.body.sort || 'sessionsHisto';
                results.items = results.items.sort(function (a, b) {
                  let result;
                  if (s === 'name') {
                    result = a.name.localeCompare(b.name);
                  } else {
                    result = b[s] - a[s];
                  }
                  return result;
                }).slice(0, size);
                return res.send(results);
              }
            });
          });
        }

        let intermediateResults = [];
        function findFileNames () {
          async.each(intermediateResults, function (fsitem, cb) {
            let split = fsitem.key.split(':');
            let node = split[0];
            let fileId = split[1];
            Db.fileIdToFile(node, fileId, function (file) {
              if (file && file.name) {
                queriesInfo.push({ key: file.name, doc_count: fsitem.doc_count, query: fsitem.query });
              }
              cb();
            });
          }, function () {
            endCb();
          });
        }

        aggs.forEach((item) => {
          if (field === 'ip.dst:port') {
            filter.term.dstIp = item.key;
            let sep = (item.key.indexOf(':') === -1) ? ':' : '.';
            item.sub.buckets.forEach((sitem) => {
              sfilter.term.dstPort = sitem.key;
              queriesInfo.push({ key: item.key + sep + sitem.key, doc_count: sitem.doc_count, query: JSON.stringify(query) });
            });
          } else if (field === 'fileand') {
            filter.term.node = item.key;
            item.sub.buckets.forEach((sitem) => {
              sfilter.term.fileand = sitem.key;
              intermediateResults.push({ key: filter.term.node + ':' + sitem.key, doc_count: sitem.doc_count, query: JSON.stringify(query) });
            });
          } else {
            filter.term[field] = item.key;
            queriesInfo.push({ key: item.key, doc_count: item.doc_count, query: JSON.stringify(query) });
          }
        });

        if (field === 'fileand') { return findFileNames(); }

        return endCb();
      }).catch((err) => {
        console.log('spigraph.json error', err);
        return res.molochError(403, ViewerUtils.errorString(err));
      });
    });
  };

  /**
   * POST/GET (preferred method is POST)
   *
   * Builds an elasticsearch session query. Gets a list of values for each field with counts and returns them to the client.
   * @name /api/spigraph
   * @param {number} date=1 - The number of hours of data to return (-1 means all data). Defaults to 1
   * @param {string} expression - The search expression string
   * @param {number} startTime - If the date parameter is not set, this is the start time of data to return. Format is seconds since Unix EPOC.
   * @param {number} stopTime - If the date parameter is not set, this is the stop time of data to return. Format is seconds since Unix EPOC.
   * @param {string} view - The view name to apply before the expression.
   * @param {string} exp - Comma separated list of db fields to populate the graph/table.
   * @param {string} bounding=last - Query sessions based on different aspects of a session's time. Options include:
     'first' - First Packet: the timestamp of the first packet received for the session.
     'last' - Last Packet: The timestamp of the last packet received for the session.
     'both' - Bounded: Both the first and last packet timestamps for the session must be inside the time window.
     'either' - Session Overlaps: The timestamp of the first packet must be before the end of the time window AND the timestamp of the last packet must be after the start of the time window.
     'database' - Database: The timestamp the session was written to the database. This can be up to several minutes AFTER the last packet was received.
   * @param {boolean} strictly=false - When set the entire session must be inside the date range to be observed, otherwise if it overlaps it is displayed. Overwrites the bounding parameter, sets bonding to 'both'
   * @returns {object} Sends the response to the client
   */
  module.getSPIGraphHierarchy = (req, res) => {
    if (req.body.exp === undefined) {
      return res.molochError(403, 'Missing exp parameter');
    }

    let fields = [];
    let parts = req.body.exp.split(',');
    for (let i = 0; i < parts.length; i++) {
      if (internals.scriptAggs[parts[i]] !== undefined) {
        fields.push(internals.scriptAggs[parts[i]]);
        continue;
      }
      let field = Config.getFieldsMap()[parts[i]];
      if (!field) {
        return res.molochError(403, `Unknown expression ${parts[i]}\n`);
      }
      fields.push(field);
    }

    module.buildSessionQuery(req, (err, query, indices) => {
      query.size = 0; // Don't need any real results, just aggregations
      delete query.sort;
      delete query.aggregations;
      const size = +req.body.size || 20;

      if (!query.query.bool.must) {
        query.query.bool.must = [];
      }

      let lastQ = query;
      for (let i = 0; i < fields.length; i++) {
        // Require that each field exists
        query.query.bool.must.push({ exists: { field: fields[i].dbField } });

        if (fields[i].script) {
          lastQ.aggregations = { field: { terms: { script: { lang: 'painless', source: fields[i].script }, size: size } } };
        } else {
          lastQ.aggregations = { field: { terms: { field: fields[i].dbField, size: size } } };
        }
        lastQ = lastQ.aggregations.field;
      }

      if (Config.debug > 2) {
        console.log('spigraph pie aggregations', indices, JSON.stringify(query, false, 2));
      }

      Db.searchPrimary(indices, 'session', query, null, function (err, result) {
        if (err) {
          console.log('spigraphpie ERROR', err);
          res.status(400);
          return res.end(err);
        }

        if (Config.debug > 2) {
          console.log('result', JSON.stringify(result, false, 2));
        }

        // format the data for the pie graph
        let hierarchicalResults = { name: 'Top Talkers', children: [] };
        function addDataToPie (buckets, addTo) {
          for (let i = 0; i < buckets.length; i++) {
            let bucket = buckets[i];
            addTo.push({
              name: bucket.key,
              size: bucket.doc_count
            });
            if (bucket.field) {
              addTo[i].children = [];
              addTo[i].size = undefined; // size is interpreted from children
              addTo[i].sizeValue = bucket.doc_count; // keep sizeValue for display
              addDataToPie(bucket.field.buckets, addTo[i].children);
            }
          }
        }

        let grandparent;
        let tableResults = [];
        // assumes only 3 levels deep
        function addDataToTable (buckets, parent) {
          for (let i = 0; i < buckets.length; i++) {
            let bucket = buckets[i];
            if (bucket.field) {
              if (parent) { grandparent = parent; }
              addDataToTable(bucket.field.buckets, {
                name: bucket.key,
                size: bucket.doc_count
              });
            } else {
              tableResults.push({
                parent: parent,
                grandparent: grandparent,
                name: bucket.key,
                size: bucket.doc_count
              });
            }
          }
        }

        addDataToPie(result.aggregations.field.buckets, hierarchicalResults.children);
        addDataToTable(result.aggregations.field.buckets);

        return res.send({
          success: true,
          tableResults: tableResults,
          hierarchicalResults: hierarchicalResults
        });
      });
    });
  };

  return module;
};
