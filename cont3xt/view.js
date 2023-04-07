/******************************************************************************/
/* view.js  -- Deal with integration views
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

const ArkimeUtil = require('../common/arkimeUtil');

class View {
  constructor (data) {
    Object.assign(this, data);
  }

  /**
   * A Cont3xt View object
   *
   * Cont3xt Views are used to save lists of integrations to apply to a query.
   * @typedef Cont3xtView
   * @type {object}
   * @param {string} _id - The id of the view
   * @param {string} name - The name of the view
   * @param {string} creator - The creator of the view
   * @param {string[]} integrations - The array of integration names in this view
   * @param {array} editRoles - The Arkime roles that can edit this view
   * @param {array} viewRoles - The Arkime roles that can view this view
   * @param {boolean} _editable - Whether the logged in user is allowed to edit this view
   * @param {boolean} _viewable - Whether the logged in user is allowed to view this view
   */

  /**
   * GET - /api/views
   *
   * Returns views that the requesting user is allowed to view.
   * @name /views
   * @returns {Cont3xtView[]} views - An array of views that the logged in user can view
   * @returns {boolean} success - True if the request was successful, false otherwise
   */
  static async apiGet (req, res, next) {
    const all = req.query.all && req.user.hasRole('cont3xtAdmin');
    const roles = await req.user.getRoles();
    const views = await Db.getMatchingViews(req.user.userId, [...roles], all);

    // Set editable on any views that the user is allowed to edit
    for (const view of views) {
      view._editable = view.creator === req.user.userId || req.user.hasRole(view.editRoles);
      view._viewable = view.creator === req.user.userId || req.user.hasRole(view.viewRoles);
    }

    res.send({ success: true, views });
  }

  /**
   * Verify the view, returns error msg on failure
   */
  static verifyView (view) {
    view = (
      ({ // only allow these properties in views
        // eslint-disable-next-line no-shadow
        name, integrations, viewRoles, editRoles, creator
      }) => ({ name, integrations, viewRoles, editRoles, creator })
    )(view);

    if (!ArkimeUtil.isString(view.name)) {
      return 'Missing name';
    }

    view.name = ArkimeUtil.removeSpecialChars(view.name);

    if (view.viewRoles !== undefined) {
      if (!Array.isArray(view.viewRoles)) { return { msg: 'viewRoles must be array' }; }

      for (const viewRole of view.viewRoles) {
        if (!ArkimeUtil.isString(viewRole)) {
          return { msg: 'viewRoles must contain strings' };
        }
      }
    }

    if (view.editRoles !== undefined) {
      if (!Array.isArray(view.editRoles)) { return { msg: 'editRoles must be array' }; }

      for (const editRole of view.editRoles) {
        if (!ArkimeUtil.isString(editRole)) {
          return { msg: 'editRoles must contain strings' };
        }
      }
    }

    if (view.integrations !== undefined) {
      if (!Array.isArray(view.integrations)) { return { msg: 'integrations must be array' }; }

      for (const integration of view.integrations) {
        if (!ArkimeUtil.isString(integration)) {
          return { msg: 'integrations must contain strings' };
        }
      }
    }

    return { view };
  }

  /**
   * POST - /api/view
   *
   * Creates a new view
   * @name /view
   * @param {Cont3xtView} views - The view to create
   * @returns {boolean} success - True if the request was successful, false otherwise
   * @returns {string} text - The success/error message to (optionally) display to the user
   */
  static async apiCreate (req, res, next) {
    const receivedView = req.body;
    receivedView.creator = req.user.userId;

    const { view, msg } = View.verifyView(receivedView);
    if (msg) {
      return res.send({ success: false, text: msg });
    }

    const result = await Db.putView(null, view);
    if (!result) {
      return res.send({ success: false, text: 'ES Error' });
    }

    view._id = result;
    view._editable = true;
    view._viewable = true;

    return res.send({ success: true, text: 'Success', view });
  }

  /**
   * PUT - /api/view/:id
   *
   * Updates a view
   * @name /view/:id
   * @param {Cont3xtView} views - The view to update
   * @returns {boolean} success - True if the request was successful, false otherwise
   * @returns {string} text - The success/error message to (optionally) display to the user
   */
  static async apiUpdate (req, res, next) {
    const dbView = await Db.getView(req.params.id);
    if (!dbView) {
      return res.send({ success: false, text: 'View not found' });
    }

    if (dbView.creator !== req.user.userId && !(req.user.hasRole(dbView.editRoles)) && !req.user.hasRole('cont3xtAdmin')) {
      return res.send({ success: false, text: 'Permission denied' });
    }

    const receivedView = req.body;
    receivedView.creator = dbView.creator; // Make sure the creator doesn't get changed

    const { view, msg } = View.verifyView(receivedView);
    if (msg) {
      return res.send({ success: false, text: msg });
    }

    try {
      const results = await Db.putView(req.params.id, view);
      if (!results) {
        return res.send({ success: false, text: 'ES Error' });
      }
      return res.send({ success: true, text: 'Success' });
    } catch (err) {
      return res.send({ success: false, text: err.toString() });
    }
  }

  /**
   * DELETE - /api/view/:id
   *
   * Deletes a view
   * @name /view/:id
   * @param {Cont3xtView} views - The view to delete
   * @returns {boolean} success - True if the request was successful, false otherwise
   * @returns {string} text - The success/error message to (optionally) display to the user
   */
  static async apiDelete (req, res, next) {
    const view = await Db.getView(req.params.id);
    if (!view) {
      return res.send({ success: false, text: 'View not found' });
    }

    if (view.creator !== req.user.userId && !(req.user.hasRole(view.editRoles)) && !req.user.hasRole('cont3xtAdmin')) {
      return res.send({ success: false, text: 'Permission denied' });
    }

    const results = await Db.deleteView(req.params.id, req.body);
    if (!results) {
      return res.send({ success: false, text: 'ES Error' });
    }
    return res.send({ success: true, text: 'Success' });
  }
}

module.exports = View;

// Must be at bottom to avoid circular dependency
const Db = require('./db.js');
