/******************************************************************************/
/* audit.js  -- Deal with a audit logs
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
   * A Cont3xt Audit Object
   * @param {string} _id
   * @param {string} userId
   * @param {Date} issuedAt
   * @param {string} iType
   * @param {string} indicator
   * @param {string[]} tags
   * @param {Object} queryOptions
   */

  /**
   * PUT - /api/audit
   *
   * Creates a new link group
   * @name /audit
   * @param {Audit} audit - The history entry to create
   * @returns {boolean} success - True if the request was successful, false otherwise
   * @returns {string} text - The success/error message to (optionally) display to the user
   */
  static async apiCreate (req, res, next) {
    const audit = { userId: req.user.userId, ...req.body };

    const msg = Audit.verifyAudit(audit);
    if (msg) {
      return res.send({ success: false, text: msg });
    }

    const results = await Db.putAudit(null, audit);
    if (!results) {
      return res.send({ success: false, text: 'ES Error' }); // TODO: on 'api' methods, change 'ES Error' to 'Database Error', or determine database at runtime
    }
    return res.send({ success: true, text: 'Success' });
  }

  // Verify the given audit entry, returns error msg on failure, null otherwise
  static verifyAudit (audit) {
    const isObject = (obj) => {
      return (typeof obj === 'object' && !Array.isArray(obj) && obj != null);
    };
    if (typeof (audit.userId) !== 'string') { return 'must have field userId of type string'; }
    if (typeof (audit.issuedAt) !== 'number') { return 'must have field issuedAt of type number (milliseconds)'; }
    if (typeof (audit.iType) !== 'string') { return 'must have field iType of type string'; }
    if (typeof (audit.indicator) !== 'string') { return 'must have field indicator of type string'; }
    if (!Array.isArray(audit.tags)) { return 'must have field tags of type Array'; }
    if (!isObject(audit.queryOptions)) { return 'must have field queryOptions of type Object'; }

    return null;
  }

  /**
   * GET - /api/audits
   *
   * Returns list of audit logs (sorted by issuedAt) that the requesting user is allowed to view.
   * @name /audits
   * @param {String} dateRange - A json-stringified object holding the requested time range, of shape: { start, end }
   * @returns {Audit[]} audits - A sorted array of audit logs that the logged-in user can view
   * @returns {boolean} success - True if the request was successful, false otherwise
   */
  static async apiGet (req, res, next) {
    const roles = await req.user.getRoles();
    const dateRange = req.params.dateRange ? JSON.parse(req.params.dateRange) : undefined;
    const audits = await Db.getMatchingAudits(req.user.userId, [...roles], dateRange);

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

    if (!audit) {
      return res.send({ success: false, text: 'History log not found' });
    }

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
