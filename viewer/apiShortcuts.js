'use strict';

const Db = require('./db.js');
const util = require('util');
const ArkimeUtil = require('../common/arkimeUtil');
const User = require('../common/user');
const internals = require('./internals');

class Mutex {
  constructor () {
    this.queue = [];
    this.locked = false;
  }

  lock () {
    return new Promise((resolve, reject) => {
      if (this.locked) {
        this.queue.push(resolve);
      } else {
        this.locked = true;
        resolve();
      }
    });
  }

  unlock () {
    if (this.queue.length > 0) {
      const resolve = this.queue.shift();
      resolve();
    } else {
      this.locked = false;
    }
  }
}

class ShortcutAPIs {
  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------
  // https://stackoverflow.com/a/48569020

  static #shortcutMutex = new Mutex();

  // --------------------------------------------------------------------------
  /**
   * Normalizes the data in a shortcut by turning values and users string to arrays
   * and removing the type parameter and replacing it with `type: values`
   * Also validates that the users added to the shortcut are valid within the system
   * NOTE: Mutates the shortcut directly
   * @param {Shortcut} shortcut - The shortcut to normalize
   * @returns {Object} {type, values, invalidusers} - The shortcut type (ip, string, number),
   *                                                  array of values, and list of invalid users
   */
  static async #normalizeShortcut (shortcut) {
    // comma/newline separated value -> array of values
    const values = ArkimeUtil.commaOrNewlineStringToArray(shortcut.value);
    shortcut[shortcut.type] = values;

    const type = shortcut.type;
    delete shortcut.type;
    delete shortcut.value;

    // comma/newline separated value -> array of values
    let users = ArkimeUtil.commaOrNewlineStringToArray(shortcut.users || '');
    users = await User.validateUserIds(users);
    shortcut.users = users.validUsers;

    return { type, values, invalidUsers: users.invalidUsers };
  }

  /**
   * checks a user's permission to access a shortcut to update/delete
   * only allow admins, editors, or creator can update/delete shortcut
   */
  static async checkShortcutAccess (req, res, next) {
    if (req.user.hasRole('arkimeAdmin')) { // an admin can do anything
      return next();
    } else {
      try {
        const { body: { _source: shortcut } } = await Db.getShortcut(req.params.id);

        if (shortcut.userId === req.settingUser.userId || req.settingUser.hasRole(shortcut.editRoles)) {
          return next();
        }

        return res.serverError(403, 'Permission denied');
      } catch (err) {
        return res.serverError(403, 'Unknown shortcut');
      }
    }
  }

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  /**
   * The shortcut object to store lists of values that can be used in search queries.
   *
   * @typedef Shortcut
   * @type {object}
   * @param {string} userId - The ID of the user that created the shortcut.
   * @param {string} name - The name of the shortcut.
   * @param {string} description - The description of the shortcut to display to users.
   * @param {number[]} number - A list of number values to use as the shortcut value. A shortcut must contain a list of numbers, strings, or ips.
   * @param {string[]} ip - A list of ip values to use as the shortcut value. A shortcut must contain a list of numbers, strings, or ips.
   * @param {string[]} string - A list of string values to use as the shortcut value. A shortcut must contain a list of numbers, strings, or ips.
   * @param {string} users - A list of userIds that have access to this shortcut.
   * @param {string[]} roles - A list of Arkime roles that have access to this shortcut.
   * @param {string[]} editRoles - A list of Arkime roles that have edit access to this shortcut.
   * @param {boolean} locked=false - Whether the shortcut is locked and must be updated using the db.pl script (can't be updated in the web application user interface).
   */

  // --------------------------------------------------------------------------
  /**
   * GET - /api/shortcuts
   *
   * Retrieves a list of shortcuts.
   * @name /shortcuts
   * @param {string} map=false - Whether to return a list or a map. Default is list.
   * @param {string} sort=name - The field to sort the results by.
   * @param {string} desc=true - Whether to sort the results descending or ascending. Default is descending.
   * @param {string} searchTerm - The search text to filter the shortcut list by.
   * @param {number} length=50 - The number of items to return. Defaults to 50.
   * @param {number} start=0 - The entry to start at. Defaults to 0.
   * @param {string} fieldType - Filter the results by type (number, ip, or string).
   * @param {string} fieldFormat=false - Sends a help field with the shortcut with the description + the values of the shortcut.
   * @returns {Shortcut[]} data - The list of shortcut results.
   * @returns {number} recordsTotal - The total number of shortcut results stored.
   * @returns {number} recordsFiltered - The number of shortcut items returned in this result.
   */
  static async getShortcuts (req, res) {
    // return nothing if we can't find the user
    const user = req.settingUser;
    if (!user) { return res.send({}); }

    const allRoles = await user.getRoles();
    const roles = [...allRoles.keys()]; // es requires an array for terms search

    const map = req.query.map && req.query.map === 'true';

    // only get shortcuts for setting user or shared
    const query = {
      query: {
        bool: {
          filter: [
            {
              bool: {
                should: [
                  { terms: { roles } }, // shared via user role
                  { terms: { editRoles: roles } }, // shared via edit role
                  { term: { users: req.settingUser.userId } }, // shared via userId
                  { term: { userId: req.settingUser.userId } } // created by this user
                ]
              }
            }
          ]
        }
      },
      sort: {},
      size: req.query.length || 50,
      from: req.query.start || 0
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

    // if fieldType exists, filter it
    if (req.query.fieldType) {
      const fieldType = internals.shortcutTypeMap[req.query.fieldType];

      if (fieldType) {
        query.query.bool.filter.push({
          exists: { field: fieldType }
        });
      }
    }

    const numQuery = { ...query };
    delete numQuery.sort;
    delete numQuery.size;
    delete numQuery.from;

    Promise.all([
      Db.searchShortcuts(query),
      Db.numberOfShortcuts(numQuery)
    ]).then(([{ body: { hits: shortcuts } }, { body: { count: total } }]) => {
      const results = { list: [], map: {} };
      for (const hit of shortcuts.hits) {
        const shortcut = hit._source;
        shortcut.id = hit._id;

        if (shortcut.number) {
          shortcut.type = 'number';
        } else if (shortcut.ip) {
          shortcut.type = 'ip';
        } else {
          shortcut.type = 'string';
        }

        const values = shortcut[shortcut.type];

        if (req.query.fieldFormat && req.query.fieldFormat === 'true') {
          const shortcutName = `$${shortcut.name}`;
          shortcut.exp = shortcutName;
          shortcut.dbField = shortcutName;
          shortcut.help = shortcut.description
            ? `${shortcut.description}: ${values.join(', ')}`
            : `${values.join(',')}`;
        }

        shortcut.value = values.join('\n');
        delete shortcut[shortcut.type];

        if ( // remove sensitive information for users this is shared with
        // (except creator, arkimeAdmin, and editors)
          user.userId !== shortcut.userId &&
          !user.hasRole('arkimeAdmin') &&
          !user.hasRole(shortcut.editRoles)) {
          delete shortcut.users;
          delete shortcut.roles;
          delete shortcut.editRoles;
        } else if (shortcut.users) {
          // client expects a string
          shortcut.users = shortcut.users.join(',');
        }

        if (map) {
          results.map[shortcut.id] = shortcut;
        } else {
          results.list.push(shortcut);
        }
      }

      const sendResults = map
        ? results.map
        : {
          data: results.list,
          recordsTotal: total,
          recordsFiltered: shortcuts.total
        };

      res.send(sendResults);
    }).catch((err) => {
      console.log(`ERROR - ${req.method} /api/shortcuts`, util.inspect(err, false, 50));
      return res.serverError(500, 'Error retrieving shortcuts - ' + err);
    });
  };

  // --------------------------------------------------------------------------
  /**
   * POST - /api/shortcut
   *
   * Creates a new shortcut.
   * @name /shortcut
   * @param {string} name - The name of the new shortcut.
   * @param {string} type - The type of the shortcut (number, ip, or string).
   * @param {string} value - The shortcut value.
   * @param {string} description - The optional description of this shortcut.
   * @param {string} users - A comma separated list of users that can view this shortcut.
   * @param {Array} roles - The roles that can view this shortcut.
   * @returns {Shortcut} shortcut - The new shortcut object.
   * @returns {boolean} success - Whether the create shortcut operation was successful.
   */
  static createShortcut (req, res) {
    // make sure all the necessary data is included in the post body
    if (!ArkimeUtil.isString(req.body.name)) {
      return res.serverError(403, 'Missing shortcut name');
    }
    if (!ArkimeUtil.isString(req.body.type)) {
      return res.serverError(403, 'Missing shortcut type');
    }
    if (!ArkimeUtil.isString(req.body.value)) {
      return res.serverError(403, 'Missing shortcut value');
    }

    if (req.body.roles !== undefined && !ArkimeUtil.isStringArray(req.body.roles)) {
      return res.serverError(403, 'Roles field must be an array of strings');
    }

    if (req.body.editRoles !== undefined && !ArkimeUtil.isStringArray(req.body.editRoles)) {
      return res.serverError(403, 'Edit roles field must be an array of strings');
    }

    if (req.body.users !== undefined && !ArkimeUtil.isString(req.body.users, 0)) {
      return res.serverError(403, 'Users field must be a string');
    }

    req.body.name = req.body.name.replace(/[^-a-zA-Z0-9_]/g, '');

    // return nothing if we can't find the user
    const user = req.settingUser;
    if (!user) { return res.send({}); }

    const query = {
      query: {
        bool: {
          filter: [
            { term: { name: req.body.name } }
          ]
        }
      }
    };

    ShortcutAPIs.#shortcutMutex.lock().then(async () => {
      try {
        const { body: { hits: shortcuts } } = await Db.searchShortcuts(query);

        // search for shortcut name collision
        for (const hit of shortcuts.hits) {
          const shortcut = hit._source;
          if (shortcut.name === req.body.name) {
            ShortcutAPIs.#shortcutMutex.unlock();
            return res.serverError(403, `A shortcut with the name, ${req.body.name}, already exists`);
          }
        }

        const newShortcut = req.body;
        newShortcut.userId = user.userId;

        const { type, values, invalidUsers } = await ShortcutAPIs.#normalizeShortcut(newShortcut);

        try {
          const { body: result } = await Db.createShortcut(newShortcut);
          newShortcut.id = result._id;
          newShortcut.type = type;
          newShortcut.value = values.join('\n');
          delete newShortcut.ip;
          delete newShortcut.string;
          delete newShortcut.number;
          ShortcutAPIs.#shortcutMutex.unlock();

          return res.send(JSON.stringify({
            invalidUsers,
            success: true,
            shortcut: newShortcut,
            text: 'Created new shortcut!'
          }));
        } catch (err) {
          ShortcutAPIs.#shortcutMutex.unlock();
          console.log(`ERROR - ${req.method} /api/shortcut (createShortcut)`, util.inspect(err, false, 50));
          return res.serverError(500, 'Error creating shortcut');
        }
      } catch (err) {
        ShortcutAPIs.#shortcutMutex.unlock();
        console.log(`ERROR - ${req.method} /api/shortcut (searchShortcuts)`, util.inspect(err, false, 50));
        return res.serverError(500, 'Error creating shortcut');
      }
    });
  };

  // --------------------------------------------------------------------------
  /**
   * PUT - /api/shortcut/:id
   *
   * Updates a shortcut.
   * @name /shortcut/:id
   * @param {string} name - The name of the shortcut.
   * @param {string} type - The type of the shortcut (number, ip, or string).
   * @param {string} value - The shortcut value.
   * @param {string} description - The optional description of this shortcut.
   * @param {string} users - A comma separated list of users that can view this shortcut.
   * @param {Array} roles - The roles that can view this shortcut.
   * @param {Array} editRoles - The roles that can edit this shortcut.
   * @returns {Shortcut} shortcut - The updated shortcut object.
   * @returns {boolean} success - Whether the update operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async updateShortcut (req, res) {
    // make sure all the necessary data is included in the post body
    if (!ArkimeUtil.isString(req.body.name)) {
      return res.serverError(403, 'Missing shortcut name');
    }
    if (!ArkimeUtil.isString(req.body.type)) {
      return res.serverError(403, 'Missing shortcut type');
    }
    if (!ArkimeUtil.isString(req.body.value)) {
      return res.serverError(403, 'Missing shortcut value');
    }

    if (req.body.roles !== undefined && !ArkimeUtil.isStringArray(req.body.roles)) {
      return res.serverError(403, 'Roles field must be an array of strings');
    }

    if (req.body.editRoles !== undefined && !ArkimeUtil.isStringArray(req.body.editRoles)) {
      return res.serverError(403, 'Edit roles field must be an array of strings');
    }

    if (req.body.users !== undefined && !ArkimeUtil.isString(req.body.users, 0)) {
      return res.serverError(403, 'Users field must be a string');
    }

    const sentShortcut = req.body;

    try {
      const { body: fetchedShortcut } = await Db.getShortcut(req.params.id);

      if (fetchedShortcut._source.locked) {
        return res.serverError(403, 'Locked Shortcut. Use db.pl script to update this shortcut.');
      }

      const query = {
        query: {
          bool: {
            filter: [
              { term: { name: req.body.name } } // same name
            ],
            must_not: [
              { ids: { values: [req.params.id] } } // but different ID
            ]
          }
        }
      };

      ShortcutAPIs.#shortcutMutex.lock().then(async () => {
        try {
          const { body: { hits: shortcuts } } = await Db.searchShortcuts(query);

          // search for shortcut name collision
          for (const hit of shortcuts.hits) {
            const shortcut = hit._source;
            if (shortcut.name === req.body.name) {
              ShortcutAPIs.#shortcutMutex.unlock();
              return res.serverError(403, `A shortcut with the name, ${req.body.name}, already exists`);
            }
          }

          const { values, invalidUsers } = await ShortcutAPIs.#normalizeShortcut(sentShortcut);

          // transfer ownership (admin and creator only)
          if (sentShortcut.userId?.length) {
            if (req.settingUser.userId !== fetchedShortcut._source.userId && !req.settingUser.hasRole('arkimeAdmin')) {
              ShortcutAPIs.#shortcutMutex.unlock();
              return res.serverError(403, 'Permission denied');
            }

            // comma/newline separated value -> array of values
            let user = ArkimeUtil.commaOrNewlineStringToArray(sentShortcut.userId);
            user = await User.validateUserIds(user);

            if (user.invalidUsers?.length) {
              ShortcutAPIs.#shortcutMutex.unlock();
              return res.serverError(404, `Invalid user: ${user.invalidUsers[0]}`);
            }
            if (user.validUsers?.length) {
              sentShortcut.userId = user.validUsers[0];
            } else {
              ShortcutAPIs.#shortcutMutex.unlock();
              return res.serverError(404, 'Cannot find valid user');
            }
          } else { // keep the same owner
            sentShortcut.userId = fetchedShortcut._source.userId;
          }

          try {
            await Db.setShortcut(req.params.id, sentShortcut);
            ShortcutAPIs.#shortcutMutex.unlock();
            sentShortcut.value = values.join('\n');
            sentShortcut.users = sentShortcut.users.join(',');

            return res.send(JSON.stringify({
              invalidUsers,
              success: true,
              shortcut: sentShortcut,
              text: 'Updated shortcut!'
            }));
          } catch (err) {
            ShortcutAPIs.#shortcutMutex.unlock();
            console.log(`ERROR - ${req.method} /api/shortcut/%s (setShortcut)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
            return res.serverError(500, 'Error updating shortcut');
          }
        } catch (err) {
          ShortcutAPIs.#shortcutMutex.unlock();
          console.log(`ERROR - ${req.method} /api/shortcut/%s (searchShortcuts)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
          return res.serverError(500, 'Error updating shortcut');
        }
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/shortcut/%s (getShortcut)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Fetching shortcut to update failed');
    }
  };

  // --------------------------------------------------------------------------
  /**
   * DELETE - /api/shortcut/:id
   *
   * Deletes a shortcut.
   * @name /shortcut/:id
   * @returns {boolean} success - Whether the delete shortcut operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async deleteShortcut (req, res) {
    try {
      await Db.deleteShortcut(req.params.id);
      res.send(JSON.stringify({
        success: true,
        text: 'Deleted shortcut successfully'
      }));
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/shortcut/%s (deleteShortcut)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Error deleting shortcut');
    }
  };

  // --------------------------------------------------------------------------
  /**
   * GET - /api/syncshortcuts
   *
   * @name /syncshortcuts
   * Updates the shortcuts in the local db if they are out of sync with the
   * remote db (remote db = user's es)
   * This happens periodically (every minute) but can be triggered with this endpoint
   * @ignore
   * @returns {boolean} success - Always true.
   */
  static syncShortcuts (req, res) {
    Db.updateLocalShortcuts();
    return res.send(JSON.stringify({ success: true }));
  };
};

module.exports = ShortcutAPIs;
