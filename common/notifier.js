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
  static #defaultAlerts = { esRed: false, esDown: false, esDropped: false, outOfDate: false, noPackets: false };

  static initialize (options) {
    Notifier.#debug = options.debug ?? 0;
    Notifier.#esclient = options.esclient;

    const prefix = ArkimeUtil.formatPrefix(options.prefix);

    Notifier.#notifiersIndex = `${prefix}notifiers`;

    Notifier.notifierTypes = {};

    const api = {
      register: (str, info) => {
        if (options.issueTypes) { info.alerts = options.issueTypes; }
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
   * Checks that the notifier is valid.
   * Mutates the notifier if on/alerts properties are missing or the name has special chars.
   * @param {object} notifier - The notifier object to be checked
   * @returns {string|undefined} - String message to describe check error or undefined if all is good
   */
  static #validateNotifier (notifier) {
    if (!ArkimeUtil.isString(notifier.name)) {
      return 'Missing a notifier name';
    }

    if (!ArkimeUtil.isString(notifier.type)) {
      return 'Missing notifier type';
    }

    if (!notifier.fields) {
      return 'Missing notifier fields';
    }

    if (!Array.isArray(notifier.fields)) {
      return 'Notifier fields must be an array';
    }

    notifier.name = notifier.name.replace(/[^-a-zA-Z0-9_: ]/g, '');
    if (notifier.name.length === 0) {
      return 'Notifier name empty';
    }

    notifier.on ??= false;
    if (typeof notifier.on !== 'boolean') {
      return 'Notifier on state must be true or false';
    }

    const type = notifier.type.toLowerCase();

    let foundNotifier;
    for (const n in Notifier.notifierTypes) {
      if (Notifier.notifierTypes[n].type === type) {
        foundNotifier = Notifier.notifierTypes[n];
      }
    }

    if (!foundNotifier) {
      return 'Unknown notifier type';
    }

    // check that required notifier fields exist
    for (const field of foundNotifier.fields) {
      if (field.required) {
        for (const sentField of notifier.fields) {
          if (sentField.name === field.name && !sentField.value) {
            return `Missing a value for ${field.name}`;
          }
        }
      }
    }

    notifier.alerts ??= Notifier.#defaultAlerts;
    if (typeof notifier.alerts !== 'object') {
      return 'Alerts must be an object';
    }
    for (const a in notifier.alerts) {
      if (typeof notifier.alerts[a] !== 'boolean') {
        return 'Alert must be true or false';
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
        const msg = `ERROR - Cannot find notifier (${id}), no alert can be issued`;
        if (Notifier.#debug) { console.log(msg); }
        return continueProcess(msg);
      }

      let notifierDefinition;
      for (const n in Notifier.notifierTypes) {
        if (Notifier.notifierTypes[n].type === notifier.type.toLowerCase()) {
          notifierDefinition = Notifier.notifierTypes[n];
        }
      }
      if (!notifierDefinition) {
        const msg = `Cannot find notifier definition (for ${notifier.type}), no alert can be issued`;
        console.log(msg);
        return continueProcess(msg);
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
          const msg = `Cannot find notifier field value: ${field.name}, no alert can be issued`;
          if (Notifier.#debug) { console.log(msg); }
          continueProcess(msg);
        }
      }

      notifierDefinition.sendAlert(config, alertMessage, null, (response) => {
        let err;
        // there should only be one error here because only one notifier alert is sent at a time
        if (response.errors) {
          for (const e in response.errors) {
            err = response.errors[e];
          }
        }
        return continueProcess(err, notifier.name);
      });
    } catch (err) {
      console.log('ERROR - Cannot find notifier (%s):', ArkimeUtil.sanitizeStr(id), util.inspect(err, false, 50));
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
  static async apiGetNotifiers (req, res) {
    const allRoles = await req.user.getRoles();
    const roles = [...allRoles.keys()]; // es requries an array for terms search

    const query = {
      sort: { created: { order: 'asc' } }
    };

    // if not an admin, restrict who can see the notifiers
    if (!req.user.hasRole('arkimeAdmin') && !req.user.hasRole('parliamentAdmin')) {
      query.query = {
        bool: {
          filter: [
            {
              bool: {
                should: [
                  { terms: { roles } }, // shared via user role
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
        if (!req.user.hasRole('arkimeAdmin') && !req.user.hasRole('parliamentAdmin')) {
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
    const errorMsg = Notifier.#validateNotifier(req.body);
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
    const errorMsg = Notifier.#validateNotifier(req.body);
    if (errorMsg) {
      return res.serverError(403, errorMsg);
    }

    try {
      const { body: { _source: notifier } } = await Notifier.getNotifier(req.params.id);

      if (!notifier) {
        return res.serverError(404, 'Notifier not found');
      }

      // only on, name, fields, roles, users, alerts can change
      notifier.on = !!req.body.on;
      notifier.name = req.body.name;
      notifier.roles = req.body.roles;
      notifier.fields = req.body.fields;
      notifier.alerts = req.body.alerts ??= Notifier.#defaultAlerts;
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
        console.log(`ERROR - ${req.method} /api/notifier/%s (setNotifier)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
        return res.serverError(500, 'Error updating notifier');
      }
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/notifier/%s (getNotifier)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
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
        console.log(`ERROR - ${req.method} /api/notifier/%s (deleteNotifier)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
        return res.serverError(500, 'Error deleting notifier');
      }
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/notifier/%s (deleteNotifier)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
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
