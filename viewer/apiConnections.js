/******************************************************************************/
/* apiConnections.js -- api calls for connections tab
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const Config = require('./config.js');
const Db = require('./db.js');
const async = require('async');
const util = require('util');
const ArkimeUtil = require('../common/arkimeUtil');
const ArkimeConfig = require('../common/arkimeConfig');
const ViewerUtils = require('./viewerUtils');
const SessionAPIs = require('./apiSessions');

let fieldsMap;

ArkimeConfig.loaded(() => {
  if (!fieldsMap) {
    setTimeout(() => { // make sure db.js loads before fetching fields
      ViewerUtils.loadFields()
        .then((result) => {
          fieldsMap = result.fieldsMap;
        });
    });
  }
});

class ConnectionAPIs {
  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------
  // buildConnectionQuery(req, fields, options, fsrc, fdst, dstipport, resultId, cb)
  //
  // Returns (via "return cb(...)") an array of 1..2 connection query objects
  // (see the definition of "result" at the beginning of the function), depending on
  // whether or not baseline is enabled. The query and indices are initially returned
  // from buildSessionQuery and then adjusted by this function.
  //
  // The queries represented by these objects can be executed via
  // dbConnectionQuerySearch.
  //
  // This code was factored out from buildConnections.
  // --------------------------------------------------------------------------
  static #buildConnectionQuery (req, fields, options, fsrc, fdst, dstipport, resultId, cb) {
    const result = {
      resultId,
      err: null,
      query: null,
      indices: null,
      options
    };

    // If network graph baseline is enabled (enabled: req.query.baselineDate != 0, disabled:req.query.baselineDate=0 or undefined)
    //   then two queries will be run (ie., run buildSessionQuery->searchSessions->process twice): first for the
    //   original specified time frame and second for the same time frame immediately preceding it.
    //
    // Nodes have an .inresult attribute where:
    //   0 = 00 = not in either result set (although you'll never see these, obviously)
    //   1 = 01 = seen during the "current" time frame but not in the "baseline" time frame (ie., "new")
    //   2 = 10 = seen during the "baseline" time frame but not in the "current" time frame (ie., "old")
    //   3 = 11 = seen during both the "current" time frame and the "baseline" time frame
    // This is only performed where startTime/startTime are defined, and never for "all" time range (date=-1).
    //
    // With baselining, req.query.baselineDate can determine baseline time frame, which is the number of
    // hours prior to the "start" query time, similar to req.query.date. If unspecified or zero, baseline
    // uses the immediate time frame of the same duration immediately prior to the req.query.startTime.
    // However, If req.query.baselineDate ends with x, the duration of the baseline is the time frame of
    // the "current" time frame multiplied by that number.
    let doBaseline = false;
    let baselineDate = 0;
    let baselineDateIsMultiplier = false;

    if (req.query.baselineDate !== undefined && !ArkimeUtil.isString(req.query.baselineDate, 0)) {
      result.err = 'Bad query.baselineDate';
      return cb([result]);
    }

    if ((ArkimeUtil.isString(req.query.baselineDate) && (req.query.baselineDate !== '0') &&
          (req.query.date !== '-1') && (req.query.startTime !== undefined) && (req.query.stopTime !== undefined)) ||
        (resultId > 1)) {
      doBaseline = true;
      let baselineDateTmpStr = req.query.baselineDate;
      if (baselineDateTmpStr.endsWith('x')) {
        baselineDateIsMultiplier = true;
        baselineDateTmpStr = baselineDateTmpStr.slice(0, -1);
      }
      baselineDate = parseInt(baselineDateTmpStr, 10);
      doBaseline = baselineDate > 0;
      baselineDateIsMultiplier = baselineDateIsMultiplier && (baselineDate > 0);
    }

    // use a copy of req.query as we will modify the startTime/stopTime if we are doing a baseline query
    const tmpReqQuery = JSON.parse(JSON.stringify(req.query));

    if (resultId > 1) {
      // replace current time frame start/stop values with baseline time frame start/stop values
      const currentQueryTimes = ViewerUtils.determineQueryTimes(req.query);
      if (Config.debug) {
        console.log('buildConnectionQuery baseline.0', 'startTime', currentQueryTimes[0], 'stopTime', currentQueryTimes[1], baselineDate, baselineDateIsMultiplier ? 'x' : '');
      }
      if ((currentQueryTimes[0] !== undefined) && (currentQueryTimes[1] !== undefined)) {
        // baseline stop time ends 1 second prior to "current" start time
        tmpReqQuery.stopTime = currentQueryTimes[0] - 1;
        if ((baselineDate > 0) && (!baselineDateIsMultiplier)) {
          // baseline time duration was specified (hours)
          tmpReqQuery.startTime = tmpReqQuery.stopTime - (60 * 60 * baselineDate);
        } else {
          // baseline time frame is unspecified, so use the immediate prior time frame of same (or multiplied) duration
          tmpReqQuery.startTime = tmpReqQuery.stopTime - ((currentQueryTimes[1] - currentQueryTimes[0]) * (baselineDateIsMultiplier ? baselineDate : 1));
        }
        if (Config.debug) {
          console.log('buildConnectionQuery baseline.1', 'startTime', tmpReqQuery.startTime, 'stopTime', tmpReqQuery.stopTime, 'diff', (tmpReqQuery.stopTime - tmpReqQuery.startTime));
        }
      }
    } // resultId > 1 (calculating baseline query time frame)

    SessionAPIs.buildSessionQuery(req, (bsqErr, query, indices) => {
      if (bsqErr) {
        console.log('ERROR - buildConnectionQuery -> buildSessionQuery', resultId, util.inspect(bsqErr, false, 50));
        result.err = bsqErr;
        return cb([result]);
      } else {
        query.query.bool.filter.push({ exists: { field: req.query.srcField } });
        query.query.bool.filter.push({ exists: { field: req.query.dstField } });

        query.fields = fields;
        query._source = false;
        query.docvalue_fields = [fsrc, fdst];

        if (dstipport) {
          query.fields.push('destination.port');
        }

        result.query = JSON.parse(JSON.stringify(query));
        result.indices = JSON.parse(JSON.stringify(indices));

        if ((resultId === 1) && (doBaseline)) {
          ConnectionAPIs.#buildConnectionQuery(req, fields, options, fsrc, fdst, dstipport, resultId + 1, (baselineResult) => {
            return cb([result].concat(baselineResult));
          });
        } else {
          return cb([result]);
        }
      } // bsqErr if/else
    }, tmpReqQuery); // buildSessionQuery
  } // buildConnectionQuery

  // --------------------------------------------------------------------------
  // dbConnectionQuerySearch(connQueries, resultId, cb)
  //
  // Executes the query/queries specified in the connQueries array (elements are
  // of the type returned by buildConnectionQuery) by calling Db.searchSessions
  // and returns the results via callback (see the definition of the "resultSet"
  // object at the beginning of this function). The results are returned in an
  // array containing the result sets which correspond to the queries in the
  // connQueries array.
  //
  // This code was factored out from buildConnections.
  // --------------------------------------------------------------------------
  static #dbConnectionQuerySearch (connQueries, cb) {
    const resultSet = {
      resultId: null,
      err: null,
      graph: null
    };

    if (connQueries.length > 0) {
      resultSet.resultId = connQueries[0] ? connQueries[0].resultId : null;
      resultSet.err = connQueries[0] ? connQueries[0].err : 'null query object';

      if (((connQueries[0]) && (connQueries[0].err)) || (!connQueries[0])) {
        // propogate query errors up into the result set without doing a search
        console.log('ERROR - buildConnectionQuery -> dbConnectionQuerySearch', resultSet.resultId, util.inspect(resultSet.err, false, 50));
        return cb([resultSet]);
      } else {
        Db.searchSessions(connQueries[0].indices, connQueries[0].query, connQueries[0].options, (err, graph) => {
          if (err || graph.error) {
            console.log('ERROR - buildConnectionQuery -> dbConnectionQuerySearch -> Db.searchSessions', connQueries[0].resultId, util.inspect(err, false, 50));
            resultSet.err = err || graph.error;
          }
          resultSet.graph = graph;
          if (connQueries.length > 1) {
            ConnectionAPIs.#dbConnectionQuerySearch(connQueries.slice(1), (baselineResultSet) => {
              return cb([resultSet].concat(baselineResultSet));
            });
          } else {
            return cb([resultSet]);
          }
        }); // Db.searchSessions
      } // if connQueries[0].err) / else
    } else {
      return cb([null]);
    } // (connQueries.length > 0) / else
  } // dbConnectionQuerySearch

  // --------------------------------------------------------------------------
  // buildConnections(req, res, cb)
  //
  // Returns objects needed to populate the graph of logical connections between
  // nodes representing fields of sessions.
  //
  // function flow is:
  //
  // 0. buildConnections
  // 1. buildConnectionQuery       - creates array of 1..2 connQueries
  // 2. dbConnectionQuerySearch    - executes connQueries searches via Db.searchSessions
  // 3. processResultSets          - accumulate nodes and links into nodesHash/connects hashes
  //    - process
  //      - updateValues
  // 4. processResultSets callback - distill nodesHash/connects hashes into
  //                                 nodes/links arrays and return
  // --------------------------------------------------------------------------
  static #buildConnections (req, res, cb) {
    let dstipport;
    if (req.query.dstField === 'ip.dst:port' || req.query.dstField === 'destination.ip:port') {
      dstipport = true;
      req.query.dstField = 'destination.ip';
    }

    req.query.srcField = Config.getDBField(req.query.srcField ?? 'source.ip', 'dbField');
    req.query.dstField = Config.getDBField(req.query.dstField ?? 'destination.ip', 'dbField');
    const fsrc = req.query.srcField;
    const fdst = req.query.dstField;
    const minConn = req.query.minConn || 1;

    // get the requested fields
    let reqFields = ['network.bytes', 'totDataBytes', 'network.packets', 'node'];
    if (req.query.fields) { reqFields = req.query.fields.split(','); }

    let options = {};
    if (req.query.cancelId) {
      options.cancelId = `${req.user.userId}::${req.query.cancelId}`;
    }
    options.arkime_unflatten = false;
    options = ViewerUtils.addCluster(req.query.cluster, options);

    const dstIsIp = fdst.match(/(\.ip|Ip)$/);

    const nodesHash = {};
    const connects = {};
    const nodes = [];
    const links = [];
    let totalHits = 0;

    // ------------------------------------------------------------------------
    // updateValues and process are for aggregating query results into their final form
    function updateValues (data, property, fields) {
      for (const i in fields) {
        const dbField = fields[i];
        const field = Config.getDBField(dbField);
        if (data[dbField]) {
          // sum integers
          if (field.type === 'integer' && field.category !== 'port') {
            property[dbField] = (property[dbField] ?? 0) + data[dbField];
          } else { // make a list of values
            if (!property[dbField]) { property[dbField] = []; }
            // make all values an array (because sometimes they are by default)
            let values = [data[dbField]];
            if (Array.isArray(data[dbField])) {
              values = data[dbField];
            }
            for (const value of values) {
              property[dbField].push(value);
            }
            if (property[dbField] && Array.isArray(property[dbField])) {
              property[dbField] = [...new Set(property[dbField])]; // unique only
            }
          }
        } else if (property[dbField] === undefined && field.type === 'integer' && field.category !== 'port') {
          property[dbField] = 0;
        }
      }
    } // updateValues

    // ------------------------------------------------------------------------
    function doProcess (vsrc, vdst, f, fields, resultId) {
      // ES 6 is returning formatted timestamps instead of ms like pre 6 did
      // https://github.com/elastic/elasticsearch/issues/27740
      if (vsrc.length === 24 && vsrc[23] === 'Z' && vsrc.match(/^\d\d\d\d-\d\d-\d\dT\d\d:\d\d:\d\d.\d\d\dZ$/)) {
        vsrc = new Date(vsrc).getTime();
      }
      if (vdst.length === 24 && vdst[23] === 'Z' && vdst.match(/^\d\d\d\d-\d\d-\d\dT\d\d:\d\d:\d\d.\d\d\dZ$/)) {
        vdst = new Date(vdst).getTime();
      }

      if (nodesHash[vsrc] === undefined) {
        nodesHash[vsrc] = { id: `${vsrc}`, cnt: 0, sessions: 0, inresult: 0 };
      }

      nodesHash[vsrc].sessions++;
      nodesHash[vsrc].type |= 1;
      nodesHash[vsrc].inresult |= resultId;
      updateValues(f, nodesHash[vsrc], fields);

      if (nodesHash[vdst] === undefined) {
        nodesHash[vdst] = { id: `${vdst}`, cnt: 0, sessions: 0, inresult: 0 };
      }

      nodesHash[vdst].sessions++;
      nodesHash[vdst].type |= 2;
      nodesHash[vdst].inresult |= resultId;
      updateValues(f, nodesHash[vdst], fields);

      const linkId = `${vsrc}->${vdst}`;
      if (connects[linkId] === undefined) {
        connects[linkId] = { value: 0, source: vsrc, target: vdst };
        nodesHash[vsrc].cnt++;
        nodesHash[vdst].cnt++;
      }

      connects[linkId].value++;
      updateValues(f, connects[linkId], fields);
    } // process

    // ------------------------------------------------------------------------
    // processResultSets - process the hits of each search resultset into nodesHash and connects
    function processResultSets (connResultSets, processResultSetsCb) {
      const resultSetStatus = {
        resultId: null,
        err: null,
        hits: 0
      };

      if (connResultSets.length > 0) {
        resultSetStatus.resultId = connResultSets[0] ? connResultSets[0].resultId : null;
        resultSetStatus.err = connResultSets[0] ? connResultSets[0].err : 'null resultset';

        if (((connResultSets[0]) && (connResultSets[0].err)) || (!connResultSets[0])) {
          // propogate errors up (and stop processing)
          console.log('ERROR - buildConnectionQuery -> processResultSets', resultSetStatus.resultId, util.inspect(resultSetStatus.err, false, 50));
          return processResultSetsCb([resultSetStatus]);
        } else {
          async.eachLimit(connResultSets[0].graph.hits.hits, 10, (hit, hitCb) => {
            const f = hit.fields;

            let asrc = hit.fields[fsrc];
            let adst = hit.fields[fdst];

            if (asrc === undefined || adst === undefined) {
              return setImmediate(hitCb);
            }

            if (!Array.isArray(asrc)) { asrc = [asrc]; }
            if (!Array.isArray(adst)) { adst = [adst]; }

            for (const vsrc of asrc) {
              for (let vdst of adst) {
                if (dstIsIp && dstipport) {
                  if (vdst.includes(':')) {
                    vdst += '.' + f['destination.port'];
                  } else {
                    vdst += ':' + f['destination.port'];
                  }
                }
                doProcess(vsrc, vdst, f, reqFields, connResultSets[0].resultId);
              } // let vdst of adst
            } // for vsrc of asrc
            setImmediate(hitCb);
          }, function (err) {
            resultSetStatus.err = err;
            resultSetStatus.hits = connResultSets[0].graph.hits.total;
            if (connResultSets.length > 1) {
              processResultSets(connResultSets.slice(1), (baselineResultSetStatus) => {
                return processResultSetsCb([resultSetStatus].concat(baselineResultSetStatus));
              });
            } else {
              return processResultSetsCb([resultSetStatus]);
            }
          }); // async.eachLimit(graph.hits.hits) / function(err)
        } // if connResultSets[0].err) / else
      } else {
        return processResultSetsCb([null]);
      } // (connResultSets.length > 0) / else
    } // processResultSets

    // ------------------------------------------------------------------------
    // call to build the session query|queries and indices
    ConnectionAPIs.#buildConnectionQuery(req, reqFields, options, fsrc, fdst, dstipport, 1, (connQueries) => {
      if (Config.debug) {
        console.log('buildConnections.connQueries', connQueries.length, JSON.stringify(connQueries, null, 2));
      }

      // ONE or TWO session queries will be executed, depending on if baseline is enabled:
      //   1. for the "current" time frame, the one specified originally in req.query
      //   2. for the "baseline" time frame immediately prior to the time frame of "1."
      //      (only if baseline is enabled)
      // The call to process() will ensure the resultId value is OR'ed into the .inresult
      //   attribute of each node.

      // prepare and execute the Db.searchSessions query|queries
      if (connQueries.length > 0) {
        ConnectionAPIs.#dbConnectionQuerySearch(connQueries, (connResultSets) => {
          if (Config.debug) {
            console.log('buildConnections.connResultSets', connResultSets.length, JSON.stringify(connResultSets, null, 2));
          }

          // aggregate final return values for nodes and links
          processResultSets(connResultSets, (connResultSetStats) => {
            if (Config.debug) {
              console.log('buildConnections.processResultSets', connResultSetStats.length, JSON.stringify(connResultSetStats, null, 2));
            }

            for (const stat of connResultSetStats) {
              if (stat.err) {
                return cb(stat.err, null, null, null);
              }
              totalHits += stat.hits;
            }

            let nodeKeys = Object.keys(nodesHash);
            if (ArkimeConfig.regressionTests) {
              nodeKeys = nodeKeys.sort((a, b) => {
                return nodesHash[a].id.localeCompare(nodesHash[b].id);
              });
            }
            for (const node of nodeKeys) {
              if (nodesHash[node].cnt < minConn) {
                nodesHash[node].pos = -1;
              } else {
                nodesHash[node].pos = nodes.length;
                nodes.push(nodesHash[node]);
              }
            }

            for (const key in connects) {
              const c = connects[key];
              c.source = nodesHash[c.source].pos;
              c.target = nodesHash[c.target].pos;
              if (c.source >= 0 && c.target >= 0) {
                links.push(connects[key]);
              }
            }

            if (Config.debug) {
              console.log('buildConnections.nodesHash', nodesHash);
              console.log('buildConnections.connects', connects);
              console.log('buildConnections.nodes', nodes.length, nodes);
              console.log('buildConnections.links', links.length, links);
              console.log('buildConnections.totalHits', totalHits);
            }

            return cb(null, nodes, links, totalHits);
          }); // processResultSets.callback
        }); // dbConnectionQuerySearch.callback
      } else {
        const err = 'no connection queries generated';
        console.log('ERROR - buildConnectionQuery', util.inspect(err, false, 50));
        return cb(err, null, null, null);
      } // connQueries.length check
    }); // buildConnectionQuery.callback
  } // buildConnections

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  /**
   * POST/GET - /api/connections
   *
   * Builds an elasticsearch connections query. Gets a list of nodes and links and returns them to the client.
   * @name /connections
   * @param {SessionsQuery} See_List - This API supports a common set of parameters documented in the SessionsQuery section
   * @param {string} srcField=ip.src - The source database field name
   * @param {string} dstField=ip.dst:port - The destination database field name
   * @param {number} baselineDate=0 - The baseline date range to compare connections against. Default is 0, disabled. Options include:
     1x - 1 times query range.
     2x - 2 times query range.
     4x - 4 times query range.
     6x - 6 times query range.
     8x - 8 times query range.
     10x - 10 times query range.
     1 - 1 hour.
     6 - 6 hours.
     24 - 1 day.
     48 - 2 days.
     72 - 3 days.
     168 - 1 week.
     336 - 2 weeks.
     720 - 1 month.
     1440 - 2 months.
     4380 - 6 months.
     8760 - 1 year.
   * @param {string} baselineVis=all - Which connections to display when a baseline date range is applied. Default is all. Options include:
     'all' - All Nodes: all nodes are visible.
     'actual' - Actual Nodes: nodes present in the "current" timeframe query results are visible.
     'actualold' - Baseline Nodes: nodes present in the "baseline" timeframe query results are visible.
     'new' - New Nodes Only: nodes present in the "current" but NOT the "baseline" timeframe are visible.
     'old' - Baseline Nodes Only: nodes present in the "baseline" but NOT the "current" timeframe are visible.
   * @returns {array} links - The list of links
   * @returns {array} nodes - The list of nodes
   */
  static getConnections (req, res) {
    ConnectionAPIs.#buildConnections(req, res, (err, nodes, links, total) => {
      if (err) { return res.serverError(403, err.toString()); }
      res.send({
        nodes,
        links,
        recordsFiltered: total
      });
    });
  };

  // --------------------------------------------------------------------------
  /**
   * POST/GET - /api/connections/csv OR /api/connections.csv
   *
   * Builds an elasticsearch connections query. Gets a list of nodes and links in csv format and returns them to the client.
   * @name /connections/csv
   * @param {SessionsQuery} See_List - This API supports a common set of parameters documented in the SessionsQuery section
   * @param {string} srcField=ip.src - The source database field name
   * @param {string} dstField=ip.dst:port - The destination database field name
   * @returns {csv} csv - The csv with the connections requested
   */
  static getConnectionsCSV (req, res) {
    ArkimeUtil.noCache(req, res, 'text/csv');

    const seperator = req.query.seperator ?? ',';
    ConnectionAPIs.#buildConnections(req, res, (err, nodes, links, total) => {
      if (err) {
        return res.send(err);
      }

      // write out the fields requested
      let fields = ['network.bytes', 'totDataBytes', 'network.packets', 'node'];
      if (req.query.fields) { fields = req.query.fields.split(','); }

      res.write('Source, Destination, Sessions');
      const displayFields = {};
      for (const field of fields) {
        const map = JSON.parse(fieldsMap);
        for (const f in map) {
          if (map[f].dbField === field) {
            const friendlyName = map[f].friendlyName;
            displayFields[field] = map[f];
            res.write(`, ${friendlyName}`);
          }
        }
      }
      res.write('\r\n');

      for (let i = 0, ilen = links.length; i < ilen; i++) {
        res.write('"' + nodes[links[i].source].id.replaceAll('"', '""') + '"' + seperator +
                  '"' + nodes[links[i].target].id.replaceAll('"', '""') + '"' + seperator +
                       links[i].value + seperator);
        for (let f = 0, flen = fields.length; f < flen; f++) {
          res.write(links[i][displayFields[fields[f]].dbField].toString());
          if (f !== flen - 1) { res.write(seperator); }
        }
        res.write('\r\n');
      }

      res.end();
    });
  };
};

module.exports = ConnectionAPIs;
