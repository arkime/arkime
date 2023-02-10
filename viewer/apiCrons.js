'use strict';

const util = require('util');
const Db = require('./db');
const Config = require('./config');
const User = require('../common/user');
const ArkimeUtil = require('../common/arkimeUtil');

class Cron {
  static #process;

  static initialize (options) {
    Cron.#process = options.processCronQueries;
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
   * @param {number} lastNotified - The time that this query last sent a notification to the notifier. Only notifies every 10 mintues. Format is seconds since Unix EPOC.
   * @param {number} lastNotifiedCount - The count of sessions that matched since the last notification was sent.
   * @param {string} description - The description of this query.
   * @param {number} created - The time that this query was created. Format is seconds since Unix EPOC.
   * @param {number} lastToggled - The time that this query was enabled or disabled. Format is seconds since Unix EPOC.
   * @param {string} lastToggledBy - The user who last enabled or disabled this query.
   */

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

    const roles = [...await user.getRoles()]; // es requries an array for terms search

    const query = {
      size: 1000,
      sort: { created: { order: 'asc' } },
      query: {
        bool: {
          filter: [
            {
              bool: {
                should: [
                  { terms: { roles } }, // shared via user role
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
      const { body: data } = await Db.getMinValue(['sessions2-*', 'sessions3-*'], '@timestamp');
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

      if (Config.get('cronQueries', false)) {
        Cron.#process();
      }

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
    const key = req.body.key;
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
        enabled: req.body.enabled
      }
    };

    if (ArkimeUtil.isString(req.body.notifier)) {
      doc.doc.notifier = req.body.notifier;
    }

    if (ArkimeUtil.isString(req.body.description)) {
      doc.doc.description = req.body.description;
    }

    try {
      const { body: { _source: cron } } = await Db.get('queries', 'query', key);

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

      if (Config.get('cronQueries', false)) { Cron.#process(); }

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

  /**
   * DELETE - /api/cron/:key
   *
   * Delete a periodic query.
   * @name /cron/:key
   * @returns {boolean} success - Whether the delete operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async deleteCron (req, res) {
    const key = req.body.key;

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
  }
}

module.exports = Cron;
