'use strict';

const Db = require('./db');
const util = require('util');
const User = require('../common/user');
const ArkimeUtil = require('../common/arkimeUtil');

class View {
  static async getViews (req) {
    const user = req.settingUser;
    if (!user) { return { data: [], recordsTotal: 0, recordsFiltered: 0 }; }

    const roles = [...await user.getRoles()]; // es requries an array for terms search

    // only get views for setting user or shared
    const query = {
      query: {
        bool: {
          filter: [
            {
              bool: {
                should: [
                  { terms: { roles } }, // shared via user role
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

      if (user.userId !== result.user && !user.hasRole('arkimeAdmin')) {
        // remove sensitive information for users this view is shared with (except arkimeAdmin)
        delete result.users;
        delete result.roles;
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
   * A database view that can be applied to any search.
   *
   * @typedef ArkimeView
   * @type {object}
   * @param {string} name - The name of the view.
   * @param {string} expression - The search expression to filter sessions.
   * @param {ArkimeColumnConfig} sessionsColConfig - The Sessions column configuration to apply to the Sessions table when applying the view.
   * @param {string} user - The user ID of the user who created the view.
   * @property {Arrray} users - The list of userIds who have access to use this view.
   * @property {Array} roles - The list of roles who have access to use this view.
   */

  /**
   * GET - /api/views
   *
   * Retrieves an Arkime views that a user can view.
   * @name /views
   * @returns {ArkimeView[]} views - A list of views a user has configured or has been shared.
   */
  static async apiGetViews (req, res) {
    const views = await View.getViews(req);
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
    if (typeof req.body.name !== 'string') {
      return res.serverError(403, 'Missing view name');
    }

    if (typeof req.body.expression !== 'string') {
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

      req.body.id = id;
      req.body.users = req.body.users.join(',');
      return res.send(JSON.stringify({
        success: true,
        view: req.body,
        text: 'Created view!',
        invalidUsers: users.invalidUsers
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
      const { body: dbView } = await Db.getView(req.params.id);

      // only allow admins or view creator to delete view
      if (!req.user.hasRole('arkimeAdmin') && req.settingUser.userId !== dbView._source.user) {
        return res.serverError(403, 'Permission denied');
      }

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
    if (typeof req.body.name !== 'string') {
      return res.serverError(403, 'Missing view name');
    }

    if (typeof req.body.expression !== 'string') {
      return res.serverError(403, 'Missing view expression');
    }

    const view = req.body;

    try {
      const { body: dbView } = await Db.getView(req.params.id);

      // only allow admins or view creator can update view
      if (!req.user.hasRole('arkimeAdmin') && req.settingUser.userId !== dbView._source.user) {
        return res.serverError(403, 'Permission denied');
      }

      // can't update creator of the view or the id
      view.user = dbView._source.user;
      if (view.id) { delete view.id; }

      // comma/newline separated value -> array of values
      let users = ArkimeUtil.commaOrNewlineStringToArray(view.users || '');
      users = await User.validateUserIds(users);
      view.users = users.validUsers;

      try {
        await Db.setView(req.params.id, view);
        view.users = view.users.join(',');
        view.id = req.params.id;

        return res.send(JSON.stringify({
          view,
          success: true,
          text: 'Updated view!',
          invalidUsers: users.invalidUsers
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

module.exports = View;
