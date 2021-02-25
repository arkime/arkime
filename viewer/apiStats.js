'use strict';

const RE2 = require('re2');
const util = require('util');
const async = require('async');

module.exports = (Config, Db, internals, ViewerUtils) => {
  const module = {};

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  // ES HEALTH APIS -----------------------------------------------------------
  /**
   * The Elasticsearch cluster health status and information.
   * @typedef ESHealth
   * @type {object}
   * @property {number} active_primary_shards - The number of active primary shards.
   * @property {number} active_shards - The total number of active primary and replica shards.
   * @property {number} active_shards_percent_as_number - The ratio of active shards in the cluster expressed as a percentage.
   * @property {string} cluster_name - The name of the arkime cluster
   * @property {number} delayed_unassigned_shards - The number of shards whose allocation has been delayed by the timeout settings.
   * @property {number} initializing_shards - The number of shards that are under initialization.
   * @property {number} molochDbVersion - The arkime database version
   * @property {number} number_of_data_nodes - The number of nodes that are dedicated data nodes.
   * @property {number} number_of_in_flight_fetch - The number of unfinished fetches.
   * @property {number} number_of_nodes - The number of nodes within the cluster.
   * @property {number} number_of_pending_tasks - The number of cluster-level changes that have not yet been executed.
   * @property {number} relocating_shards - The number of shards that are under relocation.
   * @property {string} status - Health status of the cluster, based on the state of its primary and replica shards. Statuses are:
      "green" - All shards are assigned.
      "yellow" - All primary shards are assigned, but one or more replica shards are unassigned. If a node in the cluster fails, some data could be unavailable until that node is repaired.
      "red" - One or more primary shards are unassigned, so some data is unavailable. This can occur briefly during cluster startup as primary shards are assigned.
   * @property {number} task_max_waiting_in_queue_millis - The time expressed in milliseconds since the earliest initiated task is waiting for being performed.
   * @property {boolean} timed_out - If false the response returned within the period of time that is specified by the timeout parameter (30s by default).
   * @property {number} unassigned_shards - The number of shards that are not allocated.
   * @property {string} version - the elasticsearch version number
   * @property {number} _timeStamp - timestamps in ms from unix epoc
   */

  /**
   * GET - /api/eshealth
   *
   * Retrive Elasticsearch health and stats
   * There is no auth necessary to retrieve eshealth
   * @name /eshealth
   * @returns {ESHealth} health - The elasticsearch cluster health status and info
   */
  module.getESHealth = (req, res) => {
    Db.healthCache((err, health) => {
      res.send(health);
    });
  };

  // STATS APIS ---------------------------------------------------------------
  /**
   * GET - /api/stats
   *
   * Fetches a list of stats for each node in the cluster.
   * @name /stats
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
    const query = {
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

    const rquery = {
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

    const now = Math.floor(Date.now() / 1000);

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

      const results = { total: stats.hits.total, results: [] };

      for (let i = 0, ilen = stats.hits.hits.length; i < ilen; i++) {
        const fields = stats.hits.hits[i]._source || stats.hits.hits[i].fields;
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

      const from = +req.query.start || 0;
      const stop = from + (+req.query.length || 500);

      const r = {
        recordsTotal: total.count,
        recordsFiltered: results.results.length,
        data: results.results.slice(from, stop)
      };

      res.send(r);
    }).catch((err) => {
      console.log('ERROR - /api/stats', query, err);
      res.send({ recordsTotal: 0, recordsFiltered: 0, data: [] });
    });
  };

  /**
   * GET - /api/dstats
   *
   * Fetches a list of detailed stats for different fields pertaining to a node to populate a cubism graph.
   * <a href="https://github.com/square/cubism">Cubism GitHub</a>
   * @name /dstats
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

    const query = {
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
      const data = {};
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
          const data2 = ViewerUtils.arrayZeroFill(num);
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
   * Fetches a list of stats for each Elasticsearch cluster.
   * @name /esstats
   * @param {string} filter - Search text to filter the list of Elasticsearch clusters by.
   * @param {string} sortField=nodeName - The field to sort the Elasticsearch clusters list by.
   * @param {string} desc=false - Whether to return the results in descending order. Defaults to "false".
   * @returns {array} data - List of ES clusters with their corresponding stats.
   * @returns {number} recordsTotal - The total number of ES clusters.
   * @returns {number} recordsFiltered - The number of ES clusters returned in this result.
   * @returns {ESHealth} health - The Elasticsearch cluster health status and info.
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
        const node = nodesStats.nodes[nodeKeys[n]];

        if (nodeKeys[n] === 'timestamp' || (regex && !node.name.match(regex))) { continue; }

        let read = 0;
        let write = 0;
        let rejected = 0;
        let completed = 0;

        const writeInfo = node.thread_pool.bulk || node.thread_pool.write;

        const oldnode = internals.previousNodesStats[0][nodeKeys[n]];
        if (oldnode !== undefined && node.fs.io_stats !== undefined && oldnode.fs.io_stats !== undefined && 'total' in node.fs.io_stats) {
          const timediffsec = (node.timestamp - oldnode.timestamp) / 1000.0;
          read = Math.max(0, Math.ceil((node.fs.io_stats.total.read_kilobytes - oldnode.fs.io_stats.total.read_kilobytes) / timediffsec * 1024));
          write = Math.max(0, Math.ceil((node.fs.io_stats.total.write_kilobytes - oldnode.fs.io_stats.total.write_kilobytes) / timediffsec * 1024));

          const writeInfoOld = oldnode.thread_pool.bulk || oldnode.thread_pool.write;

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
        const field = req.query.sortField === 'nodeName' ? 'name' : req.query.sortField;
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
      console.log('ERROR -  /api/esstats', err);
      r = {
        health: Db.healthCache(),
        recordsTotal: 0,
        recordsFiltered: 0,
        data: []
      };
      return res.send(r);
    });
  };

  // ES INDICES APIS ----------------------------------------------------------
  /**
   * GET - /api/esindices
   *
   * Fetches a list of Elasticsearch indices.
   * @name /esindices
   * @param {string} filter - Search text to filter the list of Elasticsearch indices by.
   * @param {string} sortField=index - The field to sort the Elasticsearch indices list by.
   * @param {string} desc=false - Whether to return the results in descending order. Defaults to "false".
   * @returns {array} data - List of ES indices with their corresponding stats.
   * @returns {number} recordsTotal - The total number of ES indices.
   * @returns {number} recordsFiltered - The number of ES indices returned in this result.
   */
  module.getESIndices = (req, res) => {
    async.parallel({
      indices: Db.indicesCache,
      indicesSettings: Db.indicesSettingsCache
    }, (err, results) => {
      if (err) {
        console.log('ERROR -  /api/esindices', err);
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
   * Deletes an Elasticsearch index (admin and remove access only).
   * @name /esindices/:index
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
   * Optimizes an Elasticsearch index (admin only).
   * @name /esindices/:index/optimize
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
   * Closes an Elasticsearch index (admin only).
   * @name /esindices/:index/close
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
   * Opens an Elasticsearch index (admin only).
   * @name /esindices/:index/open
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
   * Shrinks an Elasticsearch index (admin only).
   * @name /esindices/:index/shrink
   * @param {string} target - The index name to shrink the index to.
   * @param {number} numShards - The number of shards to shrink the index to.
   * @returns {boolean} success - Whether the close shrink operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.shrinkESIndex = (req, res) => {
    if (!req.body || !req.body.target) {
      return res.molochError(403, 'Missing target');
    }

    const settingsParams = {
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

      const shrinkParams = {
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
      const shrinkCheckInterval = setInterval(() => {
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

  // ES TASK APIS -------------------------------------------------------------
  /**
   * GET - /api/estasks
   *
   * Fetches Elasticsearch tasks.
   * @name /estasks
   * @param {string} filter - Search text to filter the list of ES tasks by.
   * @param {string} cancellable=false - Whether to return only cancellable tasks. Default is "false".
   * @param {string} sortField=action - The field to sort the ES task list by.
   * @param {string} desc=false - Whether to return the results in descending order. Defaults to "false".
   * @param {number} size=1000 - The number of ES tasks to return. Defaults to 1000.
   * @returns {array} data - List of ES tasks with their corresponding stats.
   * @returns {number} recordsTotal - The total number of ES tasks.
   * @returns {number} recordsFiltered - The number of ES tasks returned in this result.
   */
  module.getESTasks = (req, res) => {
    Db.tasks((err, tasks) => {
      if (err) {
        console.log('ERROR -  /api/estask', err);
        return res.send({
          recordsTotal: 0,
          recordsFiltered: 0,
          data: []
        });
      }

      tasks = tasks.tasks;

      let regex;
      if (req.query.filter !== undefined) {
        try {
          regex = new RE2(req.query.filter);
        } catch (e) {
          return res.molochError(500, `Regex Error: ${e}`);
        }
      }

      let rtasks = [];
      for (const key in tasks) {
        const task = tasks[key];

        task.taskId = key;
        if (task.children) {
          task.childrenCount = task.children.length;
        } else {
          task.childrenCount = 0;
        }
        delete task.children;

        if (req.query.cancellable && req.query.cancellable === 'true') {
          if (!task.cancellable) { continue; }
        }

        if (task.headers['X-Opaque-Id']) {
          const parts = ViewerUtils.splitRemain(task.headers['X-Opaque-Id'], '::', 1);
          task.user = (parts.length === 1 ? '' : parts[0]);
        } else {
          task.user = '';
        }

        if (regex && (!task.action.match(regex) && !task.user.match(regex))) { continue; }

        rtasks.push(task);
      }

      const sortField = req.query.sortField || 'action';
      if (sortField === 'action' || sortField === 'user') {
        if (req.query.desc === 'true') {
          rtasks = rtasks.sort((a, b) => { return b.action.localeCompare(a.index); });
        } else {
          rtasks = rtasks.sort((a, b) => { return a.action.localeCompare(b.index); });
        }
      } else {
        if (req.query.desc === 'true') {
          rtasks = rtasks.sort((a, b) => { return b[sortField] - a[sortField]; });
        } else {
          rtasks = rtasks.sort((a, b) => { return a[sortField] - b[sortField]; });
        }
      }

      const size = parseInt(req.query.size) || 1000;
      if (rtasks.length > size) {
        rtasks = rtasks.slice(0, size);
      }

      return res.send({
        recordsTotal: Object.keys(tasks).length,
        recordsFiltered: rtasks.length,
        data: rtasks
      });
    });
  };

  /**
   * POST - /api/estasks/:id/cancel
   *
   * Cancels an Elasticsearch task (admin only).
   * @name /estasks/:id/cancel
   * @returns {boolean} success - Whether the cancel task operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.cancelESTask = (req, res) => {
    let taskId;
    if (req.params.id) {
      taskId = req.params.id;
    } else if (req.body && req.body.taskId) {
      taskId = req.body.taskId;
    } else {
      return res.molochError(403, 'Missing ID of task to cancel');
    }

    Db.taskCancel(taskId, (err, result) => {
      return res.send(JSON.stringify({ success: true, text: result }));
    });
  };

  /**
   * POST - /api/estasks/:id/cancelwith
   *
   * Cancels an Elasticsearch task by opaque id. Used to cancel running tasks
   * that a user has created allowing a user to cancel their own tasks.
   * @name /estasks/:id/cancelwith
   * @returns {boolean} success - Whether the cancel task operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.cancelUserESTask = (req, res) => {
    let cancelId;
    if (req.params.id) {
      cancelId = req.params.id;
    } else if (req.body && req.body.cancelId) {
      cancelId = req.body.cancelId;
    } else {
      return res.molochError(403, 'Missing ID of task to cancel');
    }

    Db.cancelByOpaqueId(`${req.user.userId}::${cancelId}`, (err, result) => {
      return res.send(JSON.stringify({ success: true, text: result }));
    });
  };

  /**
   * POST - /api/estasks/cancelall
   *
   * Cancels all running Elasticsearch tasks (admin only).
   * @name /estasks/cancelall
   * @returns {boolean} success - Whether the cancel all tasks operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.cancelAllESTasks = (req, res) => {
    Db.taskCancel(undefined, (err, result) => {
      return res.send(JSON.stringify({ success: true, text: result }));
    });
  };

  // ES ADMIN APIS ------------------------------------------------------------
  /**
   * GET - /api/esadmin
   *
   * Fetches all Elasticsearch settings that a user can change (es admin only - set in config with <a href="settings#esadminusers">esAdminUsers</a>).
   * @name /esadmin
   * @returns {array} settings - List of ES settings that a user can change
   */
  module.getESAdminSettings = (req, res) => {
    Promise.all([
      Db.getClusterSettings({ flatSettings: true, include_defaults: true }),
      Db.getILMPolicy(),
      Db.getTemplate('sessions2_template')
    ]).then(([settings, ilm, template]) => {
      const rsettings = [];

      function getValue (key) {
        return settings.transient[key] || settings.persistent[key] || settings.defaults[key];
      }

      function addSetting (key, type, name, url, regex, current) {
        if (current === undefined) { current = getValue(key); }
        if (current === undefined) { return; }
        rsettings.push({ key: key, current: current, name: name, type: type, url: url, regex: regex });
      }

      addSetting('search.max_buckets', 'Integer',
        'Max Aggregation Size',
        'https://www.elastic.co/guide/en/elasticsearch/reference/current/search-aggregations-bucket.html',
        '^(|null|\\d+)$');

      addSetting('arkime.disk.watermarks', '3 Percent or Byte Values',
        'Disk Watermark Low,High,Flood',
        'https://www.elastic.co/guide/en/elasticsearch/reference/current/disk-allocator.html',
        '^(|null|\\d+(%|b|kb|mb|gb|tb|pb),\\d+(%|b|kb|mb|gb|tb|pb),\\d+(%|b|kb|mb|gb|tb|pb))$',
        getValue('cluster.routing.allocation.disk.watermark.low') + ',' +
        getValue('cluster.routing.allocation.disk.watermark.high') + ',' +
        getValue('cluster.routing.allocation.disk.watermark.flood_stage')
      );

      addSetting('cluster.routing.allocation.enable', 'Mode',
        'Allocation Mode',
        'https://www.elastic.co/guide/en/elasticsearch/reference/current/shards-allocation.html',
        '^(all|primaries|new_primaries|none)$');

      addSetting('cluster.routing.allocation.cluster_concurrent_rebalance', 'Integer',
        'Concurrent Rebalances',
        'https://www.elastic.co/guide/en/elasticsearch/reference/current/shards-allocation.html',
        '^(|null|\\d+)$');

      addSetting('cluster.routing.allocation.node_concurrent_recoveries', 'Integer',
        'Concurrent Recoveries',
        'https://www.elastic.co/guide/en/elasticsearch/reference/current/shards-allocation.html',
        '^(|null|\\d+)$');

      addSetting('cluster.routing.allocation.node_initial_primaries_recoveries', 'Integer',
        'Initial Primaries Recoveries',
        'https://www.elastic.co/guide/en/elasticsearch/reference/current/shards-allocation.html',
        '^(|null|\\d+)$');

      addSetting('cluster.max_shards_per_node', 'Integer',
        'Max Shards per Node',
        'https://www.elastic.co/guide/en/elasticsearch/reference/master/misc-cluster.html',
        '^(|null|\\d+)$');

      addSetting('indices.recovery.max_bytes_per_sec', 'Byte Value',
        'Recovery Max Bytes per Second',
        'https://www.elastic.co/guide/en/elasticsearch/reference/current/recovery.html',
        '^(|null|\\d+(b|kb|mb|gb|tb|pb))$');

      addSetting('cluster.routing.allocation.awareness.attributes', 'List of Attributes',
        'Shard Allocation Awareness',
        'https://www.elastic.co/guide/en/elasticsearch/reference/current/allocation-awareness.html',
        '^(|null|[a-z0-9_,-]+)$');

      addSetting('indices.breaker.total.limit', 'Percent',
        'Breaker - Total Limit',
        'https://www.elastic.co/guide/en/elasticsearch/reference/current/circuit-breaker.html',
        '^(|null|\\d+%)$');

      addSetting('indices.breaker.fielddata.limit', 'Percent',
        'Breaker - Field data',
        'https://www.elastic.co/guide/en/elasticsearch/reference/current/circuit-breaker.html',
        '^(|null|\\d+%)$');

      addSetting('arkime.sessions.shards', 'Integer',
        'Sessions - Number of shards for FUTURE sessions2 indices',
        'https://www.elastic.co/guide/en/elasticsearch/reference/current/index-modules.html#index-number-of-shards',
        '^\\d+$',
        template[`${internals.prefix}sessions2_template`].settings['index.number_of_shards']);

      addSetting('arkime.sessions.replicas', 'Integer',
        'Sessions - Number of replicas for FUTURE sessions2 indices',
        'https://www.elastic.co/guide/en/elasticsearch/reference/current/index-modules.html#index-number-of-replicas',
        '^\\d+$',
        template[`${internals.prefix}sessions2_template`].settings['index.number_of_replicas'] || 0);

      addSetting('arkime.sessions.shards_per_node', 'Empty or Integer',
        'Sessions - Number of shards_per_node for FUTURE sessions2 indices',
        'https://www.elastic.co/guide/en/elasticsearch/reference/current/allocation-total-shards.html',
        '^(|\\d+)$',
        template[`${internals.prefix}sessions2_template`].settings['index.routing.allocation.total_shards_per_node'] || '');

      function addIlm (key, current, name, type, regex) {
        rsettings.push({ key: key, current: current, name: name, type: type, url: 'https://arkime.com/faq#ilm', regex: regex });
      }

      if (ilm[`${internals.prefix}molochsessions`]) {
        const silm = ilm[`${internals.prefix}molochsessions`];
        addIlm('arkime.ilm.sessions.forceTime', silm.policy.phases.warm.min_age,
          'ILM - Move to warm after', 'Time String', '^\\d+[hd]$');
        addIlm('arkime.ilm.sessions.replicas', silm.policy.phases.warm.actions.allocate.number_of_replicas,
          'ILM - Number of replicas after setting to warm', 'Integer', '^\\d$');
        addIlm('arkime.ilm.sessions.segments', silm.policy.phases.warm.actions.forcemerge.max_num_segments,
          'ILM - Number of segments after setting to warm', 'Integer', '^\\d$');
        addIlm('arkime.ilm.sessions.deleteTime', silm.policy.phases.delete.min_age,
          'ILM - Delete session index after', 'Time String', '^\\d+[hd]$');
      }

      if (ilm[`${internals.prefix}molochhistory`]) {
        const hilm = ilm[`${internals.prefix}molochhistory`];
        addIlm('arkime.ilm.history.deleteTime', hilm.policy.phases.delete.min_age,
          'ILM - Delete History index after', 'Time String', '^\\d+[hd]$');
      }

      return res.send(rsettings);
    });
  };

  /**
   * POST - /api/esadmin/set
   *
   * Sets Elasticsearch settings (es admin only - set in config with <a href="settings#esadminusers">esAdminUsers</a>).
   * @name /esadmin/set
   * @returns {boolean} success - Whether saving the settings was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.setESAdminSettings = (req, res) => {
    if (req.body.key === undefined) { return res.molochError(500, 'Missing key'); }
    if (req.body.value === undefined) { return res.molochError(500, 'Missing value'); }

    // Convert null string to null
    if (req.body.value === 'null') { req.body.value = null; }

    // Must set all 3 at once because of ES bug/feature
    if (req.body.key === 'arkime.disk.watermarks') {
      const query = { body: { persistent: {} } };
      if (req.body.value === '' || req.body.value === null) {
        query.body.persistent['cluster.routing.allocation.disk.watermark.low'] = null;
        query.body.persistent['cluster.routing.allocation.disk.watermark.high'] = null;
        query.body.persistent['cluster.routing.allocation.disk.watermark.flood_stage'] = null;
      } else {
        const parts = req.body.value.split(',');
        if (parts.length !== 3) {
          return res.molochError(500, 'Must be 3 piece of info');
        }

        query.body.persistent['cluster.routing.allocation.disk.watermark.low'] = parts[0];
        query.body.persistent['cluster.routing.allocation.disk.watermark.high'] = parts[1];
        query.body.persistent['cluster.routing.allocation.disk.watermark.flood_stage'] = parts[2];
      }

      Db.putClusterSettings(query, (err, result) => {
        if (err) {
          console.log('putSettings failed', JSON.stringify(result, false, 2), 'query', JSON.stringify(query, false, 2));
          return res.molochError(500, 'Set failed');
        }
        return res.send(JSON.stringify({ success: true, text: 'Set' }));
      });
      return;
    }

    if (req.body.key.startsWith('arkime.ilm')) {
      Promise.all([Db.getILMPolicy()]).then(([ilm]) => {
        const silm = ilm[`${internals.prefix}molochsessions`];
        const hilm = ilm[`${internals.prefix}molochhistory`];

        if (silm === undefined || hilm === undefined) {
          return res.molochError(500, 'ILM isn\'t configured');
        }

        switch (req.body.key) {
        case 'arkime.ilm.sessions.forceTime':
          silm.policy.phases.warm.min_age = req.body.value;
          break;
        case 'arkime.ilm.sessions.replicas':
          silm.policy.phases.warm.actions.allocate.number_of_replicas = parseInt(req.body.value || 0, 10);
          break;
        case 'arkime.ilm.sessions.segments':
          silm.policy.phases.warm.actions.forcemerge.max_num_segments = parseInt(req.body.value || 0, 10);
          break;
        case 'arkime.ilm.sessions.deleteTime':
          silm.policy.phases.delete.min_age = req.body.value;
          break;
        case 'arkime.ilm.history.deleteTime':
          hilm.policy.phases.delete.min_age = req.body.value;
          break;
        default:
          return res.molochError(500, 'Unknown field');
        }
        if (req.body.key.startsWith('arkime.ilm.history')) {
          Db.setILMPolicy(`${internals.prefix}molochhistory`, hilm);
        } else {
          Db.setILMPolicy(`${internals.prefix}molochsessions`, silm);
        }
        return res.send(JSON.stringify({ success: true, text: 'Set' }));
      });
      return;
    }

    if (req.body.key.startsWith('arkime.sessions')) {
      Promise.all([Db.getTemplate('sessions2_template')]).then(([template]) => {
        switch (req.body.key) {
        case 'arkime.sessions.shards':
          template[`${internals.prefix}sessions2_template`].settings['index.number_of_shards'] = req.body.value;
          break;
        case 'arkime.sessions.replicas':
          template[`${internals.prefix}sessions2_template`].settings['index.number_of_replicas'] = req.body.value;
          break;
        case 'arkime.sessions.shards_per_node':
          if (req.body.value === '') {
            delete template[`${internals.prefix}sessions2_template`].settings['index.routing.allocation.total_shards_per_node'];
          } else {
            template[`${internals.prefix}sessions2_template`].settings['index.routing.allocation.total_shards_per_node'] = req.body.value;
          }
          break;
        default:
          return res.molochError(500, 'Unknown field');
        }
        Db.putTemplate('sessions2_template', template[`${internals.prefix}sessions2_template`]);
        return res.send(JSON.stringify({ success: true, text: 'Set' }));
      });
      return;
    }

    const query = { body: { persistent: {} } };
    query.body.persistent[req.body.key] = req.body.value || null;

    Db.putClusterSettings(query, (err, result) => {
      if (err) {
        console.log('putSettings failed', JSON.stringify(result, false, 2), 'query', JSON.stringify(query, false, 2));
        return res.molochError(500, 'Set failed');
      }
      return res.send(JSON.stringify({ success: true, text: 'Set' }));
    });
  };

  /**
   * POST - /api/esadmin/reroute
   *
   * Try to restart any shard migrations that have failed or paused (es admin only - set in config with <a href="settings#esadminusers">esAdminUsers</a>).
   * @name /esadmin/reroute
   * @returns {boolean} success - Whether the reroute was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.rerouteES = (req, res) => {
    Db.reroute((err) => {
      if (err) {
        return res.send(JSON.stringify({ success: true, text: 'Reroute failed' }));
      } else {
        return res.send(JSON.stringify({ success: true, text: 'Reroute successful' }));
      }
    });
  };

  /**
   * POST - /api/esadmin/flush
   *
   * Flush and refresh any data waiting in Elasticsearch to disk (es admin only - set in config with <a href="settings#esadminusers">esAdminUsers</a>).
   * @name /esadmin/flush
   * @returns {boolean} success - Always true
   * @returns {string} text - The success message to (optionally) display to the user.
   */
  module.flushES = (req, res) => {
    Db.refresh('*');
    Db.flush('*');
    return res.send(JSON.stringify({ success: true, text: 'Flushed' }));
  };

  /**
   * POST - /api/esadmin/unflood
   *
   * Try and clear any indices marked as flooded (es admin only - set in config with <a href="settings#esadminusers">esAdminUsers</a>).
   * @name /esadmin/unflood
   * @returns {boolean} success - Always true
   * @returns {string} text - The success message to (optionally) display to the user.
   */
  module.unfloodES = (req, res) => {
    Db.setIndexSettings('*', {
      body: { 'index.blocks.read_only_allow_delete': null }
    });
    return res.send(JSON.stringify({ success: true, text: 'Unflooded' }));
  };

  /**
   * POST - /api/esadmin/clearcache
   *
   * Try and clear the cache for all indices (es admin only - set in config with <a href="settings#esadminusers">esAdminUsers</a>).
   * @name /esadmin/clearcache
   * @returns {boolean} success - Whether clearing the cache was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.clearCacheES = (req, res) => {
    Db.clearCache((err, data) => {
      if (err) {
        return res.send(JSON.stringify({ success: false, text: 'Cache clear failed' }));
      } else {
        return res.send(JSON.stringify({ success: true, text: `Cache cleared: ${data._shards.successful} of ${data._shards.total} shards successful, with ${data._shards.failed} failing` }));
      }
    });
  };

  // ES SHARD APIS ------------------------------------------------------------
  /**
   * GET - /api/esshards
   *
   * Fetches all Elasticsearch shards
   * @name /esshards
   * @param {string} filter - Search text to filter the list of Elasticsearch shards by.
   * @param {string} show=all - Which types of shard to show. Options include:
     all - show all shards.
     notstarted - show unstarted shards.
     INITIALIZING - show initializing shards.
     RELOCATING - show relocating shards.
     UNASSIGNED - show unassigned shards.
   * @param {string} desc=false - Whether to return the results in descending order. Defaults to "false".
   * @returns {array} nodes - List of ES data nodes.
   * @returns {array} indices - List of ES indices.
   * @returns {array} nodeExcludes - List of node names that disallow the allocation of shards.
   * @returns {array} ipExcludes - List of node ips that disallow the allocation of shards.
   */
  module.getESShards = (req, res) => {
    Promise.all([
      Db.shards(),
      Db.getClusterSettings({ flatSettings: true })
    ]).then(([shards, settings]) => {
      let ipExcludes = [];
      if (settings.persistent['cluster.routing.allocation.exclude._ip']) {
        ipExcludes = settings.persistent['cluster.routing.allocation.exclude._ip'].split(',');
      }

      let nodeExcludes = [];
      if (settings.persistent['cluster.routing.allocation.exclude._name']) {
        nodeExcludes = settings.persistent['cluster.routing.allocation.exclude._name'].split(',');
      }

      let regex;
      if (req.query.filter !== undefined) {
        try {
          regex = new RE2(req.query.filter.toLowerCase());
        } catch (e) {
          return res.molochError(500, `Regex Error: ${e}`);
        }
      }

      const result = {};
      const nodes = {};

      for (const shard of shards) {
        if (shard.node === null || shard.node === 'null') { shard.node = 'Unassigned'; }

        if (!(req.query.show === 'all' ||
          shard.state === req.query.show || //  Show only matching stage
          (shard.state !== 'STARTED' && req.query.show === 'notstarted'))) {
          continue;
        }

        if (regex && !shard.index.toLowerCase().match(regex) && !shard.node.toLowerCase().match(regex)) { continue; }

        if (result[shard.index] === undefined) {
          result[shard.index] = { name: shard.index, nodes: {} };
        }
        if (result[shard.index].nodes[shard.node] === undefined) {
          result[shard.index].nodes[shard.node] = [];
        }
        result[shard.index].nodes[shard.node].push(shard);
        nodes[shard.node] = { ip: shard.ip, ipExcluded: ipExcludes.includes(shard.ip), nodeExcluded: nodeExcludes.includes(shard.node) };

        result[shard.index].nodes[shard.node]
          .sort((a, b) => {
            return a.shard - b.shard;
          });

        delete shard.node;
        delete shard.index;
      }

      let indices = Object.keys(result).map((k) => result[k]);
      if (req.query.desc === 'true') {
        indices = indices.sort((a, b) => {
          return b.name.localeCompare(a.name);
        });
      } else {
        indices = indices.sort((a, b) => {
          return a.name.localeCompare(b.name);
        });
      }

      res.send({
        nodes: nodes,
        indices: indices,
        nodeExcludes: nodeExcludes,
        ipExcludes: ipExcludes
      });
    });
  };

  /**
   * POST - /api/esshards/:type/:value/exclude
   *
   * Exclude Elasticsearch node by ip or name (admin only).
   * @name /esshards/:type/:value/exclude
   * @returns {boolean} success - Whether exclude node operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.excludeESShard = (req, res) => {
    if (Config.get('multiES', false)) {
      return res.molochError(401, 'Not supported in multies');
    }

    Db.getClusterSettings({ flatSettings: true }, (err, settings) => {
      let exclude = [];
      let settingName;

      if (req.params.type === 'ip') {
        settingName = 'cluster.routing.allocation.exclude._ip';
      } else if (req.params.type === 'name') {
        settingName = 'cluster.routing.allocation.exclude._name';
      } else {
        return res.molochError(403, 'Unknown exclude type');
      }

      if (settings.persistent[settingName]) {
        exclude = settings.persistent[settingName].split(',');
      }

      if (!exclude.includes(req.params.value)) {
        exclude.push(req.params.value);
      }

      const query = { body: { persistent: {} } };
      query.body.persistent[settingName] = exclude.join(',');

      Db.putClusterSettings(query, (err, settings) => {
        if (err) { console.log('putSettings', JSON.stringify(err, false, 2), 'query', query); }
        return res.send(JSON.stringify({ success: true, text: 'Excluded' }));
      });
    });
  };

  /**
   * POST - /api/esshards/:type/:value/include
   *
   * Include Elasticsearch node by ip or name (admin only).
   * @name /esshards/:type/:value/include
   * @returns {boolean} success - Whether include node operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.includeESShard = (req, res) => {
    if (Config.get('multiES', false)) {
      return res.molochError(401, 'Not supported in multies');
    }

    Db.getClusterSettings({ flatSettings: true }, (err, settings) => {
      let exclude = [];
      let settingName;

      if (req.params.type === 'ip') {
        settingName = 'cluster.routing.allocation.exclude._ip';
      } else if (req.params.type === 'name') {
        settingName = 'cluster.routing.allocation.exclude._name';
      } else {
        return res.molochError(403, 'Unknown include type');
      }

      if (settings.persistent[settingName]) {
        exclude = settings.persistent[settingName].split(',');
      }

      const pos = exclude.indexOf(req.params.value);
      if (pos > -1) {
        exclude.splice(pos, 1);
      }

      const query = { body: { persistent: {} } };
      query.body.persistent[settingName] = exclude.join(',');

      Db.putClusterSettings(query, (err, settings) => {
        if (err) { console.log('putSettings', err); }
        return res.send(JSON.stringify({ success: true, text: 'Included' }));
      });
    });
  };

  // ES RECOVERY APIS ---------------------------------------------------------
  /**
   * GET - /api/esrecovery
   *
   * Returns information about ongoing and completed shard recoveries for indices.
   * @name /esrecovery
   * @param {string} filter - Search text to filter the list of indices by.
   * @param {string} sortField=index - The field to sort the indices by.
   * @param {string} desc=false - Whether to return the results in descending order. Defaults to "false".
   * @param {string} show=active - Whether to show "all" or "active" recovering indices.
   * @returns {array} data - List of indices with their corresponding stats.
   * @returns {number} recordsTotal - The total number of indices.
   * @returns {number} recordsFiltered - The number of indices returned in this result.
   */
  module.getESRecovery = (req, res) => {
    const sortField = (req.query.sortField || 'index') + (req.query.desc === 'true' ? ':desc' : '');

    Promise.all([
      Db.recovery(sortField, req.query.show !== 'all')
    ]).then(([recoveries]) => {
      let regex;
      if (req.query.filter !== undefined) {
        try {
          regex = new RE2(req.query.filter);
        } catch (e) {
          return res.molochError(500, `Regex Error: ${e}`);
        }
      }

      let result = [];

      for (const recovery of recoveries) {
        // filtering
        if (regex && !recovery.index.match(regex) &&
          !recovery.target_node.match(regex) &&
          !recovery.source_node.match(regex)) {
          continue;
        }

        result.push(recovery);
      }

      // Work around for https://github.com/elastic/elasticsearch/issues/48070
      if (req.query.sortField && req.query.sortField.endsWith('_percent')) {
        const sf = req.query.sortField;
        if (req.query.desc === 'true') {
          result = result.sort((a, b) => { return parseFloat(b[sf]) - parseFloat(a[sf]); });
        } else {
          result = result.sort((a, b) => { return parseFloat(a[sf]) - parseFloat(b[sf]); });
        }
      }

      res.send({
        recordsTotal: recoveries.length,
        recordsFiltered: result.length,
        data: result
      });
    }).catch((err) => {
      console.log('ERROR -  /api/esrecovery', err);
      return res.send({
        recordsTotal: 0,
        recordsFiltered: 0,
        data: []
      });
    });
  };

  // PARLIAMENT APIs ----------------------------------------------------------
  /**
   * GET - /api/parliament
   *
   * Returns information all the Arkime clusters configured in your Parliament.
   * See the parliament definition <a href="https://github.com/arkime/arkime/tree/master/parliament#parliament-definition">here</a> (subject to change).
   * @name /parliament
   * @returns {array} data - List of fields that describe the cluster stats.
   * @returns {number} recordsTotal - The total number of stats.
   * @returns {number} recordsFiltered - The number of stats returned in this result.
   */
  module.getParliament = (req, res) => {
    const query = {
      size: 1000,
      query: {
        bool: {
          must_not: [
            { term: { hide: true } }
          ]
        }
      },
      _source: [
        'ver', 'nodeName', 'currentTime', 'monitoring', 'deltaBytes',
        'deltaPackets', 'deltaMS', 'deltaESDropped', 'deltaDropped',
        'deltaOverloadDropped'
      ]
    };

    Promise.all([
      Db.search('stats', 'stat', query),
      Db.numberOfDocuments('stats')
    ]).then(([stats, total]) => {
      if (stats.error) { throw stats.error; }

      const results = { total: stats.hits.total, results: [] };

      for (const stat of stats.hits.hits) {
        const fields = stat._source || stat.fields;

        if (stat._source) {
          ViewerUtils.mergeUnarray(fields, stat.fields);
        }
        fields.id = stat._id;

        // make sure necessary fields are not undefined
        const keys = ['deltaOverloadDropped', 'monitoring', 'deltaESDropped'];
        for (const key of keys) {
          fields[key] = fields[key] || 0;
        }

        fields.deltaBytesPerSec = Math.floor(fields.deltaBytes * 1000.0 / fields.deltaMS);
        fields.deltaPacketsPerSec = Math.floor(fields.deltaPackets * 1000.0 / fields.deltaMS);
        fields.deltaESDroppedPerSec = Math.floor(fields.deltaESDropped * 1000.0 / fields.deltaMS);
        fields.deltaTotalDroppedPerSec = Math.floor((fields.deltaDropped + fields.deltaOverloadDropped) * 1000.0 / fields.deltaMS);

        results.results.push(fields);
      }

      res.send({
        data: results.results,
        recordsTotal: total.count,
        recordsFiltered: results.total
      });
    }).catch((err) => {
      console.log('ERROR - /api/parliament', err);
      res.send({ recordsTotal: 0, recordsFiltered: 0, data: [] });
    });
  };

  return module;
};
