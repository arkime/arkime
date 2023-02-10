/******************************************************************************/
/* audit.js  -- Cont3xt history
 *
 * Copyright Yahoo Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
'use strict';

class Audit {
  constructor (data) {
    Object.assign(this, data);
  }

  /**
   * Initialization sets up periodic cleaning of expired audit history logs
   * @param {number} options.debug=0 The debug level to use
   * @param {number} options.expireHistoryDays How long to keep history
   */
  static initialize (options) {
    if (options.debug > 1) {
      console.log('Audit.initialize', options);
    }
    let { expireHistoryDays } = options;
    if (typeof expireHistoryDays === 'string') {
      expireHistoryDays = parseFloat(expireHistoryDays);
    }
    if (isNaN(expireHistoryDays)) {
      console.log(`ERROR - expireHistoryDays (${expireHistoryDays}) is not a number. History will NOT be cleaned!`);
      return;
    }

    const hourMs = 3600000;
    const expireHistoryMsBack = Math.ceil(expireHistoryDays * 24 * hourMs);

    const deleteExpiredAudits = () => {
      Db.deleteExpiredAudits(Date.now() - expireHistoryMsBack).then((numDeleted) => {
        console.log(`Successful deletion of ${numDeleted} expired history logs`);
      }).catch((err) => {
        console.log('ERROR - Failed to delete expired history logs.', err);
      });
    };

    // fire off one initial cleaning, then begin hourly interval
    deleteExpiredAudits();
    setInterval(deleteExpiredAudits, hourMs);
  }

  /**
   * A Cont3xt Audit Object
   * @param {string} _id
   * @param {string} userId
   * @param {number} issuedAt
   * @param {number} took
   * @param {number} resultCount
   * @param {string} iType
   * @param {string} indicator
   * @param {string[]} tags
   * @param {string | undefined} viewId
   */

  /**
   * Creates a new history audit log
   * @name /audit
   * @param {Audit} audit - The history entry to create
   * @returns {Promise} - The promise that either resolves or rejects in error
   */
  static async create (audit) {
    return new Promise((resolve, reject) => {
      const msg = Audit.verifyAudit(audit);

      if (msg) {
        reject(msg);
      }

      Db.putAudit(null, audit).then((results) => {
        if (!results) {
          reject('ES Error');
        } else {
          resolve(results);
        }
      }).catch((err) => reject('ES Error', err));
    });
  }

  // Verify the given audit entry, returns error msg on failure, null otherwise
  static verifyAudit (audit) {
    if (typeof (audit.userId) !== 'string') { return 'must have field userId of type string'; }
    if (typeof (audit.issuedAt) !== 'number') { return 'must have field issuedAt of type number (milliseconds)'; }
    if (typeof (audit.took) !== 'number') { return 'must have field took of type number (milliseconds)'; }
    if (typeof (audit.resultCount) !== 'number') { return 'must have field resultCount of type number (milliseconds)'; }
    if (typeof (audit.iType) !== 'string') { return 'must have field iType of type string'; }
    if (typeof (audit.indicator) !== 'string') { return 'must have field indicator of type string'; }
    if (!Array.isArray(audit.tags)) { return 'must have field tags of type Array'; }
    if (typeof (audit.viewId) !== 'string' && audit.viewId !== undefined) { return 'field viewId must be of type string or undefined'; }

    return null;
  }

  /**
   * GET - /api/audits
   *
   * Returns list of audit logs (sorted by issuedAt) that the requesting user is allowed to view.
   * @name /audits
   * @param {string} searchTerm - an optional query parameter to filter on indicator, iType, and tags
   * @param {string} startMs - an optional query parameter to specify the start of results (milliseconds since Unix EPOC)
   * @param {string} stopMs - an optional query parameter to specify the end of results (milliseconds since Unix EPOC)
   * @param {string} seeAll - an optional query parameter to request viewing all history (only works for admin users)
   * @returns {Audit[]} audits - A sorted array of audit logs that the logged-in user can view
   * @returns {boolean} success - True if the request was successful, false otherwise
   */
  static async apiGet (req, res, next) {
    const roles = await req.user.getRoles();
    const audits = await Db.getMatchingAudits(req.user.userId, [...roles], req.query);

    // sorts chronologically, according to issuedAt (milliseconds)
    audits.sort((a, b) => b.issuedAt - a.issuedAt);

    res.send({ success: true, audits });
  }

  /**
   * DELETE - /api/audit/:id
   *
   * Delete a log from history
   * @name /audit/:id
   * @returns {boolean} success - Whether the delete history log operation was successful.
   * @returns {string} text - The success/error message to (optionally) display.
   */
  static async apiDelete (req, res, next) {
    const audit = await Db.getAudit(req.params.id);
    const roles = [...await req.user.getRoles()];

    if (!audit) {
      return res.send({ success: false, text: 'History log not found' });
    }

    // permissions ---------------------------
    if (!req.user.removeEnabled) {
      return res.send({ success: false, text: 'Can not delete a history log when user has data removal disabled.' });
    }

    if (req.user.userId !== audit.userId && !roles?.includes('cont3xtAdmin')) {
      return res.send({ success: false, text: 'User does not have permission to delete this log.' });
    }
    // ----------------------------------------

    const results = await Db.implementation.deleteAudit(req.params.id);

    if (!results) {
      return res.send({ success: false, text: 'Database error' });
    }

    res.send({ success: true, text: 'Success' });
  }
}

module.exports = Audit;

// Must be at bottom to avoid circular dependency
const Db = require('./db.js');
