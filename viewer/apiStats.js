'use strict';

const RE2 = require('re2');
const util = require('util');
const async = require('async');

module.exports = (Config, Db, internals, ViewerUtils) => {
  let module = {};

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  /**
   * GET - /api/stats
   *
   * Fetches a list of stats for each node in the cluster.
   * @name stats
   * @param {string} filter - Search text to filter the list of nodes by.
   * @param {number} length=500 - The number of nodes to return. Defaults to 500.
   * @param {number} start=0 - The entry to start at. Defaults to 0.
   * @param {string} sortField=nodeName - The field to sort the node list by.
   * @param {string} desc=false - Whether to return the results in descending order. Defaults to "false".
   * @param {string} hide - Which nodes to exclude from the results. Options include:
     none - show all nodes.
     old - hide out of date nodes (nodes whose current time is behind by at least 5 minutes).
     nosession - hide nodes without sessions.
     both - hide out of date nodes and nodes without sessions.
   * @returns {array} data - List of nodes with their corresponding stats.
   * @returns {number} recordsTotal - The total number of nodes.
   * @returns {number} recordsFiltered - The number of nodes returned in this result.
   */
  module.getStats = (req, res) => {
    let query = {
      from: 0,
      size: 10000,
      query: {
        bool: {
          must: [],
          should: [],
          must_not: [
            { term: { hide: true } }
          ]
        }
      }
    };

    if (req.query.filter !== undefined && req.query.filter !== '') {
      const names = req.query.filter.split(',');
      for (let name of names) {
        name = name.trim();
        if (name !== '') {
          if (name.endsWith('$')) { name = name.slice(0, -1); } else { name += '.*'; }
          query.query.bool.should.push({
            regexp: { nodeName: name }
          });
        }
      }
    }

    let rquery = {
      query: { term: { locked: 0 } },
      size: 0,
      aggregations: {
        buckets: {
          terms: { field: 'node', size: 1000 },
          aggregations: {
            first: { min: { field: 'first' } }
          }
        }
      }
    };

    if (req.query.hide !== undefined && req.query.hide !== 'none') {
      if (req.query.hide === 'old' || req.query.hide === 'both') {
        query.query.bool.must.push({ range: { currentTime: { gte: 'now-5m' } } });
      }
      if (req.query.hide === 'nosession' || req.query.hide === 'both') {
        query.query.bool.must.push({ range: { monitoring: { gte: '1' } } });
      }
    }

    let now = Math.floor(Date.now() / 1000);

    Promise.all([Db.search('stats', 'stat', query),
      Db.numberOfDocuments('stats'),
      Db.search('files', 'file', rquery)
    ]).then(([stats, total, retention]) => {
      if (stats.error) { throw stats.error; }

      if (retention.aggregations.buckets && retention.aggregations.buckets.buckets) {
        retention = ViewerUtils.arrayToObject(retention.aggregations.buckets.buckets, 'key');
      } else {
        retention = {};
      }

      let results = { total: stats.hits.total, results: [] };

      for (let i = 0, ilen = stats.hits.hits.length; i < ilen; i++) {
        let fields = stats.hits.hits[i]._source || stats.hits.hits[i].fields;
        if (stats.hits.hits[i]._source) {
          ViewerUtils.mergeUnarray(fields, stats.hits.hits[i].fields);
        }
        fields.id = stats.hits.hits[i]._id;

        if (retention[fields.id]) {
          fields.retention = now - retention[fields.id].first.value;
        } else {
          fields.retention = 0;
        }

        fields.deltaBytesPerSec = Math.floor(fields.deltaBytes * 1000.0 / fields.deltaMS);
        fields.deltaWrittenBytesPerSec = Math.floor(fields.deltaWrittenBytes * 1000.0 / fields.deltaMS);
        fields.deltaUnwrittenBytesPerSec = Math.floor(fields.deltaUnwrittenBytes * 1000.0 / fields.deltaMS);
        fields.deltaBitsPerSec = Math.floor(fields.deltaBytes * 1000.0 / fields.deltaMS * 8);
        fields.deltaPacketsPerSec = Math.floor(fields.deltaPackets * 1000.0 / fields.deltaMS);
        fields.deltaSessionsPerSec = Math.floor(fields.deltaSessions * 1000.0 / fields.deltaMS);
        fields.deltaSessionBytesPerSec = Math.floor(fields.deltaSessionBytes * 1000.0 / fields.deltaMS);
        fields.sessionSizePerSec = Math.floor(fields.deltaSessionBytes / fields.deltaSessions);
        fields.deltaDroppedPerSec = Math.floor(fields.deltaDropped * 1000.0 / fields.deltaMS);
        fields.deltaFragsDroppedPerSec = Math.floor(fields.deltaFragsDropped * 1000.0 / fields.deltaMS);
        fields.deltaOverloadDroppedPerSec = Math.floor(fields.deltaOverloadDropped * 1000.0 / fields.deltaMS);
        fields.deltaESDroppedPerSec = Math.floor(fields.deltaESDropped * 1000.0 / fields.deltaMS);
        fields.deltaDupDroppedPerSec = Math.floor(fields.deltaDupDropped * 1000.0 / fields.deltaMS) || 0;
        fields.deltaTotalDroppedPerSec = Math.floor((fields.deltaDropped + fields.deltaOverloadDropped) * 1000.0 / fields.deltaMS);
        fields.runningTime = fields.currentTime - fields.startTime;
        results.results.push(fields);
      }

      // sort after all the results are aggregated
      req.query.sortField = req.query.sortField || 'nodeName';
      if (results.results[0] && results.results[0][req.query.sortField] !== undefined) { // make sure the field exists to sort on
        results.results = results.results.sort((a, b) => {
          if (req.query.desc === 'true') {
            if (!isNaN(a[req.query.sortField])) {
              return b[req.query.sortField] - a[req.query.sortField];
            } else {
              return b[req.query.sortField].localeCompare(a[req.query.sortField]);
            }
          } else {
            if (!isNaN(a[req.query.sortField])) {
              return a[req.query.sortField] - b[req.query.sortField];
            } else {
              return a[req.query.sortField].localeCompare(b[req.query.sortField]);
            }
          }
        });
      }

      let from = +req.query.start || 0;
      let stop = from + (+req.query.length || 500);

      let r = {
        recordsTotal: total.count,
        recordsFiltered: results.results.length,
        data: results.results.slice(from, stop)
      };

      res.send(r);
    }).catch((err) => {
      console.log('ERROR - /stats.json', query, err);
      res.send({ recordsTotal: 0, recordsFiltered: 0, data: [] });
    });
  };

  /**
   * GET - /api/dstats
   *
   * Fetches a list of detailed stats for different fields pertaining to a node to populate a cubism graph.
   * <a href="https://github.com/square/cubism">Cubism GitHub</a>
   * @name dstats
   * @param {string} nodeName - The name of the node to get the detailed stats for.
   * @param {string} name - The name of the field to get the detailed stats for.
   * @param {number} start - The start time of data to return. Format is seconds since Unix EPOC.
   * @param {number} stop  - The stop time of data to return. Format is seconds since Unix EPOC.
   * @param {number} step - The context step of the cubism graph in milliseconds.
   * @param {number} interval=60 - The time interval to search for.
   * @param {number} size=1440 - The size of the cubism graph. Defaults to 1440.
   * @returns {array} List of values to populate the cubism graph.
   */
  module.getDetailedStats = (req, res) => {
    const nodeName = req.query.nodeName;

    let query = {
      query: {
        bool: {
          filter: [
            {
              range: { currentTime: { from: req.query.start, to: req.query.stop } }
            },
            {
              term: { interval: req.query.interval || 60 }
            }
          ]
        }
      }
    };

    if (nodeName !== undefined && nodeName !== 'Total' && nodeName !== 'Average') {
      query.sort = { currentTime: { order: 'desc' } };
      query.size = req.query.size || 1440;
      query.query.bool.filter.push({ term: { nodeName: nodeName } });
    } else {
      query.size = 100000;
    }

    const mapping = {
      deltaBits: { _source: ['deltaBytes'], func: function (item) { return Math.floor(item.deltaBytes * 8.0); } },
      deltaTotalDropped: { _source: ['deltaDropped', 'deltaOverloadDropped'], func: function (item) { return Math.floor(item.deltaDropped + item.deltaOverloadDropped); } },
      deltaBytesPerSec: { _source: ['deltaBytes', 'deltaMS'], func: function (item) { return Math.floor(item.deltaBytes * 1000.0 / item.deltaMS); } },
      deltaBitsPerSec: { _source: ['deltaBytes', 'deltaMS'], func: function (item) { return Math.floor(item.deltaBytes * 1000.0 / item.deltaMS * 8); } },
      deltaWrittenBytesPerSec: { _source: ['deltaWrittenBytes', 'deltaMS'], func: function (item) { return Math.floor(item.deltaWrittenBytes * 1000.0 / item.deltaMS); } },
      deltaUnwrittenBytesPerSec: { _source: ['deltaUnwrittenBytes', 'deltaMS'], func: function (item) { return Math.floor(item.deltaUnwrittenBytes * 1000.0 / item.deltaMS); } },
      deltaPacketsPerSec: { _source: ['deltaPackets', 'deltaMS'], func: function (item) { return Math.floor(item.deltaPackets * 1000.0 / item.deltaMS); } },
      deltaSessionsPerSec: { _source: ['deltaSessions', 'deltaMS'], func: function (item) { return Math.floor(item.deltaSessions * 1000.0 / item.deltaMS); } },
      deltaSessionBytesPerSec: { _source: ['deltaSessionBytes', 'deltaMS'], func: function (item) { return Math.floor(item.deltaSessionBytes * 1000.0 / item.deltaMS); } },
      sessionSizePerSec: { _source: ['deltaSessionBytes', 'deltaSessions'], func: function (item) { return Math.floor(item.deltaSessionBytes / item.deltaSessions); } },
      deltaDroppedPerSec: { _source: ['deltaDropped', 'deltaMS'], func: function (item) { return Math.floor(item.deltaDropped * 1000.0 / item.deltaMS); } },
      deltaFragsDroppedPerSec: { _source: ['deltaFragsDropped', 'deltaMS'], func: function (item) { return Math.floor(item.deltaFragsDropped * 1000.0 / item.deltaMS); } },
      deltaOverloadDroppedPerSec: { _source: ['deltaOverloadDropped', 'deltaMS'], func: function (item) { return Math.floor(item.deltaOverloadDropped * 1000.0 / item.deltaMS); } },
      deltaESDroppedPerSec: { _source: ['deltaESDropped', 'deltaMS'], func: function (item) { return Math.floor(item.deltaESDropped * 1000.0 / item.deltaMS); } },
      deltaDupDroppedPerSec: { _source: ['deltaDupDropped', 'deltaMS'], func: function (item) { return Math.floor(item.deltaDupDropped * 1000.0 / item.deltaMS); } },
      deltaTotalDroppedPerSec: { _source: ['deltaDropped', 'deltaOverloadDropped', 'deltaMS'], func: function (item) { return Math.floor((item.deltaDropped + item.deltaOverloadDropped) * 1000.0 / item.deltaMS); } },
      cpu: { _source: ['cpu'], func: function (item) { return item.cpu * 0.01; } }
    };

    query._source = mapping[req.query.name] ? mapping[req.query.name]._source : [req.query.name];
    query._source.push('nodeName', 'currentTime');

    const func = mapping[req.query.name] ? mapping[req.query.name].func : function (item) { return item[req.query.name]; };

    Db.searchScroll('dstats', 'dstat', query, { filter_path: '_scroll_id,hits.total,hits.hits._source' }, (err, result) => {
      if (err || result.error) {
        console.log('ERROR - dstats', query, err || result.error);
      }

      let i, ilen;
      let data = {};
      const num = (req.query.stop - req.query.start) / req.query.step;

      let mult = 1;
      if (req.query.name === 'freeSpaceM' || req.query.name === 'usedSpaceM') {
        mult = 1000000;
      }

      if (Config.debug > 2) {
        console.log('dstats.json result', util.inspect(result, false, 50));
      }

      if (result && result.hits && result.hits.hits) {
        for (i = 0, ilen = result.hits.hits.length; i < ilen; i++) {
          const fields = result.hits.hits[i]._source;
          const pos = Math.floor((fields.currentTime - req.query.start) / req.query.step);

          if (data[fields.nodeName] === undefined) {
            data[fields.nodeName] = ViewerUtils.arrayZeroFill(num);
          }
          data[fields.nodeName][pos] = mult * func(fields);
        }
      }

      if (nodeName === undefined) {
        res.send(data);
      } else {
        if (data[nodeName] === undefined) {
          data[nodeName] = ViewerUtils.arrayZeroFill(num);
        }
        if (nodeName === 'Total' || nodeName === 'Average') {
          delete data[nodeName];
          let data2 = ViewerUtils.arrayZeroFill(num);
          let cnt = 0;
          for (const key in data) {
            for (i = 0; i < num; i++) {
              data2[i] += data[key][i];
            }
            cnt++;
          }
          if (nodeName === 'Average') {
            for (i = 0; i < num; i++) {
              data2[i] /= cnt;
            }
          }
          res.send(data2);
        } else {
          res.send(data[req.query.nodeName]);
        }
      }
    });
  };

  /**
   * GET - /api/esstats
   *
   * Fetches a list of stats for each elasticsearch cluster.
   * @name esstats
   * @param {string} filter - Search text to filter the list of elasticsearch clusters by.
   * @param {string} sortField=nodeName - The field to sort the elasticsearch clusters list by.
   * @param {string} desc=false - Whether to return the results in descending order. Defaults to "false".
   * @returns {array} data - List of es clusters with their corresponding stats.
   * @returns {number} recordsTotal - The total number of es clusters.
   * @returns {number} recordsFiltered - The number of es clusters returned in this result.
   * @returns {ESHealth} health - The elasticsearch cluster health status and info.
   */
  module.getESStats = (req, res) => {
    let stats = [];
    let r;

    Promise.all([Db.nodesStatsCache(),
      Db.nodesInfoCache(),
      Db.masterCache(),
      Db.healthCachePromise(),
      Db.allocation(),
      Db.getClusterSettings({ flatSettings: true })
    ]).then(([nodesStats, nodesInfo, master, health, allocation, settings]) => {
      const shards = new Map(allocation.map(i => [i.node, parseInt(i.shards, 10)]));

      let ipExcludes = [];
      if (settings.persistent['cluster.routing.allocation.exclude._ip']) {
        ipExcludes = settings.persistent['cluster.routing.allocation.exclude._ip'].split(',');
      }

      let nodeExcludes = [];
      if (settings.persistent['cluster.routing.allocation.exclude._name']) {
        nodeExcludes = settings.persistent['cluster.routing.allocation.exclude._name'].split(',');
      }

      const now = new Date().getTime();
      while (internals.previousNodesStats.length > 1 && internals.previousNodesStats[1].timestamp + 10000 < now) {
        internals.previousNodesStats.shift();
      }

      let regex;
      if (req.query.filter !== undefined) {
        try {
          regex = new RE2(req.query.filter);
        } catch (e) {
          return res.molochError(500, `Regex Error: ${e}`);
        }
      }

      const nodeKeys = Object.keys(nodesStats.nodes);
      for (let n = 0, nlen = nodeKeys.length; n < nlen; n++) {
        let node = nodesStats.nodes[nodeKeys[n]];

        if (nodeKeys[n] === 'timestamp' || (regex && !node.name.match(regex))) { continue; }

        let read = 0;
        let write = 0;
        let rejected = 0;
        let completed = 0;

        let writeInfo = node.thread_pool.bulk || node.thread_pool.write;

        const oldnode = internals.previousNodesStats[0][nodeKeys[n]];
        if (oldnode !== undefined && node.fs.io_stats !== undefined && oldnode.fs.io_stats !== undefined && 'total' in node.fs.io_stats) {
          const timediffsec = (node.timestamp - oldnode.timestamp) / 1000.0;
          read = Math.max(0, Math.ceil((node.fs.io_stats.total.read_kilobytes - oldnode.fs.io_stats.total.read_kilobytes) / timediffsec * 1024));
          write = Math.max(0, Math.ceil((node.fs.io_stats.total.write_kilobytes - oldnode.fs.io_stats.total.write_kilobytes) / timediffsec * 1024));

          let writeInfoOld = oldnode.thread_pool.bulk || oldnode.thread_pool.write;

          completed = Math.max(0, Math.ceil((writeInfo.completed - writeInfoOld.completed) / timediffsec));
          rejected = Math.max(0, Math.ceil((writeInfo.rejected - writeInfoOld.rejected) / timediffsec));
        }

        const ip = (node.ip ? node.ip.split(':')[0] : node.host);

        let threadpoolInfo;
        let version = '';
        let molochtype;
        let molochzone;
        if (nodesInfo.nodes[nodeKeys[n]]) {
          threadpoolInfo = nodesInfo.nodes[nodeKeys[n]].thread_pool.bulk || nodesInfo.nodes[nodeKeys[n]].thread_pool.write;
          version = nodesInfo.nodes[nodeKeys[n]].version;
          if (nodesInfo.nodes[nodeKeys[n]].attributes) {
            molochtype = nodesInfo.nodes[nodeKeys[n]].attributes.molochtype;
            molochzone = nodesInfo.nodes[nodeKeys[n]].attributes.molochzone;
          }
        } else {
          threadpoolInfo = { queue_size: 0 };
        }

        stats.push({
          name: node.name,
          ip: ip,
          ipExcluded: ipExcludes.includes(ip),
          nodeExcluded: nodeExcludes.includes(node.name),
          storeSize: node.indices.store.size_in_bytes,
          freeSize: node.roles.includes('data') ? node.fs.total.available_in_bytes : 0,
          docs: node.indices.docs.count,
          searches: node.indices.search.query_current,
          searchesTime: node.indices.search.query_time_in_millis,
          heapSize: node.jvm.mem.heap_used_in_bytes,
          nonHeapSize: node.jvm.mem.non_heap_used_in_bytes,
          cpu: node.process.cpu.percent,
          read: read,
          write: write,
          writesRejected: writeInfo.rejected,
          writesCompleted: writeInfo.completed,
          writesRejectedDelta: rejected,
          writesCompletedDelta: completed,
          writesQueueSize: threadpoolInfo.queue_size,
          load: node.os.load_average !== undefined ? /* ES 2 */ node.os.load_average : /* ES 5 */ node.os.cpu.load_average['5m'],
          version: version,
          molochtype: molochtype,
          molochzone: molochzone,
          roles: node.roles,
          isMaster: (master.length > 0 && node.name === master[0].node),
          shards: shards.get(node.name) || 0,
          segments: node.indices.segments.count || 0
        });
      }

      if (req.query.sortField && stats.length > 1) {
        let field = req.query.sortField === 'nodeName' ? 'name' : req.query.sortField;
        if (typeof (stats[0][field]) === 'string') {
          if (req.query.desc === 'true') {
            stats = stats.sort((a, b) => { return b[field].localeCompare(a[field]); });
          } else {
            stats = stats.sort((a, b) => { return a[field].localeCompare(b[field]); });
          }
        } else {
          if (req.query.desc === 'true') {
            stats = stats.sort((a, b) => { return b[field] - a[field]; });
          } else {
            stats = stats.sort((a, b) => { return a[field] - b[field]; });
          }
        }
      }

      nodesStats.nodes.timestamp = new Date().getTime();
      internals.previousNodesStats.push(nodesStats.nodes);

      r = {
        health: health,
        recordsTotal: (nodeKeys.includes('timestamp')) ? nodeKeys.length - 1 : nodeKeys.length,
        recordsFiltered: stats.length,
        data: stats
      };

      res.send(r);
    }).catch((err) => {
      console.log('ERROR -  /esstats.json', err);
      r = {
        health: Db.healthCache(),
        recordsTotal: 0,
        recordsFiltered: 0,
        data: []
      };
      return res.send(r);
    });
  };

  /**
   * GET - /api/esindices
   *
   * Fetches a list of elasticsearch indices.
   * @name esindices
   * @param {string} filter - Search text to filter the list of elasticsearch indices by.
   * @param {string} sortField=index - The field to sort the elasticsearch indices list by.
   * @param {string} desc=false - Whether to return the results in descending order. Defaults to "false".
   * @returns {array} data - List of es indices with their corresponding stats.
   * @returns {number} recordsTotal - The total number of es indices.
   * @returns {number} recordsFiltered - The number of es indices returned in this result.
   */
  module.getESIndices = (req, res) => {
    async.parallel({
      indices: Db.indicesCache,
      indicesSettings: Db.indicesSettingsCache
    }, (err, results) => {
      if (err) {
        console.log('ERROR -  /esindices/list', err);
        return res.send({
          recordsTotal: 0,
          recordsFiltered: 0,
          data: []
        });
      }

      const indices = results.indices;
      const indicesSettings = results.indicesSettings;

      let findices = [];

      // filtering
      if (req.query.filter !== undefined) {
        try {
          const regex = new RE2(req.query.filter);
          for (const index of indices) {
            if (!index.index.match(regex)) { continue; }
            findices.push(index);
          }
        } catch (e) {
          return res.molochError(500, `Regex Error: ${e}`);
        }
      } else {
        findices = indices;
      }

      // Add more fields from indicesSettings
      for (const index of findices) {
        if (!indicesSettings[index.index]) { continue; }

        if (indicesSettings[index.index].settings['index.routing.allocation.require.molochtype']) {
          index.molochtype = indicesSettings[index.index].settings['index.routing.allocation.require.molochtype'];
        }

        if (indicesSettings[index.index].settings['index.routing.allocation.total_shards_per_node']) {
          index.shardsPerNode = indicesSettings[index.index].settings['index.routing.allocation.total_shards_per_node'];
        }

        index.creationDate = parseInt(indicesSettings[index.index].settings['index.creation_date']);
        index.versionCreated = parseInt(indicesSettings[index.index].settings['index.version.created']);
        index.docSize = index['docs.count'] === '0' ? 0 : Math.ceil(parseInt(index['store.size']) / parseInt(index['docs.count']));
      }

      // sorting
      const sortField = req.query.sortField || 'index';
      if (sortField === 'index' || sortField === 'status' || sortField === 'health') {
        if (req.query.desc === 'true') {
          findices = findices.sort((a, b) => { return b[sortField].localeCompare(a[sortField]); });
        } else {
          findices = findices.sort((a, b) => { return a[sortField].localeCompare(b[sortField]); });
        }
      } else {
        if (req.query.desc === 'true') {
          findices = findices.sort((a, b) => { return b[sortField] - a[sortField]; });
        } else {
          findices = findices.sort((a, b) => { return a[sortField] - b[sortField]; });
        }
      }

      // send result
      return res.send({
        recordsTotal: indices.length,
        recordsFiltered: findices.length,
        data: findices
      });
    });
  };

  /**
   * DELETE - /api/esindices/:index
   *
   * Deletes an elasticsearch index.
   * @name esindices/:index
   * @returns {boolean} success - Whether the delete index operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.deleteESIndex = (req, res) => {
    Db.deleteIndex([req.params.index], {}, (err, result) => {
      if (err) {
        res.status(404);
        return res.send(JSON.stringify({
          success: false,
          text: 'Error deleting index'
        }));
      }
      return res.send(JSON.stringify({ success: true, text: result }));
    });
  };

  /**
   * POST - /api/esindices/:index/optimize
   *
   * Optimizes an elasticsearch index.
   * @name esindices/:index/optimize
   * @returns {boolean} success - Always true, the optimizeIndex function might block. Check the logs for errors.
   */
  module.optimizeESIndex = (req, res) => {
    Db.optimizeIndex([req.params.index], {}, (err, result) => {
      if (err) {
        console.log('ERROR -', req.params.index, 'optimize failed', err);
      }
    });

    // always return right away, optimizeIndex might block
    return res.send(JSON.stringify({ success: true }));
  };

  /**
   * POST - /api/esindices/:index/close
   *
   * Closes an elasticsearch index.
   * @name esindices/:index/close
   * @returns {boolean} success - Whether the close index operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.closeESIndex = (req, res) => {
    Db.closeIndex([req.params.index], {}, (err, result) => {
      if (err) {
        res.status(404);
        return res.send(JSON.stringify({
          success: false,
          text: 'Error closing index'
        }));
      }
      return res.send(JSON.stringify({ success: true, text: result }));
    });
  };

  /**
   * POST - /api/esindices/:index/open
   *
   * Opens an elasticsearch index.
   * @name esindices/:index/open
   * @returns {boolean} success - Always true, the openIndex function might block. Check the logs for errors.
   */
  module.openESIndex = (req, res) => {
    Db.openIndex([req.params.index], {}, (err, result) => {
      if (err) {
        console.log('ERROR -', req.params.index, 'open failed', err);
      }
    });

    // always return right away, openIndex might block
    return res.send(JSON.stringify({ success: true, text: {} }));
  };

  /**
   * POST - /api/esindices/:index/shrink
   *
   * Shrinks an elasticsearch index.
   * @name esindices/:index/shrink
   * @param {string} target - TODO
   * @param {number} numShards - TODO
   * @returns {boolean} success - Whether the close shrink operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.shrinkESIndex = (req, res) => {
    if (!req.body || !req.body.target) {
      return res.molochError(403, 'Missing target');
    }

    let settingsParams = {
      body: {
        'index.routing.allocation.total_shards_per_node': null,
        'index.routing.allocation.require._name': req.body.target,
        'index.blocks.write': true
      }
    };

    Db.setIndexSettings(req.params.index, settingsParams, (err, results) => {
      if (err) {
        return res.send(JSON.stringify({
          success: false,
          text: err.message || 'Error shrinking index'
        }));
      }

      let shrinkParams = {
        body: {
          settings: {
            'index.routing.allocation.require._name': null,
            'index.blocks.write': null,
            'index.codec': 'best_compression',
            'index.number_of_shards': req.body.numShards || 1
          }
        }
      };

      // wait for no more reloacting shards
      let shrinkCheckInterval = setInterval(() => {
        Db.healthCachePromise()
          .then((result) => {
            if (result.relocating_shards === 0) {
              clearInterval(shrinkCheckInterval);
              Db.shrinkIndex(req.params.index, shrinkParams, (err, results) => {
                if (err) {
                  console.log(`ERROR - ${req.params.index} shrink failed`, err);
                }
                Db.indices((err, indexResult) => {
                  if (err) {
                    console.log(`Error fetching ${req.params.index} and ${req.params.index}-shrink indices after shrinking`);
                  } else if (indexResult[0] && indexResult[1] &&
                    indexResult[0]['docs.count'] === indexResult[1]['docs.count']) {
                    Db.deleteIndex([req.params.index], {}, (err, result) => {
                      if (err) {
                        console.log(`Error deleting ${req.params.index} index after shrinking`);
                      }
                    });
                  }
                }, `${req.params.index}-shrink,${req.params.index}`);
              });
            }
          });
      }, 10000);

      // always return right away, shrinking might take a while
      return res.send(JSON.stringify({ success: true }));
    });
  };

  return module;
};
