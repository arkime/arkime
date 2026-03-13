/* apiViews.js -- api calls for views
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const Db = require('./db');
const util = require('util');
const User = require('../common/user');
const ArkimeUtil = require('../common/arkimeUtil');

class ViewAPIs {
  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------
  /**
   * Retrieves Arkime views that a user can view.
   */
  static async getViews (req) {
    const user = req.settingUser;
    if (!user) { return { data: [], recordsTotal: 0, recordsFiltered: 0 }; }

    const roles = [...await user.getRoles()]; // es requires an array for terms search

    const allowedViewsSortFields = { name: 1, expression: 1, created: 1 };
    const params = {
      user: user.userId,
      roles,
      all: req.query.all && roles.includes('arkimeAdmin'),
      sortField: allowedViewsSortFields[req.query.sort] ? req.query.sort : 'name',
      sortOrder: req.query.desc === 'true' ? 'desc' : 'asc',
      from: req.query.start || 0,
      size: req.query.length || 50,
      searchTerm: req.query.searchTerm
    };

    const { data: views, total: recordsFiltered } = await Db.searchViews(params);
    const recordsTotal = await Db.numberOfViews({ ...params, searchTerm: undefined });

    const results = views.map((view) => {
      const id = view.id;
      const result = view.source;

      if ( // remove sensitive information for users this is shared with
        // (except creator, arkimeAdmin, and editors)
        user.userId !== result.user &&
        !user.hasRole('arkimeAdmin') &&
        !user.hasRole(result.editRoles)) {
        delete result.users;
        delete result.roles;
        delete result.editRoles;
      } else if (result.users) {
        // client expects a string
        result.users = result.users.join(',');
      }

      result.id = id;
      return result;
    });

    return { data: results, recordsTotal, recordsFiltered };
  }

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------

  /**
   * A database view that can be applied to any search.
   *
   * @typedef ArkimeView
   * @type {object}
   * @param {string} name - The name of the view.
   * @param {string} expression - The search expression to filter sessions.
   * @param {ArkimeColumnConfig} sessionsColConfig - The Sessions column configuration to apply to the Sessions table when applying the view.
   * @param {string} user - The user ID of the user who created the view.
   * @param {string} users - The list of userIds who have access to use this view.
   * @param {string[]} roles - The list of roles who have access to use this view.
   * @param {string[]} editRoles - The list of roles who have access to edit this view.
   */

  /**
   * GET - /api/views
   *
   * Retrieves Arkime views that a user can view.
   * @name /views
   * @returns {ArkimeView[]} views - A list of views a user has configured or has been shared.
   */
  static async apiGetViews (req, res) {
    const views = await ViewAPIs.getViews(req);
    res.json(views);
  }

  /**
   * POST - /api/view
   *
   * Creates an Arkime view.
   * @name /view
   * @returns {boolean} success - Whether the create view operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {string} viewName - The name of the new view.
   * @returns {ArkimeView} view - The new view data.
   */
  static async apiCreateView (req, res) {
    if (!ArkimeUtil.isString(req.body.name)) {
      return res.serverError(403, 'Missing view name', 'api.views.missingName');
    }

    if (!ArkimeUtil.isString(req.body.expression)) {
      return res.serverError(403, 'Missing view expression', 'api.views.missingExpression');
    }

    const user = req.settingUser;

    // comma/newline separated value -> array of values
    let users = ArkimeUtil.commaOrNewlineStringToArray(req.body.users || '');
    users = await User.validateUserIds(users);

    const doc = {
      name: req.body.name,
      expression: req.body.expression,
      user: user.userId,
      users: users.validUsers,
      roles: req.body.roles,
      editRoles: req.body.editRoles
    };

    if (req.body.sessionsColConfig !== undefined) {
      doc.sessionsColConfig = req.body.sessionsColConfig;
    }

    try {
      const id = await Db.createView(doc);
      const view = await Db.getView(id);

      view.id = id;
      view.users = view.users?.join(',') ?? '';

      return res.json({
        success: true,
        view,
        text: 'Created view!',
        i18n: 'api.views.created',
        invalidUsers: ArkimeUtil.safeStr(users.invalidUsers)
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/view (createView)`, util.inspect(err, false, 50));
      return res.serverError(500, 'Error creating view', 'api.views.errorCreating');
    }
  }

  /**
   * DELETE - /api/view/:id
   *
   * Deletes an Arkime view.
   * @name /view/:id
   * @returns {boolean} success - Whether the delete view operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async apiDeleteView (req, res) {
    try {
      await Db.deleteView(req.params.id);
      res.json({
        success: true,
        text: 'Deleted view successfully',
        i18n: 'api.views.deleted'
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/view/%s (deleteView)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Error deleting view', 'api.views.errorDeleting');
    }
  }

  /**
   * PUT - /api/view/:id
   *
   * Updates an Arkime view.
   * @name /view/:id
   * @returns {boolean} success - Whether the update view operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async apiUpdateView (req, res) {
    // make sure all the necessary data is included in the body
    if (!ArkimeUtil.isString(req.body.name)) {
      return res.serverError(403, 'Missing view name', 'api.views.missingName');
    }

    if (!ArkimeUtil.isString(req.body.expression)) {
      return res.serverError(403, 'Missing view expression', 'api.views.missingExpression');
    }

    try {
      const dbViewSource = await Db.getView(req.params.id);

      const doc = {
        name: req.body.name,
        expression: req.body.expression,
        users: dbViewSource.users,
        roles: req.body.roles,
        editRoles: req.body.editRoles,
        user: req.body.user ?? dbViewSource.user
      };

      if (req.body.sessionsColConfig !== undefined) {
        doc.sessionsColConfig = req.body.sessionsColConfig;
      } else if (dbViewSource.sessionsColConfig !== undefined) {
        doc.sessionsColConfig = dbViewSource.sessionsColConfig;
      }

      // checks permission and validates new owner (only creator/admin can transfer)
      if (!await User.setOwner(req, res, doc, dbViewSource, 'user')) {
        return;
      }

      // comma/newline separated value -> array of values
      let users = ArkimeUtil.commaOrNewlineStringToArray(req.body.users || '');
      users = await User.validateUserIds(users);
      doc.users = users.validUsers;

      try {
        await Db.setView(req.params.id, doc);
        const newView = await Db.getView(req.params.id);
        newView.users = newView.users?.join(',') ?? '';
        newView.id = req.params.id;

        return res.json({
          view: newView,
          success: true,
          text: 'Updated view!',
          i18n: 'api.views.updated',
          invalidUsers: ArkimeUtil.safeStr(users.invalidUsers)
        });
      } catch (err) {
        console.log(`ERROR - ${req.method} /api/view/%s (setView)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
        return res.serverError(500, 'Error updating view', 'api.views.errorUpdating');
      }
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/view/%s (getView)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Fetching view to update failed', 'api.views.errorFetching');
    }
  }
}

module.exports = ViewAPIs;
