/******************************************************************************/
/* apiCrons.js -- api calls for periodic queries
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const Config = require('./config');
const util = require('util');
const Db = require('./db');
const Auth = require('../common/auth');
const User = require('../common/user');
const ArkimeUtil = require('../common/arkimeUtil');
const ArkimeConfig = require('../common/arkimeConfig');
const async = require('async');
const internals = require('./internals');
const SessionAPIs = require('./apiSessions');
const ViewerUtils = require('./viewerUtils');
const http = require('http');
const arkimeparser = require('./arkimeparser.js');
const Notifier = require('../common/notifier');

class CronAPIs {
  // --------------------------------------------------------------------------
  static #primaryViewer = false;
  static isPrimaryViewer () {
    return CronAPIs.#primaryViewer;
  }

  // --------------------------------------------------------------------------
  static #runPrimaryViewer () {
    if (!CronAPIs.#primaryViewer) { return; }
    CronAPIs.processCronQueries();
    HuntAPIs.processHuntJobs();
    Db.updateLocalShortcuts();
  }

  // --------------------------------------------------------------------------
  static async #updatePrimaryViewer (force) {
    const previous = CronAPIs.#primaryViewer;
    CronAPIs.#primaryViewer = await Db.setQueriesNode(Config.nodeName(), force);
    if (previous !== CronAPIs.#primaryViewer) {
      if (!previous) {
        console.log('This node will process Periodic Queries (CRON) & Hunts, delayed by', internals.cronTimeout, 'seconds');
      } else {
        console.log('This node will no longer process Periodic Queries (CRON) & Hunts');
      }
    }
  }

  // --------------------------------------------------------------------------
  static async initialize (options) {
    if (Config.get('cronQueries') === 'auto') {
      setTimeout(CronAPIs.#updatePrimaryViewer, 1000, false);
      setInterval(CronAPIs.#updatePrimaryViewer, 45 * 1000, false);
      setInterval(CronAPIs.#runPrimaryViewer, 60 * 1000);
    } else if (Config.get('cronQueries') === true) {
      setTimeout(CronAPIs.#updatePrimaryViewer, 1000, true);
      setInterval(CronAPIs.#updatePrimaryViewer, 120 * 1000, true);
      setInterval(CronAPIs.#runPrimaryViewer, 60 * 1000);
    } else if (!Config.get('multiES', false)) {
      const info = await Db.getQueriesNode();
      if (info.node === undefined) {
        console.log(`WARNING - No cronQueries=true found in ${ArkimeConfig.configFile}, one and only one node MUST have cronQueries=true set for cron/hunts to work`);
      } else if (Date.now() - info.updateTime > 2 * 60 * 1000) {
        console.log(`WARNING - cronQueries=true node '${info.node}' hasn't checked in lately, cron/hunts might be broken`);
      }
    }
  }

  /**
   * A query to be run periodically that can perform actions on sessions that match the queries. The query runs against sessions delayed by 90 seconds to make sure all updates have been completed for that session.
   *
   * @typedef ArkimeQuery
   * @type {object}
   * @param {string} name - The name of the query
   * @param {boolean} enabled - Whether the query is enabled. If enabled, the query will run every 90 seconds.
   * @param {number} lpValue - The last packet timestamp that was searched. Used to query for the next group of sessions to search. Format is seconds since Unix EPOC.
   * @param {number} lastRun - The time that the query was last run. Format is seconds since Unix EPOC.
   * @param {number} count - The count of total sessions that have matched this query.
   * @param {number} lastCount - The count of sessions that have matched this query during its last run.
   * @param {string} query - The search expression to apply when searching for sessions.
   * @param {string} action=tag - The action to perform when sessions have matched. "tag" or "forward:clusterName".
   * @param {string} creator - The id of the user that created this query.
   * @param {string} tags - A comma separated list of tags to add to each session that matches this query.
   * @param {string} notifier - The name of the notifier to alert when there are matches for this query.
   * @param {number} lastNotified - The time that this query last sent a notification to the notifier. Only notifies every 10 minutes. Format is seconds since Unix EPOC.
   * @param {number} lastNotifiedCount - The count of sessions that matched since the last notification was sent.
   * @param {string} description - The description of this query.
   * @param {number} created - The time that this query was created. Format is seconds since Unix EPOC.
   * @param {number} lastToggled - The time that this query was enabled or disabled. Format is seconds since Unix EPOC.
   * @param {string} lastToggledBy - The user who last enabled or disabled this query.
   * @param {string} users - The list of userIds who have access to use this query.
   * @param {string} roles - The list of roles who have access to use this query.
   * @param {string} editRoles - The list of roles who have access to edit this query.
   */

  // --------------------------------------------------------------------------
  /**
   * GET - /api/crons
   *
   * Retrieves periodic queries a user can view.
   * @name /crons
   * @returns {ArkimeQuery[]} queries - A list of query objects.
   */
  static async getCrons (req, res) {
    if (!req.settingUser) {
      return res.serverError(403, 'Unknown user');
    }

    const user = req.settingUser;
    if (user.settings === undefined) { user.settings = {}; }

    const roles = [...await user.getRoles()]; // es requires an array for terms search

    const query = {
      size: 1000,
      sort: { created: { order: 'asc' } },
      query: {
        bool: {
          must_not: [
            { ids: { values: ['primary-viewer'] } }
          ],
          filter: [
            {
              bool: {
                should: [
                  { terms: { roles } }, // shared via user role
                  { terms: { editRoles: roles } }, // shared via edit role
                  { term: { users: user.userId } }, // shared via userId
                  { term: { creator: user.userId } } // created by this user
                ]
              }
            }
          ]
        }
      }
    };

    if (req.query.all && roles.includes('arkimeAdmin')) {
      query.query.bool.filter = []; // remove sharing restrictions
    }

    Db.search('queries', 'query', query, (err, data) => {
      if (err || data.error) {
        console.log(`ERROR - ${req.method} /api/crons`, util.inspect(err || data.error, false, 50));
      }

      let queries = [];

      if (data && data.hits && data.hits.hits) {
        queries = data.hits.hits.map((item) => {
          const key = item._id;
          const result = item._source;

          if (user.userId !== result.creator && !user.hasRole('arkimeAdmin')) {
            // remove sensitive information for users this query is shared with (except arkimeAdmin)
            delete result.users;
            delete result.roles;
            delete roles.editRoles;
          } else {
            if (result.users) { // client expects a string
              result.users = result.users.join(',');
            }
          }

          result.key = key;
          return result;
        });
      }

      res.send(queries);
    });
  }

  // --------------------------------------------------------------------------
  /**
   * POST - /api/cron
   *
   * Create a new periodic query.
   * @name /cron
   * @returns {boolean} success - Whether the create operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {ArkimeQuery} query - The new query
   */
  static async createCron (req, res) {
    if (!ArkimeUtil.isString(req.body.name)) {
      return res.serverError(403, 'Missing query name');
    }
    if (!ArkimeUtil.isString(req.body.query)) {
      return res.serverError(403, 'Missing query expression');
    }
    if (!ArkimeUtil.isString(req.body.action)) {
      return res.serverError(403, 'Missing query action');
    }
    if (!ArkimeUtil.isString(req.body.tags)) {
      return res.serverError(403, 'Missing query tag(s)');
    }

    if (req.body.roles !== undefined && !ArkimeUtil.isStringArray(req.body.roles)) {
      return res.serverError(403, 'Roles field must be an array of strings');
    }

    if (req.body.editRoles !== undefined && !ArkimeUtil.isStringArray(req.body.editRoles)) {
      return res.serverError(403, 'Edit roles field must be an array of strings');
    }

    if (req.body.users !== undefined && !ArkimeUtil.isString(req.body.users, 0)) {
      return res.serverError(403, 'Users field must be a string');
    }

    // comma/newline separated value -> array of values
    let users = ArkimeUtil.commaOrNewlineStringToArray(req.body.users || '');
    users = await User.validateUserIds(users);
    req.body.users = users.validUsers;

    const doc = {
      doc: {
        enabled: true,
        name: req.body.name,
        tags: req.body.tags,
        users: req.body.users,
        roles: req.body.roles,
        query: req.body.query,
        action: req.body.action,
        editRoles: req.body.editRoles,
        created: Math.floor(Date.now() / 1000)
      }
    };

    if (ArkimeUtil.isString(req.body.description)) {
      doc.doc.description = req.body.description;
    }

    if (ArkimeUtil.isString(req.body.notifier)) {
      doc.doc.notifier = req.body.notifier;
    }

    const userId = req.settingUser.userId;

    let minTimestamp;
    try {
      const { body: data } = await Db.getMinValue(Db.getSessionIndices(true), '@timestamp');
      minTimestamp = Math.floor(data.aggregations.min.value / 1000);
    } catch (err) {
      minTimestamp = Math.floor(Date.now() / 1000);
    }

    if (+req.body.since === -1) {
      doc.doc.lpValue = doc.doc.lastRun = minTimestamp;
    } else {
      doc.doc.lpValue = doc.doc.lastRun =
         Math.max(minTimestamp, Math.floor(Date.now() / 1000) - 60 * 60 * parseInt(req.body.since || '0', 10));
    }

    doc.doc.count = 0;
    doc.doc.creator = userId || 'anonymous';

    try {
      const { body: info } = await Db.indexNow('queries', 'query', null, doc.doc);

      if (CronAPIs.#primaryViewer) { CronAPIs.processCronQueries(); }

      doc.doc.key = info._id;
      if (doc.doc.users) {
        doc.doc.users = doc.doc.users.join(',');
      }

      return res.send(JSON.stringify({
        success: true,
        query: doc.doc,
        text: 'Created query!',
        invalidUsers: users.invalidUsers
      }));
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/cron`, util.inspect(err, false, 50));
      return res.serverError(500, 'Create query failed');
    }
  }

  // --------------------------------------------------------------------------
  /**
   * POST - /api/cron/:key
   *
   * Update a periodic query.
   * @name /user/:key
   * @returns {boolean} success - Whether the update operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {ArkimeQuery} query - The updated query object
   */
  static async updateCron (req, res) {
    const key = req.params.key;
    if (key === 'primary-viewer') {
      return res.serverError(403, 'Bad query key');
    }
    if (!ArkimeUtil.isString(key)) {
      return res.serverError(403, 'Missing query key');
    }
    if (!ArkimeUtil.isString(req.body.name)) {
      return res.serverError(403, 'Missing query name');
    }
    if (!ArkimeUtil.isString(req.body.query)) {
      return res.serverError(403, 'Missing query expression');
    }
    if (!ArkimeUtil.isString(req.body.action)) {
      return res.serverError(403, 'Missing query action');
    }
    if (!ArkimeUtil.isString(req.body.tags)) {
      return res.serverError(403, 'Missing query tag(s)');
    }

    if (req.body.roles !== undefined && !ArkimeUtil.isStringArray(req.body.roles)) {
      return res.serverError(403, 'Roles field must be an array of strings');
    }

    if (req.body.editRoles !== undefined && !ArkimeUtil.isStringArray(req.body.editRoles)) {
      return res.serverError(403, 'Edit roles field must be an array of strings');
    }

    if (req.body.users !== undefined && !ArkimeUtil.isString(req.body.users, 0)) {
      return res.serverError(403, 'Users field must be a string');
    }

    // comma/newline separated value -> array of values
    let users = ArkimeUtil.commaOrNewlineStringToArray(req.body.users || '');
    users = await User.validateUserIds(users);
    req.body.users = users.validUsers;

    const doc = {
      doc: {
        description: '',
        notifier: undefined,
        name: req.body.name,
        tags: req.body.tags,
        users: req.body.users,
        roles: req.body.roles,
        query: req.body.query,
        action: req.body.action,
        enabled: req.body.enabled,
        editRoles: req.body.editRoles
      }
    };

    if (ArkimeUtil.isString(req.body.notifier)) {
      doc.doc.notifier = req.body.notifier;
    }

    if (ArkimeUtil.isString(req.body.description)) {
      doc.doc.description = req.body.description;
    }

    try {
      const { body: { _source: cron } } = await Db.getQuery(key);

      // sets the owner if it has changed
      doc.doc.creator ??= req.body.creator;
      if (!await User.setOwner(req, res, doc.doc, cron, 'creator')) {
        return;
      }

      if (doc.doc.enabled !== cron.enabled) { // the query was enabled or disabled
        doc.doc.lastToggledBy = req.settingUser.userId;
        doc.doc.lastToggled = Math.floor(Date.now() / 1000);
      }

      const query = { // last object property overwrites the previous one
        ...cron,
        ...doc.doc
      };

      try {
        await Db.update('queries', 'query', key, doc, { refresh: true });
      } catch (err) {
        console.log(`ERROR - ${req.method} /api/cron/%s`, ArkimeUtil.sanitizeStr(key), util.inspect(err, false, 50));
      }

      if (CronAPIs.#primaryViewer) { CronAPIs.processCronQueries(); }

      query.key = key;
      if (query.users) {
        query.users = query.users.join(',');
      }

      return res.send(JSON.stringify({
        query,
        success: true,
        text: 'Updated periodic query!',
        invalidUsers: users.invalidUsers
      }));
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/cron/%s`, ArkimeUtil.sanitizeStr(key), util.inspect(err, false, 50));
      return res.serverError(403, 'Periodic query update failed');
    }
  }

  // --------------------------------------------------------------------------
  /**
   * DELETE - /api/cron/:key
   *
   * Delete a periodic query.
   * @name /cron/:key
   * @returns {boolean} success - Whether the delete operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async deleteCron (req, res) {
    const key = req.params.key;

    if (key === 'primary-viewer') {
      return res.serverError(403, 'Bad query key');
    }

    if (!ArkimeUtil.isString(key)) {
      return res.serverError(403, 'Missing periodic query key');
    }

    try {
      await Db.deleteDocument('queries', 'query', key, { refresh: true });
      res.send(JSON.stringify({
        success: true,
        text: 'Deleted periodic query successfully'
      }));
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/cron/%s`, ArkimeUtil.sanitizeStr(key), util.inspect(err, false, 50));
      return res.serverError(500, 'Delete periodic query failed');
    }
  };

  // --------------------------------------------------------------------------
  static #qlworking = {};
  static #sendSessionsListQL (pOptions, list, nextQLCb) {
    if (!list) {
      return;
    }

    const nodes = {};

    list.forEach(function (item) {
      if (!nodes[item.node]) {
        nodes[item.node] = [];
      }
      nodes[item.node].push(item.id);
    });

    const keys = Object.keys(nodes);

    async.eachLimit(keys, 15, function (node, nextCb) {
      SessionAPIs.isLocalView(node, function () {
        let sent = 0;
        nodes[node].forEach(function (item) {
          const options = {
            id: item,
            nodeName: node
          };
          Db.merge(options, pOptions);

          // Get from our DISK
          internals.sendSessionQueue.push(options, function () {
            sent++;
            if (sent === nodes[node].length) {
              nextCb();
            }
          });
        });
      },
      function () {
        // Get from remote DISK
        ViewerUtils.getViewUrl(node, (err, viewUrl, client) => {
          let sendPath = `${Config.basePath(node)}api/sessions/${node}/send?saveId=${pOptions.saveId}&remoteCluster=${pOptions.cluster}`;
          if (pOptions.tags) { sendPath += `&tags=${pOptions.tags}`; }
          const url = new URL(sendPath, viewUrl);
          const reqOptions = {
            method: 'POST',
            agent: client === http ? internals.httpAgent : internals.httpsAgent
          };

          Auth.addS2SAuth(reqOptions, pOptions.user, node, sendPath);
          ViewerUtils.addCaTrust(reqOptions, node);

          const preq = client.request(url, reqOptions, (pres) => {
            pres.on('data', (chunk) => {
              CronAPIs.#qlworking[url.path] = 'data';
            });
            pres.on('end', () => {
              delete CronAPIs.#qlworking[url.path];
              setImmediate(nextCb);
            });
          });
          preq.on('error', (e) => {
            delete CronAPIs.#qlworking[url.path];
            console.log("ERROR - Couldn't proxy sendSession request=", url, '\nerror=', e);
            setImmediate(nextCb);
          });
          preq.setHeader('content-type', 'application/x-www-form-urlencoded');
          preq.write('ids=');
          preq.write(nodes[node].join(','));
          preq.end();
          CronAPIs.#qlworking[url.path] = 'sent';
        });
      });
    }, (err) => {
      nextQLCb();
    });
  }

  // --------------------------------------------------------------------------
  /* Process a single cron query.  At max it will process 24 hours worth of data
   * to give other queries a chance to run.  Because its timestamp based and not
   * lastPacket based since 1.0 it now search all indices each time.
   */
  static #processCronQuery (cq, options, query, endTime, cb) {
    if (Config.debug > 2) {
      console.log('CRON', cq.name, cq.creator, '- processCronQuery(', cq, options, query, endTime, ')');
    }

    let singleEndTime;
    let count = 0;
    async.doWhilst((whilstCb) => {
      // Process at most 24 hours
      singleEndTime = Math.min(endTime, cq.lpValue + 24 * 60 * 60);
      query.query.bool.filter[0] = { range: { '@timestamp': { gte: cq.lpValue * 1000, lt: singleEndTime * 1000 } } };

      if (Config.debug > 2) {
        console.log('CRON', cq.name, cq.creator, '- start:', new Date(cq.lpValue * 1000), 'stop:', new Date(singleEndTime * 1000), 'end:', new Date(endTime * 1000), 'remaining runs:', ((endTime - singleEndTime) / (24 * 60 * 60.0)));
      }

      Db.searchSessions(Db.getSessionIndices(true), query, { scroll: internals.esScrollTimeout }, function getMoreUntilDone (err, result) {
        async function doNext () {
          count += result.hits.hits.length;

          // No more data, all done
          if (result.hits.hits.length === 0) {
            Db.clearScroll({ body: { scroll_id: result._scroll_id } });
            return setImmediate(whilstCb, 'DONE');
          } else {
            const doc = { doc: { count: (query.count || 0) + count } };
            try {
              Db.update('queries', 'query', options.qid, doc, { refresh: true });
            } catch (err) {
              console.log('ERROR CRON - updating query', err);
            }
          }

          query = {
            body: {
              scroll_id: result._scroll_id
            },
            scroll: internals.esScrollTimeout
          };

          try {
            const { body: results } = await Db.scroll(query);
            return getMoreUntilDone(null, results);
          } catch (err) {
            console.log('ERROR CRON - issuing scroll for cron job', err);
            return getMoreUntilDone(err, {});
          }
        }

        if (err || result.error) {
          console.log('CRON - cronQuery error', err, (result ? result.error : null), 'for', cq);
          return setImmediate(whilstCb, 'ERR');
        }

        const ids = [];
        const hits = result.hits.hits;
        let i, ilen;
        if (cq.action.indexOf('forward:') === 0) {
          for (i = 0, ilen = hits.length; i < ilen; i++) {
            ids.push({ id: Db.session2Sid(hits[i]), node: hits[i]._source.node });
          }

          CronAPIs.#sendSessionsListQL(options, ids, doNext);
        } else if (cq.action.indexOf('tag') === 0) {
          for (i = 0, ilen = hits.length; i < ilen; i++) {
            ids.push(Db.session2Sid(hits[i]));
          }

          if (Config.debug > 1) {
            console.log('CRON', cq.name, cq.creator, '- Updating tags:', ids.length);
          }

          const tags = options.tags.split(',');
          SessionAPIs.sessionsListFromIds(null, ids, ['tags', 'node'], (err, list) => {
            SessionAPIs.addTagsList(tags, list, doNext);
          });
        } else {
          console.log('CRON - Unknown action', cq);
          doNext();
        }
      });
    }, (testCb) => {
      Db.refresh('sessions*');
      if (Config.debug > 1) {
        console.log('CRON', cq.name, cq.creator, '- Continue process', singleEndTime, endTime);
      }
      return setImmediate(testCb, null, singleEndTime !== endTime);
    }, (err) => {
      cb(count, singleEndTime);
    });
  }

  // --------------------------------------------------------------------------
  static processCronQueries () {
    if (internals.cronRunning) {
      console.log('CRON - processQueries already running', CronAPIs.#qlworking);
      return;
    }
    internals.cronRunning = true;
    if (Config.debug) {
      console.log('CRON - cronRunning set to true');
    }

    let repeat;
    async.doWhilst(function (whilstCb) {
      repeat = false;
      Db.search('queries', 'query', { size: 1000 }, (err, data) => {
        if (err) {
          internals.cronRunning = false;
          console.log('CRON - processCronQueries', err);
          return setImmediate(whilstCb, err);
        }

        const queries = {};
        data.hits.hits.forEach(function (item) {
          if (item._id === 'primary-viewer') { return; }
          queries[item._id] = item._source;
        });

        // Delayed by the max Timeout
        const endTime = Math.floor(Date.now() / 1000) - internals.cronTimeout;

        // Go thru the queries, fetch the user, make the query
        async.eachSeries(Object.keys(queries), (qid, forQueriesCb) => {
          const cq = queries[qid];
          let cluster = null;

          if (Config.debug > 1) {
            console.log('CRON - Running', qid, cq);
          }

          if (!cq.enabled || endTime < cq.lpValue) {
            return forQueriesCb();
          }

          if (cq.action.indexOf('forward:') === 0) {
            cluster = cq.action.substring(8);
          }

          ViewerUtils.getUserCacheIncAnon(cq.creator, async (err, user) => {
            if (err && !user) {
              return forQueriesCb();
            }
            if (!user) {
              console.log(`CRON - User ${cq.creator} doesn't exist`);
              return forQueriesCb(null);
            }
            if (!user.enabled) {
              console.log(`CRON - User '${cq.creator}' has been disabled on users tab, either delete their cron jobs or enable them`);
              return forQueriesCb();
            }

            const options = {
              user,
              cluster,
              saveId: Config.nodeName() + '-' + new Date().getTime().toString(36),
              tags: cq.tags.replace(/[^-a-zA-Z0-9_:,]/g, ''),
              qid
            };

            let shortcuts;
            try { // try to fetch shortcuts
              shortcuts = await Db.getShortcutsCache(user);
            } catch (err) { // don't need to do anything, there will just be no
              // shortcuts sent to the parser. but still log the error.
              console.log('ERROR CRON - fetching shortcuts cache when processing periodic query', err);
            }

            // always complete building the query regardless of shortcuts
            arkimeparser.parser.yy = {
              emailSearch: user.emailSearch === true,
              fieldsMap: Config.getFieldsMap(),
              dbFieldsMap: Config.getDBFieldsMap(),
              prefix: internals.prefix,
              shortcuts,
              shortcutTypeMap: internals.shortcutTypeMap
            };

            const query = {
              from: 0,
              size: 1000,
              query: { bool: { filter: [{}] } },
              _source: ['_id', 'node']
            };

            try {
              query.query.bool.filter.push(arkimeparser.parse(cq.query));
            } catch (e) {
              console.log("CRON - Couldn't compile periodic query expression", cq, e);
              return forQueriesCb();
            }

            if (user.getExpression()) {
              try {
                // Expression was set by admin, so assume email search ok
                arkimeparser.parser.yy.emailSearch = true;
                const userExpression = arkimeparser.parse(user.getExpression());
                query.query.bool.filter.push(userExpression);
              } catch (e) {
                console.log("CRON - Couldn't compile user forced expression", user.getExpression(), e);
                return forQueriesCb();
              }
            }

            ViewerUtils.lookupQueryItems(query.query.bool.filter, (lerr) => {
              CronAPIs.#processCronQuery(cq, options, query, endTime, (count, lpValue) => {
                if (Config.debug > 1) {
                  console.log('CRON - setting lpValue', new Date(lpValue * 1000));
                }
                // Do the OpenSearch/Elasticsearch update
                const doc = {
                  doc: {
                    lpValue,
                    lastRun: Math.floor(Date.now() / 1000),
                    count: (cq.count || 0) + count,
                    lastCount: count
                  }
                };

                async function continueProcess () {
                  try {
                    await Db.update('queries', 'query', qid, doc, { refresh: true });
                  } catch (err) {
                    console.log('ERROR CRON - updating query', err);
                  }
                  if (lpValue !== endTime) { repeat = true; }
                  return forQueriesCb();
                }

                // issue alert via notifier if the count has changed and it has been at least 10 minutes
                if (cq.notifier && count && cq.count !== doc.doc.count &&
                  (!cq.lastNotified || (Math.floor(Date.now() / 1000) - cq.lastNotified >= 600))) {
                  const newMatchCount = cq.lastNotifiedCount ? (doc.doc.count - cq.lastNotifiedCount) : doc.doc.count;
                  doc.doc.lastNotifiedCount = doc.doc.count;

                  let urlPath = 'sessions?expression=';
                  const tags = cq.tags.split(',');
                  for (let t = 0, tlen = tags.length; t < tlen; t++) {
                    const tag = tags[t];
                    urlPath += `tags%20%3D%3D%20${tag}`; // encoded ' == '
                    if (t !== tlen - 1) { urlPath += '%20%26%26%20'; } // encoded ' && '
                  }

                  const message = `
  *${cq.name}* periodic query match alert:
  *${newMatchCount} new* matches
  *${doc.doc.count} total* matches
  ${Config.arkimeWebURL()}${urlPath}${cq.description ? '\n' + cq.description : ''}
                  `;

                  Db.refresh('*'); // Before sending alert make sure everything has been refreshed
                  Notifier.issueAlert(cq.notifier, message, continueProcess);
                } else {
                  return continueProcess();
                }
              });
            });
          });
        }, (err) => {
          if (Config.debug > 1) {
            console.log('CRON - Finished one pass of all crons');
          }
          return setImmediate(whilstCb, err);
        });
      });
    }, (testCb) => {
      if (Config.debug > 1) {
        console.log('CRON - Process again: ', repeat);
      }
      return setImmediate(testCb, null, repeat);
    }, (err) => {
      if (Config.debug) {
        console.log('CRON - Should be up to date');
      }
      internals.cronRunning = false;
    });
  }
}

module.exports = CronAPIs;
const HuntAPIs = require('./apiHunts');
