/******************************************************************************/
/* apiShortcuts.js -- api calls for shortcuts
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
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
   * @private
   *
   * Normalizes the data in a shortcut by turning values and users string to arrays
   * and removing the type parameter and replacing it with `type: values`
   * Also validates that the users added to the shortcut are valid within the system
   * NOTE: Mutates the shortcut directly
   * @param {Shortcut} shortcut - The shortcut to normalize
   * @returns {Object} {type, values, invalidusers} - The shortcut type (ip, string, number),
   *                                                  array of values, and list of invalid users
   */
  static async #normalizeShortcut (shortcut) {
    const validTypes = { ip: 1, string: 1, number: 1 };
    if (!validTypes[shortcut.type]) {
      return { type: shortcut.type, values: [], invalidUsers: [], error: 'Invalid shortcut type' };
    }

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
    const roles = [...allRoles.keys()];

    const map = req.query.map && req.query.map === 'true';

    const allowedShortcutsSortFields = { name: 1, description: 1, created: 1 };
    const params = {
      user: req.settingUser.userId,
      roles,
      all: req.query.all && roles.includes('arkimeAdmin'),
      sortField: allowedShortcutsSortFields[req.query.sort] ? req.query.sort : 'name',
      sortOrder: req.query.desc === 'true' ? 'desc' : 'asc',
      size: req.query.length || 50,
      from: req.query.start || 0,
      searchTerm: req.query.searchTerm
    };

    // if fieldType exists, filter it
    if (req.query.fieldType) {
      params.fieldType = internals.shortcutTypeMap[req.query.fieldType];
    }

    try {
      const [{ data, total: recordsFiltered }, recordsTotal] = await Promise.all([
        Db.searchShortcuts(params),
        Db.numberOfShortcuts({ ...params, searchTerm: undefined })
      ]);

      const results = { list: [], map: {} };
      for (const hit of data) {
        const shortcut = hit.source;
        shortcut.id = hit.id;

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
          recordsTotal,
          recordsFiltered
        };

      res.send(sendResults);
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/shortcuts`, util.inspect(err, false, 50));
      return res.serverError(500, 'Error retrieving shortcuts', 'api.shortcuts.retrieveFailed');
    }
  }

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
      return res.serverError(403, 'Missing shortcut name', 'api.shortcuts.missingName');
    }
    if (!ArkimeUtil.isString(req.body.type)) {
      return res.serverError(403, 'Missing shortcut type', 'api.shortcuts.missingType');
    }
    if (!ArkimeUtil.isString(req.body.value)) {
      return res.serverError(403, 'Missing shortcut value', 'api.shortcuts.missingValue');
    }

    if (req.body.roles !== undefined && !ArkimeUtil.isStringArray(req.body.roles)) {
      return res.serverError(403, 'Roles field must be an array of strings', 'api.shortcuts.rolesMustBeArray');
    }

    if (req.body.editRoles !== undefined && !ArkimeUtil.isStringArray(req.body.editRoles)) {
      return res.serverError(403, 'Edit roles field must be an array of strings', 'api.shortcuts.editRolesMustBeArray');
    }

    if (req.body.users !== undefined && !ArkimeUtil.isString(req.body.users, 0)) {
      return res.serverError(403, 'Users field must be a string', 'api.shortcuts.usersMustBeString');
    }

    req.body.name = req.body.name.replace(/[^-a-zA-Z0-9_]/g, '');

    // return nothing if we can't find the user
    const user = req.settingUser;
    if (!user) { return res.send({}); }

    const nameCheckParams = { nameCheck: req.body.name, all: true };

    ShortcutAPIs.#shortcutMutex.lock().then(async () => {
      try {
        const { data: shortcuts } = await Db.searchShortcuts(nameCheckParams);

        // search for shortcut name collision
        for (const hit of shortcuts) {
          if (hit.source.name === req.body.name) {
            ShortcutAPIs.#shortcutMutex.unlock();
            return res.serverError(403, `A shortcut with the name, ${req.body.name}, already exists`, 'api.shortcuts.nameExists', { name: req.body.name });
          }
        }

        const newShortcut = req.body;
        newShortcut.userId = user.userId;

        const { type, values, invalidUsers, error } = await ShortcutAPIs.#normalizeShortcut(newShortcut);

        if (error) {
          ShortcutAPIs.#shortcutMutex.unlock();
          return res.serverError(403, error);
        }

        try {
          const id = await Db.createShortcut(newShortcut);
          newShortcut.id = id;
          newShortcut.type = type;
          newShortcut.value = values.join('\n');
          delete newShortcut.ip;
          delete newShortcut.string;
          delete newShortcut.number;
          ShortcutAPIs.#shortcutMutex.unlock();

          return res.json({
            invalidUsers,
            success: true,
            shortcut: newShortcut,
            text: 'Created new shortcut!',
            i18n: 'api.shortcuts.created'
          });
        } catch (err) {
          ShortcutAPIs.#shortcutMutex.unlock();
          console.log(`ERROR - ${req.method} /api/shortcut (createShortcut)`, util.inspect(err, false, 50));
          return res.serverError(500, 'Error creating shortcut', 'api.shortcuts.errorCreating');
        }
      } catch (err) {
        ShortcutAPIs.#shortcutMutex.unlock();
        console.log(`ERROR - ${req.method} /api/shortcut (searchShortcuts)`, util.inspect(err, false, 50));
        return res.serverError(500, 'Error creating shortcut', 'api.shortcuts.errorCreating');
      }
    });
  }

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
      return res.serverError(403, 'Missing shortcut name', 'api.shortcuts.missingName');
    }
    if (!ArkimeUtil.isString(req.body.type)) {
      return res.serverError(403, 'Missing shortcut type', 'api.shortcuts.missingType');
    }
    if (!ArkimeUtil.isString(req.body.value)) {
      return res.serverError(403, 'Missing shortcut value', 'api.shortcuts.missingValue');
    }

    if (req.body.roles !== undefined && !ArkimeUtil.isStringArray(req.body.roles)) {
      return res.serverError(403, 'Roles field must be an array of strings', 'api.shortcuts.rolesMustBeArray');
    }

    if (req.body.editRoles !== undefined && !ArkimeUtil.isStringArray(req.body.editRoles)) {
      return res.serverError(403, 'Edit roles field must be an array of strings', 'api.shortcuts.editRolesMustBeArray');
    }

    if (req.body.users !== undefined && !ArkimeUtil.isString(req.body.users, 0)) {
      return res.serverError(403, 'Users field must be a string', 'api.shortcuts.usersMustBeString');
    }

    const sentShortcut = req.body;
    sentShortcut.name = sentShortcut.name.replace(/[^-a-zA-Z0-9_]/g, '');

    try {
      const fetchedSource = await Db.getShortcut(req.params.id);

      if (fetchedSource.locked) {
        return res.serverError(403, 'Locked Shortcut. Use db.pl script to update this shortcut.', 'api.shortcuts.locked');
      }

      const nameCheckParams = { nameCheck: req.body.name, excludeId: req.params.id, all: true };

      ShortcutAPIs.#shortcutMutex.lock().then(async () => {
        try {
          const { data: shortcuts } = await Db.searchShortcuts(nameCheckParams);

          // search for shortcut name collision
          for (const hit of shortcuts) {
            if (hit.source.name === req.body.name) {
              ShortcutAPIs.#shortcutMutex.unlock();
              return res.serverError(403, `A shortcut with the name, ${req.body.name}, already exists`, 'api.shortcuts.nameExists', { name: req.body.name });
            }
          }

          const { values, invalidUsers, error } = await ShortcutAPIs.#normalizeShortcut(sentShortcut);

          if (error) {
            ShortcutAPIs.#shortcutMutex.unlock();
            return res.serverError(403, error);
          }

          // sets the owner if it has changed
          if (!await User.setOwner(req, res, sentShortcut, fetchedSource, 'userId')) {
            ShortcutAPIs.#shortcutMutex.unlock();
            return;
          }

          try {
            await Db.setShortcut(req.params.id, sentShortcut);
            ShortcutAPIs.#shortcutMutex.unlock();
            sentShortcut.value = values.join('\n');
            sentShortcut.users = sentShortcut.users.join(',');

            return res.json({
              invalidUsers,
              success: true,
              shortcut: sentShortcut,
              text: 'Updated shortcut!',
              i18n: 'api.shortcuts.updated'
            });
          } catch (err) {
            ShortcutAPIs.#shortcutMutex.unlock();
            console.log(`ERROR - ${req.method} /api/shortcut/%s (setShortcut)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
            return res.serverError(500, 'Error updating shortcut', 'api.shortcuts.errorUpdating');
          }
        } catch (err) {
          ShortcutAPIs.#shortcutMutex.unlock();
          console.log(`ERROR - ${req.method} /api/shortcut/%s (searchShortcuts)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
          return res.serverError(500, 'Error updating shortcut', 'api.shortcuts.errorUpdating');
        }
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/shortcut/%s (getShortcut)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Fetching shortcut to update failed', 'api.shortcuts.errorFetching');
    }
  }

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
      res.json({
        success: true,
        text: 'Deleted shortcut successfully',
        i18n: 'api.shortcuts.deleted'
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/shortcut/%s (deleteShortcut)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Error deleting shortcut', 'api.shortcuts.errorDeleting');
    }
  }

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
    return res.json({ success: true });
  }
}

module.exports = ShortcutAPIs;
