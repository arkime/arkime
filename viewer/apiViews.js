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
   * Retrieves an Arkime views that a user can view.
   */
  static async getViews (req) {
    const user = req.settingUser;
    if (!user) { return { data: [], recordsTotal: 0, recordsFiltered: 0 }; }

    const roles = [...await user.getRoles()]; // es requires an array for terms search

    // only get views for setting user or shared
    const query = {
      query: {
        bool: {
          filter: [
            {
              bool: {
                should: [
                  { terms: { roles } }, // shared via user role
                  { terms: { editRoles: roles } }, // shared via user editRoles
                  { term: { users: user.userId } }, // shared via userId
                  { term: { user: user.userId } } // created by this user
                ]
              }
            }
          ]
        }
      },
      sort: {},
      from: req.query.start || 0,
      size: req.query.length || 50
    };

    if (req.query.all && roles.includes('arkimeAdmin')) {
      query.query.bool.filter = []; // remove sharing restrictions
    }

    query.sort[req.query.sort || 'name'] = {
      order: req.query.desc === 'true' ? 'desc' : 'asc'
    };

    if (req.query.searchTerm) {
      query.query.bool.filter.push({
        wildcard: { name: '*' + req.query.searchTerm + '*' }
      });
    }

    const { body: { hits: views } } = await Db.searchViews(query);

    delete query.sort;
    delete query.size;
    delete query.from;
    const { body: { count: total } } = await Db.numberOfViews(query);

    const results = views.hits.map((view) => {
      const id = view._id;
      const result = view._source;

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

    return { data: results, recordsTotal: views.total, recordsFiltered: total };
  }

  /**
   * checks a user's permission to access a view to update/delete
   * only allow admins, editors, or creator can update/delete view
   */
  static async checkViewAccess (req, res, next) {
    if (req.user.hasRole('arkimeAdmin')) { // an admin can do anything
      return next();
    } else {
      try {
        const { body: { _source: view } } = await Db.getView(req.params.id);

        if (view.user === req.settingUser.userId || req.settingUser.hasRole(view.editRoles)) {
          return next();
        }

        return res.serverError(403, 'Permission denied');
      } catch (err) {
        return res.serverError(403, 'Unknown view');
      }
    }
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
   * Retrieves an Arkime views that a user can view.
   * @name /views
   * @returns {ArkimeView[]} views - A list of views a user has configured or has been shared.
   */
  static async apiGetViews (req, res) {
    const views = await ViewAPIs.getViews(req);
    res.send(views);
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
      return res.serverError(403, 'Missing view name');
    }

    if (!ArkimeUtil.isString(req.body.expression)) {
      return res.serverError(403, 'Missing view expression');
    }

    const user = req.settingUser;

    req.body.user = user.userId;

    // comma/newline separated value -> array of values
    let users = ArkimeUtil.commaOrNewlineStringToArray(req.body.users || '');
    users = await User.validateUserIds(users);
    req.body.users = users.validUsers;

    try {
      const { body: { _id: id } } = await Db.createView(req.body);
      const { body: { _source: view } } = await Db.getView(id);

      view.id = id;
      view.users = view.users.join(',');

      return res.send(JSON.stringify({
        success: true,
        view,
        text: 'Created view!',
        invalidUsers: ArkimeUtil.safeStr(users.invalidUsers)
      }));
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/view (createView)`, util.inspect(err, false, 50));
      return res.serverError(500, 'Error creating view');
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
      res.send(JSON.stringify({
        success: true,
        text: 'Deleted view successfully'
      }));
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/view/%s (deleteView)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Error deleting notifier');
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
      return res.serverError(403, 'Missing view name');
    }

    if (!ArkimeUtil.isString(req.body.expression)) {
      return res.serverError(403, 'Missing view expression');
    }

    const view = req.body;

    try {
      const { body: dbView } = await Db.getView(req.params.id);

      // transfer ownership of view (admin and creator only)
      if (view.user && view.user !== dbView._source.user && ArkimeUtil.isString(view.user)) {
        if (req.settingUser.userId !== dbView._source.user && !req.settingUser.hasRole('arkimeAdmin')) {
          return res.serverError(403, 'Permission denied');
        }

        // check if user is valid before updating it
        // comma/newline separated value -> array of values
        let user = ArkimeUtil.commaOrNewlineStringToArray(view.user);
        user = await User.validateUserIds(user);

        if (user.invalidUsers?.length) {
          return res.serverError(404, `Invalid user: ${user.invalidUsers[0]}`);
        }
        if (user.validUsers?.length) {
          view.user = user.validUsers[0];
        } else {
          return res.serverError(404, 'Cannot find valid user');
        }
      } else { // keep the same owner
        view.user = dbView._source.user;
      }

      // can't update the id
      if (view.id) { delete view.id; }

      // comma/newline separated value -> array of values
      let users = ArkimeUtil.commaOrNewlineStringToArray(view.users || '');
      users = await User.validateUserIds(users);
      view.users = users.validUsers;

      try {
        await Db.setView(req.params.id, view);
        const { body: { _source: newView } } = await Db.getView(req.params.id);
        newView.users = newView.users.join(',');
        newView.id = dbView._id;

        return res.send(JSON.stringify({
          view: newView,
          success: true,
          text: 'Updated view!',
          invalidUsers: ArkimeUtil.safeStr(users.invalidUsers)
        }));
      } catch (err) {
        console.log(`ERROR - ${req.method} /api/view/%s (setView)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
        return res.serverError(500, 'Error updating view');
      }
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/view/%s (getView)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Fetching view to update failed');
    }
  }
}

module.exports = ViewAPIs;
