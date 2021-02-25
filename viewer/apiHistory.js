'use strict';

module.exports = (Db) => {
  const module = {};

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
   * @param {number} timestamp - The time that the request was made. Format is seconds since Unix EPOC.
   * @param {number} range - The date range of the request. Range is described in hours, -1 means all.
   * @param {string} query - The query parameters of the request.
   * @param {number} queryTime - The time it took for the response to be returned after the request was issued.
   * @param {number} recordsTotal - The total number of items in the data set.
   * @param {number} recordsFiltered - The number of items returned from searching the dataset (before paging).
   * @param {number} recordsReturned - The number of items returned in the response (after paging).
   * @param {object} body - The request body.
   * @param {string} forcedExpression - The expression applied to the search as a result of a users forced expression. Only visible to admins, normal users cannot see their forced expressions.
   */

  /**
   * GET - /api/histories
   *
   * Retrieves a list of histories.
   * @name /histories
   * @param {number} date=1 - The number of hours of data to return (-1 means all data). Defaults to 1.
   * @param {number} startTime - If the date parameter is not set, this is the start time of data to return. Format is seconds since Unix EPOC.
   * @param {number} stopTime  - If the date parameter is not set, this is the stop time of data to return. Format is seconds since Unix EPOC.
   * @param {string} searchTerm - The search text to filter the history list by.
   * @param {number} length=100 - The number of items to return. Defaults to 1,000.
   * @param {number} start=0 - The entry to start at. Defaults to 0.
   * @param {string} sortField=timestamp - The field to sort the results by.
   * @param {string} desc=true - Whether to sort the results descending or ascending. Default is descending.
   * @param {string} userId - The ID of a user to request history results for. Admin can retrieve all users. Normal users can only retrieve their own.
   * @returns {History[]} data - The list of history results.
   * @returns {number} recordsTotal - The total number of history results stored.
   * @returns {number} recordsFiltered - The number of history items returned in this result.
   */
  module.getHistories = (req, res) => {
    let userId;
    if (req.user.createEnabled) { // user is an admin, they can view all logs
      // if the admin has requested a specific user
      if (req.query.userId) { userId = req.query.userId; }
    } else { // user isn't an admin, so they can only view their own logs
      if (req.query.userId && req.query.userId !== req.user.userId) { return res.molochError(403, 'Need admin privileges'); }
      userId = req.user.userId;
    }

    const query = {
      sort: {},
      from: +req.query.start || 0,
      size: +req.query.length || 1000
    };

    query.sort[req.query.sortField || 'timestamp'] = { order: req.query.desc === 'true' ? 'desc' : 'asc' };

    if (req.query.searchTerm || userId) {
      query.query = { bool: { must: [] } };

      if (req.query.searchTerm) { // apply search term
        query.query.bool.must.push({
          query_string: {
            query: req.query.searchTerm,
            fields: ['expression', 'userId', 'api', 'view.name', 'view.expression']
          }
        });
      }

      if (userId) { // filter on userId
        query.query.bool.must.push({
          wildcard: { userId: '*' + userId + '*' }
        });
      }
    }

    if (req.query.api) { // filter on api endpoint
      if (!query.query) { query.query = { bool: { must: [] } }; }
      query.query.bool.must.push({
        wildcard: { api: '*' + req.query.api + '*' }
      });
    }

    if (req.query.exists) {
      if (!query.query) { query.query = { bool: { must: [] } }; }
      const existsArr = req.query.exists.split(',');
      for (let i = 0, len = existsArr.length; i < len; ++i) {
        query.query.bool.must.push({
          exists: { field: existsArr[i] }
        });
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

      if (!query.query) { query.query = { bool: {} }; }
      query.query.bool.filter = [{
        range: {
          timestamp: {
            gte: req.query.startTime,
            lte: req.query.stopTime
          }
        }
      }];
    }

    Promise.all([
      Db.searchHistory(query),
      Db.numberOfLogs()
    ]).then(([logs, total]) => {
      if (logs.error) { throw logs.error; }

      const results = { total: logs.hits.total, results: [] };
      for (const hit of logs.hits.hits) {
        const log = hit._source;
        log.id = hit._id;
        log.index = hit._index;
        if (!req.user.createEnabled) {
          // remove forced expression for reqs made by nonadmin users
          log.forcedExpression = undefined;
        }
        results.results.push(log);
      }

      res.send({
        data: results.results,
        recordsTotal: total.count,
        recordsFiltered: results.total
      });
    }).catch(err => {
      console.log('ERROR - /history/logs', err);
      return res.molochError(500, 'Error retrieving log history - ' + err);
    });
  };

  /**
   * DELETE - /api/history/:id
   *
   * Deletes a history entry (admin only).
   * @name /history/:id
   * @param {string} index - The Elasticsearch index that the history item was stored in.
   * @returns {boolean} success - Whether the delete history operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.deleteHistory = (req, res) => {
    if (!req.query.index) {
      return res.molochError(403, 'Missing history index');
    }

    Db.deleteHistoryItem(req.params.id, req.query.index, (err, result) => {
      if (err || result.error) {
        console.log('ERROR - deleting history item', err || result.error);
        return res.molochError(500, 'Error deleting history item');
      } else {
        res.send(JSON.stringify({
          success: true,
          text: 'Deleted history item successfully'
        }));
      }
    });
  };

  return module;
};
