'use strict';

const glob = require('glob');
const path = require('path');
const util = require('util');
const User = require('./user');
const ArkimeUtil = require('./arkimeUtil');

class Notifier {
  static notifierTypes;

  static #debug;
  static #esclient;
  static #notifiersIndex;

  static initialize (options) {
    Notifier.#debug = options.debug ?? 0;
    Notifier.#esclient = options.esclient;

    let prefix = '';
    if (options.prefix === undefined) {
      prefix = 'arkime_';
    } else if (options.prefix === '') {
      prefix = '';
    } else if (options.prefix.endsWith('_')) {
      prefix = options.prefix;
    } else {
      prefix = options.prefix + '_';
    }

    Notifier.#notifiersIndex = `${prefix}notifiers`;

    Notifier.notifierTypes = {};

    const api = {
      register: (str, info) => {
        Notifier.notifierTypes[str] = info;
      }
    };

    // look for all notifier providers and initialize them
    const files = glob.sync(path.join(__dirname, '/../common/notifier.*.js'));
    files.forEach((file) => {
      const plugin = require(file);
      plugin.init(api);
    });
  }

  // --------------------------------------------------------------------------
  // DB INTERACTIONS
  // --------------------------------------------------------------------------
  static async getNotifier (id) {
    return Notifier.#esclient.get({ index: Notifier.#notifiersIndex, id });
  }

  static async searchNotifiers (query) {
    return Notifier.#esclient.search({
      index: Notifier.#notifiersIndex, body: query, rest_total_hits_as_int: true, version: true
    });
  }

  static async createNotifier (doc) {
    return await Notifier.#esclient.index({
      index: Notifier.#notifiersIndex, body: doc, refresh: 'wait_for', timeout: '10m'
    });
  }

  static async deleteNotifier (id) {
    return await Notifier.#esclient.delete({
      index: Notifier.#notifiersIndex, id, refresh: true
    });
  }

  static async setNotifier (id, doc) {
    return await Notifier.#esclient.index({
      index: Notifier.#notifiersIndex, body: doc, id, refresh: true, timeout: '10m'
    });
  }

  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------

  /**
   * Checks that the notifier type is valid and the required fields are filled out
   * @param {string} type - The type of notifier that is being checked
   * @param {Array} fields - The list of fields to be checked against the type of notifier
   *                         to determine that all the required fields are filled out
   * @returns {string|undefined} - String message to describe check error or undefined if all is good
   */
  static #checkNotifierTypesAndFields (type, fields) {
    let foundNotifier;
    for (const n in Notifier.notifierTypes) {
      const notifier = Notifier.notifierTypes[n];
      if (notifier.type === type) {
        foundNotifier = notifier;
      }
    }

    if (!foundNotifier) {
      return 'Unknown notifier type';
    }

    // check that required notifier fields exist
    for (const field of foundNotifier.fields) {
      if (field.required) {
        for (const sentField of fields) {
          if (sentField.name === field.name && !sentField.value) {
            return `Missing a value for ${field.name}`;
          }
        }
      }
    }

    return;
  }

  static async issueAlert (id, alertMessage, continueProcess) {
    if (!id) {
      return continueProcess('No id supplied for notifier');
    }

    try {
      const { body: { _source: notifier } } = await Notifier.getNotifier(id);

      if (!notifier) {
        if (Notifier.#debug) {
          console.log('Cannot find notifier, no alert can be issued');
        }
        return continueProcess('Cannot find notifier, no alert can be issued');
      }

      let notifierDefinition;
      for (const n in Notifier.notifierTypes) {
        if (Notifier.notifierTypes[n].type === notifier.type) {
          notifierDefinition = Notifier.notifierTypes[n];
        }
      }
      if (!notifierDefinition) {
        if (Notifier.#debug) {
          console.log('Cannot find notifier definition, no alert can be issued');
        }
        return continueProcess('Cannot find notifier, no alert can be issued');
      }

      const config = {};
      for (const field of notifierDefinition.fields) {
        for (const configuredField of notifier.fields) {
          if (configuredField.name === field.name && configuredField.value !== undefined) {
            config[field.name] = configuredField.value;
          }
        }

        // If a field is required and nothing was set, then we have an error
        if (field.required && config[field.name] === undefined) {
          if (Notifier.#debug) {
            console.log(`Cannot find notifier field value: ${field.name}, no alert can be issued`);
          }
          continueProcess(`Cannot find notifier field value: ${field.name}, no alert can be issued`);
        }
      }

      notifierDefinition.sendAlert(config, alertMessage, null, (response) => {
        let err;
        // there should only be one error here because only one
        // notifier alert is sent at a time
        if (response.errors) {
          for (const e in response.errors) {
            err = response.errors[e];
          }
        }
        return continueProcess(err, notifier.name);
      });
    } catch (err) {
      if (Notifier.#debug) {
        console.log('Cannot find notifier, no alert can be issued');
      }
      return continueProcess('Cannot find notifier, no alert can be issued');
    }
  }

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  /**
   * A service that can be sent a notification.
   * @typedef Notifier
   * @type {object}
   * @property {string} name - The human readable name of the notifier. Must be unique.
   * @property {string} type - The type of notifier (e.g. email, slack, twilio).
   * @property {array} fields - The list of fields that need to be configured to use the notifier.
   * @property {number} created - The time the notifier was created. Format is seconds since Unix EPOC.
   * @property {number} updated - The time the notifier was last updated. Format is seconds since Unix EPOC.
   * @property {string} user - The ID of the user that created the notifier.
   * @property {Arrray} users - The list of userIds who have access to use this notifier.
   * @property {Array} roles - The list of roles who have access to use this notifier.
   */

  /**
   * GET - /api/notifiertypes
   *
   * Retrieves notifier types (admin only).
   * @name /notifiertypes
   * @returns {object} notifiers - The notifiers that Arkime knows about.
   */
  static apiGetNotifierTypes (req, res) {
    return res.send(Notifier.notifierTypes);
  }

  /**
   * GET - /api/notifiers
   *
   * Retrieves notifiers that have been configured.
   * @name /notifiers
   * @returns {Notifier[]} notifiers - The notifiers that have been created.
   */
  static apiGetNotifiers (req, res) {
    const roles = [...req.user._allRoles.keys()]; // es requries an array for terms search

    const query = {
      sort: { created: { order: 'asc' } }
    };

    // if not an admin, restrict who can see the notifiers
    if (!req.user.hasRole('arkimeAdmin')) {
      query.query = {
        bool: {
          filter: [
            {
              bool: {
                should: [
                  { terms: { roles: roles } }, // shared via user role
                  { term: { users: req.user.userId } } // shared via userId
                ]
              }
            }
          ]
        }
      };
    }

    Notifier.searchNotifiers(query).then(({ body: { hits: { hits: notifiers } } }) => {
      const results = notifiers.map((notifier) => {
        const id = notifier._id;
        const result = notifier._source;
        // client expects a string
        const users = result.users;
        result.users = '';
        if (users) {
          result.users = users.join(',') || '';
        }
        if (!req.user.hasRole('arkimeAdmin')) {
          // non-admins only need name and type to use notifiers (they cannot create/update/delete)
          const notifierType = result.type;
          const notifierName = result.name;
          return { name: notifierName, type: notifierType };
        }
        result.id = id;
        return result;
      });

      return res.send(results);
    });
  }

  /**
   * POST - /api/notifier
   *
   * Creates a new notifier (admin only).
   * @name /notifier
   * @param {string} name - The name of the new notifier.
   * @param {type} type - The type of notifier.
   * @param {array} fields - The fields to configure the notifier.
   * @returns {boolean} success - Whether the create notifier operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {Notifier} notifier - If successful, the notifier with name sanitized and created/user fields added.
   */
  static async apiCreateNotifier (req, res) {
    if (!req.body.name) {
      return res.serverError(403, 'Missing a notifier name');
    }

    if (!req.body.type) {
      return res.serverError(403, 'Missing notifier type');
    }

    if (!req.body.fields) {
      return res.serverError(403, 'Missing notifier fields');
    }

    if (!Array.isArray(req.body.fields)) {
      return res.serverError(403, 'Notifier fields must be an array');
    }

    req.body.name = req.body.name.replace(/[^-a-zA-Z0-9_: ]/g, '');

    const errorMsg = Notifier.#checkNotifierTypesAndFields(req.body.type, req.body.fields);
    if (errorMsg) {
      return res.serverError(403, errorMsg);
    }

    // add user and created date
    req.body.user = req.user.userId;
    req.body.created = Math.floor(Date.now() / 1000);

    // comma/newline separated value -> array of values
    let users = ArkimeUtil.commaOrNewlineStringToArray(req.body.users || '');
    users = await User.validateUserIds(users);
    req.body.users = users.validUsers;

    try {
      const { body: { _id: id } } = await Notifier.createNotifier(req.body);

      req.body.id = id;
      req.body.users = req.body.users.join(',');
      return res.send(JSON.stringify({
        success: true,
        notifier: req.body,
        text: 'Created notifier!',
        invalidUsers: users.invalidUsers
      }));
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/notifier (createNotifier)`, util.inspect(err, false, 50));
      return res.serverError(500, 'Error creating notifier');
    }
  }

  /**
   * PUT - /api/notifier/:id
   *
   * Updates an existing notifier (admin only).
   * @name /notifier/:id
   * @param {string} id - The new id of the notifier.
   * @param {type} type - The new type of notifier.
   * @param {array} fields - The new field values to configure the notifier.
   * @returns {boolean} success - Whether the update notifier operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {Notifier} notifier - If successful, the updated notifier with name sanitized and updated field added/updated.
   */

  static async apiUpdateNotifier (req, res) {
    if (!req.body.name) {
      return res.serverError(403, 'Missing a notifier name');
    }

    if (!req.body.type) {
      return res.serverError(403, 'Missing notifier type');
    }

    if (!req.body.fields) {
      return res.serverError(403, 'Missing notifier fields');
    }

    if (!Array.isArray(req.body.fields)) {
      return res.serverError(403, 'Notifier fields must be an array');
    }

    req.body.name = req.body.name.replace(/[^-a-zA-Z0-9_: ]/g, '');

    const errorMsg = Notifier.#checkNotifierTypesAndFields(req.body.type, req.body.fields);
    if (errorMsg) {
      return res.serverError(403, errorMsg);
    }

    try {
      const { body: { _source: notifier } } = await Notifier.getNotifier(req.params.id);

      if (!notifier) {
        return res.serverError(404, 'Notifier not found');
      }

      // only name, fields, roles, users can change
      notifier.name = req.body.name;
      notifier.roles = req.body.roles;
      notifier.fields = req.body.fields;
      notifier.updated = Math.floor(Date.now() / 1000); // update/add updated time

      // comma/newline separated value -> array of values
      let users = ArkimeUtil.commaOrNewlineStringToArray(req.body.users || '');
      users = await User.validateUserIds(users);
      notifier.users = users.validUsers;

      try {
        await Notifier.setNotifier(req.params.id, notifier);
        notifier.id = req.params.id;
        notifier.users = notifier.users.join(',');
        res.send(JSON.stringify({
          notifier,
          success: true,
          invalidUsers: users.invalidUsers,
          text: 'Updated notifier successfully'
        }));
      } catch (err) {
        console.log(`ERROR - ${req.method} /api/notifier/${req.params.id} (setNotifier)`, util.inspect(err, false, 50));
        return res.serverError(500, 'Error updating notifier');
      }
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/notifier/${req.params.id} (getNotifier)`, util.inspect(err, false, 50));
      return res.serverError(500, 'Fetching notifier to update failed');
    }
  }

  /**
   * DELETE - /api/notifier/:id
   *
   * Deletes an existing notifier (admin only).
   * @name /notifier/:id
   * @returns {boolean} success - Whether the delete notifier operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async apiDeleteNotifier (req, res) {
    try {
      const { body: notifier } = await Notifier.getNotifier(req.params.id);

      if (!notifier) {
        return res.serverError(404, 'Notifier not found');
      }

      try {
        await Notifier.deleteNotifier(req.params.id);
        res.send(JSON.stringify({
          success: true,
          text: 'Deleted notifier successfully'
        }));
      } catch (err) {
        console.log(`ERROR - ${req.method} /api/notifier/${req.params.id} (deleteNotifier)`, util.inspect(err, false, 50));
        return res.serverError(500, 'Error deleting notifier');
      }
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/notifier/${req.params.id} (deleteNotifier)`, util.inspect(err, false, 50));
      return res.serverError(500, 'Fetching notifier to delete failed');
    }
  }

  /**
   * POST - /api/notifier/:id/test
   *
   * Tests an existing notifier (admin only).
   * @name /notifier/:id/test
   * @returns {boolean} success - Whether the test notifier operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static apiTestNotifier (req, res) {
    function continueProcess (err, notifierName) {
      if (err) {
        return res.serverError(500, `Error testing alert: ${err}`);
      }

      return res.send(JSON.stringify({
        success: true,
        text: `Successfully issued alert using the ${notifierName} notifier.`
      }));
    }

    Notifier.issueAlert(req.params.id, `Test alert from ${req.user.userId}`, continueProcess);
  }
}

module.exports = Notifier;
