/******************************************************************************/
/* apiHistory.js -- api calls for history tab
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const Db = require('./db.js');
const util = require('util');
const ArkimeUtil = require('../common/arkimeUtil');

class HistoryAPIs {
  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  /**
   * The history object to describe user client requests.
   *
   * @typedef History
   * @type {object}
   * @param {string} uiPage - The client application page that the user accessed to make the request.
   * @param {string} userId - The ID of the user that made the request.
   * @param {string} method - The HTTP method that the request used.
   * @param {string} api - The API endpoint of the request.
   * @param {string} expression - The sessions search expression used in the request.
   * @param {ArkimeView} view - The view applied to the request.
   * @param {number} timestamp - The time that the request was made. Format is seconds since Unix EPOCH.
   * @param {number} range - The date range of the request. Range is described in hours, -1 means all.
   * @param {string} query - The query parameters of the request.
   * @param {number} queryTime - The time it took for the response to be returned after the request was issued.
   * @param {number} recordsTotal - The total number of items in the data set.
   * @param {number} recordsFiltered - The number of items returned from searching the dataset (before paging).
   * @param {number} recordsReturned - The number of items returned in the response (after paging).
   * @param {object} body - The request body.
   * @param {string} forcedExpression - The expression applied to the search as a result of a user's forced expression. Only visible to admins, normal users cannot see their forced expressions.
   */

  // --------------------------------------------------------------------------
  /**
   * GET - /api/histories
   *
   * Retrieves a list of histories, or user client requests to the APIs.
   * @name /histories
   * @param {number} date=1 - The number of hours of data to return (-1 means all data). Defaults to 1.
   * @param {number} startTime - If the date parameter is not set, this is the start time of data to return. Format is seconds since Unix EPOCH.
   * @param {number} stopTime  - If the date parameter is not set, this is the stop time of data to return. Format is seconds since Unix EPOCH.
   * @param {string} searchTerm - The search text to filter the history list by.
   * @param {number} length=1000 - The number of items to return. Defaults to 1,000.
   * @param {number} start=0 - The entry to start at. Defaults to 0.
   * @param {string} sortField=timestamp - The field to sort the results by.
   * @param {string} desc=true - Whether to sort the results descending or ascending. Default is descending.
   * @param {string} userId - The ID of a user to request history results for. Admin can retrieve all users. Normal users can only retrieve their own.
   * @returns {History[]} data - The list of history results.
   * @returns {number} recordsTotal - The total number of history results stored.
   * @returns {number} recordsFiltered - The number of history items matching query.
   */
  static async getHistories (req, res) {
    try {
      let userId;
      const isAdmin = req.user.hasRole('arkimeAdmin');
      if (isAdmin) { // user is an admin, they can view all history items
        // if the admin has requested a specific user
        if (req.query.userId) { userId = req.query.userId; }
      } else { // user isn't an admin, so they can only view their own history items
        if (req.query.userId && req.query.userId !== req.user.userId) {
          return res.serverError(403, 'Need admin privileges', 'api.history.needAdminPrivileges');
        }
        userId = req.user.userId;
      }

      const sortField = /^[A-Za-z_][A-Za-z0-9_.]*$/.test(req.query.sortField ?? '') ? req.query.sortField : 'timestamp';
      const params = {
        sort: { [sortField]: req.query.desc === 'true' ? 'desc' : 'asc' },
        from: +req.query.start || 0,
        size: Math.min(Math.max(+req.query.length || 1000, 0), 10000)
      };

      const and = [];
      if (ArkimeUtil.isString(req.query.searchTerm)) { // apply search term across whitelisted fields only
        and.push({ $or: ['expression', 'userId', 'api', 'view.name', 'view.expression'].map(field => ({ [field]: { $contains: req.query.searchTerm } })) });
      }

      if (userId) { // filter on userId, admins can use * and ?
        and.push({ userId: isAdmin ? { $wildcard: userId } : userId });
      }

      if (ArkimeUtil.isString(req.query.api)) { // filter on api endpoint
        and.push({ api: { $contains: req.query.api } });
      }

      if (ArkimeUtil.isString(req.query.exists)) {
        // each one is a subquery over every history row, the page has a handful of columns
        for (const field of [...new Set(req.query.exists.split(','))].slice(0, 20)) {
          if (/^[A-Za-z_][A-Za-z0-9_.]*$/.test(field)) {
            and.push({ [field]: { $exists: true } });
          }
        }
      }

      // filter history table by a time range
      if (req.query.startTime && req.query.stopTime) {
        if (!/^[0-9]+$/.test(req.query.startTime)) {
          req.query.startTime = Date.parse(req.query.startTime.replace('+', ' ')) / 1000;
        } else {
          req.query.startTime = parseInt(req.query.startTime, 10);
        }

        if (!/^[0-9]+$/.test(req.query.stopTime)) {
          req.query.stopTime = Date.parse(req.query.stopTime.replace('+', ' ')) / 1000;
        } else {
          req.query.stopTime = parseInt(req.query.stopTime, 10);
        }

        and.push({ timestamp: { $gte: req.query.startTime, $lte: req.query.stopTime } });
      }
      params.where = { $and: and };

      const [histories, total] = await Promise.all([
        Db.searchHistory(params, req.query.cluster),
        Db.countHistory(req.query.cluster)
      ]);

      const results = { total: histories.total, results: [] };
      for (const hit of histories.data) {
        const item = hit.doc;
        item.id = hit.id;
        item.index = hit.index;
        if (!isAdmin) {
          // remove forced expression and es query for reqs made by nonadmin users
          item.forcedExpression = undefined;
          item.esQuery = undefined;
        }
        results.results.push(item);
      }

      res.json({
        data: results.results,
        recordsTotal: total,
        recordsFiltered: results.total
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/histories`, util.inspect(err, false, 50));
      return res.serverError(500, 'Error retrieving history', 'api.history.retrieveFailed');
    }
  }

  // --------------------------------------------------------------------------
  /**
   * DELETE - /api/history/:id
   *
   * Deletes a history entry (admin only).
   * @name /history/:id
   * @returns {boolean} success - Whether the delete history operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async deleteHistory (req, res) {
    try {
      if (!await Db.deleteHistory(req.params.id, req.query.cluster)) {
        return res.serverError(404, 'History item not found', 'api.history.notFound');
      }
      return res.json({
        success: true,
        text: 'Deleted history item successfully',
        i18n: 'api.history.deletedSuccessfully'
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/history/%s`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Error deleting history item', 'api.history.deleteFailed');
    }
  }
}

module.exports = HistoryAPIs;
