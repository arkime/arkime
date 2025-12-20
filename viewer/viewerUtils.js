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
const ArkimeUtil = require('../common/arkimeUtil');
const Auth = require('../common/auth');
const User = require('../common/user');
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

    for (const item of aggregations.dbHisto.buckets) {
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
    }

    return graph;
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
      if (!Object.prototype.hasOwnProperty.call(from, key)) { continue; }
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
  static async getViewUrl (node, cb) {
    if (Array.isArray(node)) {
      node = node[0];
    }

    const url = Config.getFull(node, 'viewUrl');
    if (url) {
      if (Config.debug > 1) {
        console.log(`DEBUG: node:${node} is using ${url} because viewUrl was set for ${node} in config file`);
      }
      if (cb) {
        cb(null, url, url.slice(0, 5) === 'https' ? https : http);
      } else {
        return { viewUrl: url, client: url.slice(0, 5) === 'https' ? https : http };
      }
      return;
    }

    try {
      const stat = await Db.arkimeNodeStatsCache(node);

      if (Config.debug > 1) {
        console.log(`DEBUG: node:${node} is using ${stat.hostname} from OpenSearch/Elasticsearch stats index`);
      }

      if (Config.isHTTPS(node)) {
        const result = 'https://' + stat.hostname + ':' + Config.getFull(node, 'viewPort', '8005');
        if (cb) {
          cb(null, result, https);
        } else {
          return { viewUrl: result, client: https };
        }
      } else {
        const result = 'http://' + stat.hostname + ':' + Config.getFull(node, 'viewPort', '8005');
        if (cb) {
          cb(null, result, http);
        } else {
          return { viewUrl: result, client: http };
        }
      }
    } catch (err) {
      if (cb) {
        return cb(err);
      } else {
        throw err;
      }
    }
  };

  // ----------------------------------------------------------------------------
  static makeRequest (node, path, user, cb) {
    ViewerUtils.getViewUrl(node, (err, viewUrl, client) => {
      if (err) {
        return cb(err);
      }
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

      Auth.addS2SAuth(options, user, node, url.pathname);
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
  static async getUserCacheIncAnon (userId) {
    if (Auth.isAnonymousMode()) { // user is anonymous
      const anonUser = await User.getUserCache('anonymous');
      const anon = Object.assign(new User(), internals.anonymousUser);

      if (anonUser) {
        anon.settings = anonUser.settings || {};
      }

      return anon;
    } else {
      return await User.getUserCache(userId);
    }
  };
}

module.exports = ViewerUtils;
