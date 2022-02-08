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

class View {
  constructor (data) {
    Object.assign(this, data);
  }

  save () {
  }

  /**
   * Return views a user can see
   */
  static async apiGet (req, res, next) {
    const roles = await req.user.getRoles();
    const views = await Db.getMatchingViews(req.user.userId, [...roles]);

    // Set editable on any views that the user is allowed to edit
    for (const view of views) {
      view._editable = view.creator === req.user.userId || req.user.hasRole(view.editRoles);
      view._viewable = view.creator === req.user.userId || req.user.hasRole(view.viewRoles);
    }

    res.send({ success: true, views: views });
  }

  /**
   * Verify the link group, returns error msg on failure
   */
  static verifyView (view) {
    // TODO: Check roles
    if (typeof (view.name) !== 'string') {
      return 'Missing name';
    }

    if (view.viewRoles !== undefined && !Array.isArray(view.viewRoles)) {
      return 'viewRoles must be array';
    }

    if (view.editRoles !== undefined && !Array.isArray(view.editRoles)) {
      return 'viewRoles must be array';
    }

    if (view.integrations !== undefined && !Array.isArray(view.integrations)) {
      return 'integrations must be array';
    }

    return null;
  }

  /**
   * Create new view
   */
  static async apiCreate (req, res, next) {
    const view = req.body;
    view.creator = req.user.userId;

    const msg = View.verifyView(view);
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
   * Update a view
   */
  static async apiUpdate (req, res, next) {
    const dbView = await Db.getView(req.params.id);
    if (!dbView) {
      return res.send({ success: false, text: 'View not found' });
    }

    if (dbView.creator !== req.user.userId && !(req.user.hasRole(dbView.editRoles))) {
      return res.send({ success: false, text: 'Permission denied' });
    }

    const view = req.body;
    view.creator = dbView.creator; // Make sure the creator doesn't get changed

    const msg = View.verifyView(view);
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
   * Delete a view
   */
  static async apiDelete (req, res, next) {
    const view = await Db.getView(req.params.id);
    if (!view) {
      return res.send({ success: false, text: 'View not found' });
    }

    if (view.creator !== req.user.userId && !(req.user.hasRole(view.editRoles))) {
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
