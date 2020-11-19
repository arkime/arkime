'use strict';

let util = require('util');
let async = require('async');

let Db = require('../db');
let Utils = require('../utils');
let Config = require('../config');
let molochparser = require('../molochparser');
let internals = require('../internals').internals;

// HELPERS ----------------------------------------------------------------- //
function addSortToQuery (query, info, d) {
  function addSortDefault () {
    if (d) {
      if (!query.sort) {
        query.sort = [];
      }
      var obj = {};
      obj[d] = { order: 'asc' };
      obj[d].missing = '_last';
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
      var parts = item.split(':');
      var field = parts[0];

      var obj = {};
      if (field === 'firstPacket') {
        obj.firstPacket = { order: parts[1] };
      } else if (field === 'lastPacket') {
        obj.lastPacket = { order: parts[1] };
      } else {
        obj[field] = { order: parts[1] };
      }

      obj[field].unmapped_type = 'string';
      var fieldInfo = Config.getDBFieldsMap()[field];
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

    var obj = {};
    var field = info['mDataProp_' + info['iSortCol_' + i]];
    obj[field] = { order: info['sSortDir_' + i] };
    query.sort.push(obj);

    if (field === 'firstPacket') {
      query.sort.push({ firstPacket: { order: info['sSortDir_' + i] } });
    } else if (field === 'lastPacket') {
      query.sort.push({ lastPacket: { order: info['sSortDir_' + i] } });
    }
  }
}

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
    continueBuildQueryCb(req, query, err, finalCb, queryOverride);
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
        continueBuildQueryCb(req, query, err, finalCb, queryOverride);
      }
    });
  }
}

function buildSessionQuery (req, buildCb, queryOverride = null) {
  // validate time limit is not exceeded
  let timeLimitExceeded = false;
  var interval;

  // queryOverride can supercede req.body if specified
  let reqQuery = queryOverride || req.body;

  // determineQueryTimes calculates startTime, stopTime, and interval from reqQuery
  let startAndStopParams = Utils.determineQueryTimes(reqQuery);
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

  let limit = Math.min(2000000, +reqQuery.length || +reqQuery.iDisplayLength || 100);

  var query = { from: reqQuery.start || reqQuery.iDisplayStart || 0,
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

  if (!err && reqQuery.view) { // TODO ECR - test with view
    addViewToQuery(req, query, Utils.continueBuildQuery, buildCb, queryOverride);
  } else {
    Utils.continueBuildQuery(req, query, err, buildCb, queryOverride);
  }
}

// APIs -------------------------------------------------------------------- //
function getSessions (req, res) {
  // if using GET not POST, body will be empty and params will have query
  if (!Object.keys(req.body).length) { req.body = req.query; }

  let map = {};
  let graph = {};

  let options;
  if (req.body.cancelId) {
    options = {
      cancelId: `${req.user.userId}::${req.body.cancelId}`
    };
  }

  buildSessionQuery(req, (bsqErr, query, indices) => { // TODO ECR
    if (bsqErr) {
      const r = {
        recordsTotal: 0,
        recordsFiltered: 0,
        graph: {},
        map: {},
        error: bsqErr.toString(),
        health: Db.healthCache(),
        data: []
      };
      return res.send(r);
    }

    let addMissing = false;
    if (req.body.fields) {
      query._source = Utils.queryValueToArray(req.body.fields);
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

      map = Utils.mapMerge(sessions.aggregations);
      graph = Utils.graphMerge(req, query, sessions.aggregations);

      let results = { total: sessions.hits.total, results: [] };
      async.each(sessions.hits.hits, (hit, hitCb) => {
        let fields = hit._source || hit.fields;
        if (fields === undefined) {
          return hitCb(null);
        }

        fields.id = Db.session2Sid(hit);

        if (parseInt(req.body.flatten) === 1) {
          fields = Utils.flattenFields(fields);
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
          Utils.fixFields(fields, function () {
            results.results.push(fields);
            return hitCb();
          });
        }
      }, function () {
        const r = {
          recordsTotal: total.count,
          recordsFiltered: (results ? results.total : 0),
          graph: graph,
          health: health,
          map: map,
          data: (results ? results.results : [])
        };
        res.logCounts(r.data.length, r.recordsFiltered, r.recordsTotal);

        try {
          res.send(r);
        } catch (e) {
          console.trace('fetch sessions error', e.stack);
        }
      });
    }).catch((err) => {
      console.log('ERROR - sessions error', err);
      const r = {
        recordsTotal: 0,
        recordsFiltered: 0,
        graph: {},
        map: {},
        health: Db.healthCache(),
        data: [],
        error: err.toString()
      };
      res.send(r);
    });
  });
}

module.exports = { getSessions, buildSessionQuery };
