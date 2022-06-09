'use strict';

const Db = require('./db');
const util = require('util');
const User = require('../common/user');
const ArkimeUtil = require('../common/arkimeUtil');

class View {
  static async getViews (req) {
    const user = req.settingUser;
    if (!user) { return {}; }

    const roles = [...user._allRoles.keys()]; // es requries an array for terms search

    // only get shortcuts for setting user or shared
    const query = {
      query: {
        bool: {
          filter: [
            {
              bool: {
                should: [
                  { terms: { roles: roles } }, // shared via user role
                  { term: { users: user.userId } }, // shared via userId
                  { term: { user: user.userId } } // created by this user
                ]
              }
            }
          ]
        }
      },
      size: 1000
    };

    const { body: { hits: { hits: views } } } = await Db.searchViews(query);

    const results = views.map((view) => {
      const id = view._id;
      const result = view._source;

      if (user.userId !== result.userId && !user.hasRole('arkimeAdmin')) {
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

    return results;
  }

  /**
   * A database view that can be applied to any search.
   *
   * @typedef ArkimeView
   * @type {object}
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
    if (!req.body.name) {
      return res.serverError(403, 'Missing view name');
    }

    if (!req.body.expression) {
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
      await Db.deleteView(req.params.id);
      res.send(JSON.stringify({
        success: true,
        text: 'Deleted view successfully'
      }));
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/view/${req.params.id} (deleteView)`, util.inspect(err, false, 50));
      return res.serverError(500, 'Error deleting notifier');
    }
  }
}

module.exports = View;
