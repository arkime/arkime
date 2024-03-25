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
const async = require('async');
const http = require('http');
const https = require('https');
const ArkimeUtil = require('../common/arkimeUtil');
const Auth = require('../common/auth');
const User = require('../common/user');
const arkimeparser = require('./arkimeparser.js');
const internals = require('./internals');

class ViewerUtils {
  // ----------------------------------------------------------------------------
  static addCaTrust (options, node) {
    if (!Config.isHTTPS(node)) {
      return;
    }

    let certs = internals.caTrustCerts.get(node);
    if (certs && certs.length > 0) {
      options.ca = certs;
      return;
    }

    const caTrustFile = Config.getFull(node, 'caTrustFile');
    certs = ArkimeUtil.certificateFileToArray(caTrustFile);
    internals.caTrustCerts.set(node, certs);

    if (certs && certs.length > 0) {
      options.ca = certs;
    }
  };

  // ----------------------------------------------------------------------------
  static queryValueToArray (val) {
    if (val === undefined || val === null) {
      return [];
    }
    if (!Array.isArray(val)) {
      val = [val];
    }
    return val.join(',').split(',');
  };

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

    if (reqQuery.date && isNaN(parseInt(reqQuery.date))) {
      reqQuery.date = '-1';
    }

    if ((reqQuery.date && parseInt(reqQuery.date) === -1) ||
        (reqQuery.segments && reqQuery.segments === 'all')) {
      interval = 60 * 60; // Hour to be safe
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

      const diff = reqQuery.stopTime - reqQuery.startTime;
      if (diff < 30 * 60) {
        interval = 1; // second
      } else if (diff <= 5 * 24 * 60 * 60) {
        interval = 60; // minute
      } else {
        interval = 60 * 60; // hour
      }
    } else {
      const queryDate = reqQuery.date || 1;
      startTimeSec = (Math.floor(Date.now() / 1000) - 60 * 60 * parseInt(queryDate, 10));
      stopTimeSec = Date.now() / 1000;

      if (queryDate <= 5 * 24) {
        interval = 60; // minute
      } else {
        interval = 60 * 60; // hour
      }
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
  };

  // ----------------------------------------------------------------------------
  /* This method fixes up parts of the query that jison builds to what ES actually
   * understands.  This includes using the collapse function and the filename mapping.
   */
  static lookupQueryItems (query, doneCb) {
    // console.log('BEFORE', JSON.stringify(query, false, 2));
    ViewerUtils.#collapseQuery(query);
    // console.log('AFTER', JSON.stringify(query, false, 2));
    if (Config.get('multiES', false)) {
      return doneCb(null);
    }

    let outstanding = 0;
    let finished = 0;
    let err = null;

    function doProcess (qParent, obj, item) {
      // console.log("\nprocess:\n", item, obj, typeof obj[item], "\n");
      if (item === 'fileand' && typeof obj[item] === 'string') {
        const fileName = obj.fileand;
        delete obj.fileand;
        outstanding++;
        Db.fileNameToFiles(fileName, function (files) {
          outstanding--;
          if (files === null || files.length === 0) {
            err = "File '" + fileName + "' not found";
          } else if (files.length > 1) {
            obj.bool = { should: [] };
            files.forEach(function (file) {
              obj.bool.should.push({ bool: { filter: [{ term: { node: file.node } }, { term: { fileId: file.num } }] } });
            });
          } else {
            obj.bool = { filter: [{ term: { node: files[0].node } }, { term: { fileId: files[0].num } }] };
          }
          if (finished && outstanding === 0) {
            doneCb(err);
          }
        });
      } else if (item === 'field' && obj.field === 'fileand') {
        obj.field = 'fileId';
      } else if (typeof obj[item] === 'object') {
        convert(obj, obj[item]);
      }
    }

    function convert (qParent, obj) {
      for (const item in obj) {
        doProcess(qParent, obj, item);
      }
    }

    convert(null, query);
    if (outstanding === 0) {
      return doneCb(err);
    }

    finished = 1;
  };

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
          // most_not: { - when just NOTing one thing
          query.bool.must_not = query.bool.must_not.bool.should;
          ViewerUtils.#collapseQuery(query);
        } else if (query.bool?.must_not?.length > 0) {
          // We can collapse both must_not: most_not: and most_not: should:
          const len = query.bool.must_not.length;
          const newItems = newArray(newArray(query.bool.must_not, 'must_not'), 'should');
          query.bool.must_not = newItems;
          if (newItems.length !== len) {
            ViewerUtils.#collapseQuery(query);
          } else {
            ViewerUtils.#collapseQuery(query.bool.must_not);
          }
        } else if (query.bool?.should?.length > 0) {
          // Collapse should: should:
          const len = query.bool.should.length;
          const newItems = newArray(query.bool.should, 'should');
          query.bool.should = newItems;
          if (newItems.length !== len) {
            ViewerUtils.#collapseQuery(query);
          } else {
            ViewerUtils.#collapseQuery(query.bool.should);
          }
        } else if (query.bool?.filter?.length > 0) {
          // Collapse filter: filter:
          const len = query.bool.filter.length;
          const newItems = newArray(query.bool.filter, 'filter');
          query.bool.filter = newItems;
          if (newItems.length !== len) {
            ViewerUtils.#collapseQuery(query);
          } else {
            ViewerUtils.#collapseQuery(query.bool.filter);
          }
        } else {
          // Just recurse
          ViewerUtils.#collapseQuery(query.bool);
        }
      } else if (typeof query[prop] === 'object') {
        ViewerUtils.#collapseQuery(query[prop]);
      }
    }
  };

  // ----------------------------------------------------------------------------
  static continueBuildQuery (req, query, err, finalCb, queryOverride = null) {
    // queryOverride can supercede req.query if specified
    const reqQuery = queryOverride || req.query;

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

    ViewerUtils.lookupQueryItems(query.query.bool.filter, async (lerr) => {
      req._arkimeESQuery = JSON.stringify(query);

      if (reqQuery.date === '-1' || // An all query
          Config.get('queryAllIndices', Config.get('multiES', false))) { // queryAllIndices (default: multiES)
        req._arkimeESQueryIndices = Db.fixIndex(Db.getSessionIndices());
        return finalCb(err || lerr, query, req._arkimeESQueryIndices); // Then we just go against all indices for a slight overhead
      }

      const indices = await Db.getIndices(reqQuery.startTime, reqQuery.stopTime, reqQuery.bounding, Config.get('rotateIndex', 'daily'), Config.getArray('queryExtraIndices', ''));

      if (indices.length > 3000) { // Will url be too long
        req._arkimeESQueryIndices = Db.fixIndex(Db.getSessionIndices());
        return finalCb(err || lerr, query, req._arkimeESQueryIndices);
      } else {
        req._arkimeESQueryIndices = indices;
        return finalCb(err || lerr, query, indices);
      }
    });
  };

  // ----------------------------------------------------------------------------
  static mapMerge (aggregations) {
    const map = { src: {}, dst: {}, xffGeo: {} };

    if (!aggregations || !aggregations.mapG1) {
      return {};
    }

    aggregations.mapG1.buckets.forEach(function (item) {
      map.src[item.key] = item.doc_count;
    });

    aggregations.mapG2.buckets.forEach(function (item) {
      map.dst[item.key] = item.doc_count;
    });

    aggregations.mapG3.buckets.forEach(function (item) {
      map.xffGeo[item.key] = item.doc_count;
    });

    return map;
  };

  // ----------------------------------------------------------------------------
  static graphMerge (req, query, aggregations) {
    let filters = req.user.settings.timelineDataFilters || internals.settingDefaults.timelineDataFilters;

    // Convert old names to names locally
    filters = filters.map(x => {
      if (x === 'totPackets') return 'network.packets';
      if (x === 'totBytes') return 'network.bytes';
      return x;
    });

    const graph = {
      xmin: req.query.startTime * 1000 || null,
      xmax: req.query.stopTime * 1000 || null,
      interval: query.aggregations ? query.aggregations.dbHisto.histogram.interval / 1000 || 60 : 60,
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

    for (let i = 0; i < filters.length; i++) {
      const filter = filters[i];
      if (filtersMap[filter] !== undefined) {
        for (const j of filtersMap[filter]) {
          graph[j + 'Histo'] = [];
        }
      } else {
        graph[filter + 'Histo'] = [];
      }

      graph[filters[i] + 'Total'] = 0;
    }

    if (!aggregations || !aggregations.dbHisto) {
      return graph;
    }

    aggregations.dbHisto.buckets.forEach(function (item) {
      const key = item.key;

      // always add session information
      graph.sessionsHisto.push([key, item.doc_count]);
      graph.sessionsTotal += item.doc_count;

      for (const prop in item) {
        // excluding every item prop that isnt a summed up aggregate collection (ie. es keys)
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
    });

    return graph;
  };

  // ----------------------------------------------------------------------------
  static fixFields (fields, fixCb) {
    if (!fields.fileId) {
      fields.fileId = [];
      return fixCb(null, fields);
    }

    const files = [];
    async.forEachSeries(fields.fileId, function (item, cb) {
      Db.fileIdToFile(fields.node, item, function (file) {
        if (file && file.locked === 1) {
          files.push(file.name);
        }
        cb(null);
      });
    },
    function (err) {
      fields.fileId = files;
      fixCb(err, fields);
    });
  };

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

    if (str.match('IndexMissingException')) {
      return "Arkime's OpenSearch/Elasticsearch database has no matching session indices for the timeframe selected.";
    } else {
      return 'OpenSearch/Elasticsearch error: ' + str;
    }
  };

  // ----------------------------------------------------------------------------
  static async loadFields () {
    try {
      let data = await Db.loadFields();
      data = data.hits.hits;

      // Everything will use fieldECS or dbField2 as dbField
      for (let i = 0, ilen = data.length; i < ilen; i++) {
        if (data[i]._source.fieldECS) {
          internals.oldDBFields.set(data[i]._source.dbField, data[i]._source);
          internals.oldDBFields.set(data[i]._source.dbField2, data[i]._source);
          data[i]._source.dbField = data[i]._source.fieldECS;
        } else {
          internals.oldDBFields.set(data[i]._source.dbField, data[i]._source);
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
          return (a.exp > b.exp ? 1 : -1);
        })
      };
    } catch (err) {
      return { fieldsMap: {}, fieldsArr: [] };
    }
  };

  // ----------------------------------------------------------------------------
  static oldDB2newDB (x) {
    const old = internals.oldDBFields.get(x);

    if (old === undefined) { return x; }
    return old.dbFieldECS ?? old.dbField2;
  };

  // ----------------------------------------------------------------------------
  static mergeUnarray (to, from) {
    for (const key in from) {
      if (Array.isArray(from[key])) {
        to[key] = from[key][0];
      } else {
        to[key] = from[key];
      }
    }
  };

  // ----------------------------------------------------------------------------
  // https://medium.com/dailyjs/rewriting-javascript-converting-an-array-of-objects-to-an-object-ec579cafbfc7
  static arrayToObject (array, key) {
    return array.reduce((obj, item) => {
      obj[item[key]] = item;
      return obj;
    }, {});
  };

  // ----------------------------------------------------------------------------
  static arrayZeroFill (n) {
    const a = [];

    while (n > 0) {
      a.push(0);
      n--;
    }

    return a;
  };

  // ----------------------------------------------------------------------------
  static getViewUrl (node, cb) {
    if (Array.isArray(node)) {
      node = node[0];
    }

    const url = Config.getFull(node, 'viewUrl');
    if (url) {
      if (Config.debug > 1) {
        console.log(`DEBUG: node:${node} is using ${url} because viewUrl was set for ${node} in config file`);
      }
      cb(null, url, url.slice(0, 5) === 'https' ? https : http);
      return;
    }

    Db.arkimeNodeStatsCache(node, function (err, stat) {
      if (err) {
        return cb(err);
      }

      if (Config.debug > 1) {
        console.log(`DEBUG: node:${node} is using ${stat.hostname} from OpenSearch/Elasticsearch stats index`);
      }

      if (Config.isHTTPS(node)) {
        cb(null, 'https://' + stat.hostname + ':' + Config.getFull(node, 'viewPort', '8005'), https);
      } else {
        cb(null, 'http://' + stat.hostname + ':' + Config.getFull(node, 'viewPort', '8005'), http);
      }
    });
  };

  // ----------------------------------------------------------------------------
  static makeRequest (node, path, user, cb) {
    ViewerUtils.getViewUrl(node, (err, viewUrl, client) => {
      if (err) {
        return cb(err);
      }
      const nodePath = encodeURI(`${Config.basePath(node)}${path}`);
      const url = new URL(nodePath, viewUrl);
      const options = {
        timeout: 20 * 60 * 1000,
        agent: client === http ? internals.httpAgent : internals.httpsAgent
      };

      Auth.addS2SAuth(options, user, node, nodePath);
      ViewerUtils.addCaTrust(options, node);

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
        console.log(`Retry ${url.path} on remote viewer: ${err}`);
        const preq2 = client.request(url, options, responseFunc);
        preq2.on('error', (err) => {
          console.log(`Error with ${url.path} on remote viewer: ${err}`);
          cb(err);
        });
        preq2.end();
      });
      preq.end();
    });
  };

  // ----------------------------------------------------------------------------
  static addCluster (cluster, options) {
    if (!options) options = {};
    if (cluster && Config.get('multiES', false)) {
      options.cluster = cluster;
    }
    return options;
  };

  // ----------------------------------------------------------------------------
  // check for anonymous mode before fetching user cache and return anonymous
  // user or the user requested by the userId
  static getUserCacheIncAnon (userId, cb) {
    if (Auth.isAnonymousMode()) { // user is anonymous
      User.getUserCache('anonymous', (err, anonUser) => {
        const anon = Object.assign(new User(), internals.anonymousUser);

        if (anonUser) {
          anon.settings = anonUser.settings || {};
        }

        return cb(null, anon);
      });
    } else {
      User.getUserCache(userId, cb);
    }
  };
}

module.exports = ViewerUtils;
