'use strict';

const glob = require('glob');
const path = require('path');
const util = require('util');
const User = require('../common/user');

module.exports = (Config, Db, internals) => {
  const nModule = {};

  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------
  function buildNotifiers () {
    internals.notifiers = {};

    const api = {
      register: (str, info) => {
        internals.notifiers[str] = info;
      }
    };

    // look for all notifier providers and initialize them
    const files = glob.sync(path.join(__dirname, '/../common/notifier.*.js'));
    files.forEach((file) => {
      const plugin = require(file);
      plugin.init(api);
    });
  };

  nModule.issueAlert = (notifierName, alertMessage, continueProcess) => {
    if (!notifierName) {
      return continueProcess('No name supplied for notifier');
    }

    if (!internals.notifiers) { buildNotifiers(); }

    // find notifier
    User.getUser('_moloch_shared', (err, sharedUser) => {
      if (!sharedUser) {
        if (Config.debug) {
          console.log('Cannot find notifier, no alert can be issued');
        }
        return continueProcess('Cannot find notifier, no alert can be issued');
      }

      sharedUser.notifiers = sharedUser.notifiers || {};

      const notifier = sharedUser.notifiers[notifierName];

      if (!notifier) {
        if (Config.debug) {
          console.log('Cannot find notifier, no alert can be issued');
        }
        return continueProcess('Cannot find notifier, no alert can be issued');
      }

      let notifierDefinition;
      for (const n in internals.notifiers) {
        if (internals.notifiers[n].type === notifier.type) {
          notifierDefinition = internals.notifiers[n];
        }
      }
      if (!notifierDefinition) {
        if (Config.debug) {
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
          if (Config.debug) {
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
        return continueProcess(err);
      });
    });
  };

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
   */

  /**
   * GET - /api/notifiertypes
   *
   * Retrieves notifier types (admin only).
   * @name /notifiertypes
   * @returns {object} notifiers - The notifiers that Arkime knows about.
   */
  nModule.getNotifierTypes = (req, res) => {
    if (!internals.notifiers) { buildNotifiers(); }
    return res.send(internals.notifiers);
  };

  /**
   * GET - /api/notifiers
   *
   * Retrieves notifiers that have been configured.
   * @name /notifiers
   * @returns {Notifier[]} notifiers - The notifiers that have been created.
   */
  nModule.getNotifiers = (req, res) => {
    function cloneNotifiers (notifiers) {
      const clone = {};

      for (const key in notifiers) { // strip sensitive notifier fields
        if (notifiers[key]) {
          const notifier = notifiers[key];
          clone[key] = {
            name: notifier.name,
            type: notifier.type
          };
        }
      }

      return clone;
    }

    // send client an array so it can be sorted and is always in the same order
    function arrayifyAndSort (notifiers) {
      const notifierArray = [];
      for (const key in notifiers) {
        const notifier = notifiers[key];
        notifier.key = key;
        notifierArray.push(notifier);
      }
      notifierArray.sort((a, b) => { return a.created < b.created; });
      return notifierArray;
    }

    User.getUser('_moloch_shared', (err, sharedUser) => {
      if (!sharedUser) {
        return res.send([]);
      }

      const notifiers = sharedUser.notifiers || [];

      if (req.user.createEnabled) {
        return res.send(arrayifyAndSort(notifiers));
      }

      return res.send(arrayifyAndSort(cloneNotifiers(notifiers)));
    });
  };

  /**
   * POST - /api/notifier
   *
   * Creates a new notifier (admin only).
   * @name /notifier
   * @param {string} name - The name of the new notifier (must be unique).
   * @param {type} type - The type of notifier.
   * @param {array} fields - The fields to configure the notifier.
   * @returns {boolean} success - Whether the create notifier operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {Notifier} notifier - If successful, the notifier with name sanitized and created/user fields added.
   */
  nModule.createNotifier = (req, res) => {
    if (!req.body.name) {
      return res.serverError(403, 'Missing a unique notifier name');
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

    if (!internals.notifiers) { buildNotifiers(); }

    let foundNotifier;
    for (const n in internals.notifiers) {
      const notifier = internals.notifiers[n];
      if (notifier.type === req.body.type) {
        foundNotifier = notifier;
      }
    }

    if (!foundNotifier) {
      return res.serverError(403, 'Unknown notifier type');
    }

    // check that required notifier fields exist
    for (const field of foundNotifier.fields) {
      if (field.required) {
        for (const sentField of req.body.fields) {
          if (sentField.name === field.name && !sentField.value) {
            return res.serverError(403, `Missing a value for ${field.name}`);
          }
        }
      }
    }

    // add user and created date
    req.body.user = req.user.userId;
    req.body.created = Math.floor(Date.now() / 1000);

    // save the notifier on the shared user
    User.getUser('_moloch_shared', (err, sharedUser) => {
      if (!sharedUser) {
        // sharing for the first time
        sharedUser = {
          userId: '_moloch_shared',
          userName: '_moloch_shared',
          enabled: false,
          webEnabled: false,
          emailSearch: false,
          headerAuthEnabled: false,
          createEnabled: false,
          removeEnabled: false,
          packetSearch: false,
          views: {},
          notifiers: {}
        };
      }

      sharedUser.notifiers = sharedUser.notifiers || {};

      if (sharedUser.notifiers[req.body.name]) {
        if (Config.debug) {
          console.log('Trying to add duplicate notifier', sharedUser);
        }
        return res.serverError(403, 'Notifier already exists');
      }

      sharedUser.notifiers[req.body.name] = req.body;

      User.setUser('_moloch_shared', sharedUser, (err, info) => {
        if (err) {
          console.log(`ERROR - ${req.method} /api/notifier`, util.inspect(err, false, 50), info);
          return res.serverError(500, 'Creating notifier failed');
        }
        return res.send(JSON.stringify({
          success: true,
          text: 'Successfully created notifier',
          notifier: { ...req.body, key: req.body.name }
        }));
      });
    });
  };

  /**
   * PUT - /api/notifier/:name
   *
   * Updates an existing notifier (admin only).
   * @name /notifier/:name
   * @param {string} name - The new name of the notifier (must be unique).
   * @param {type} type - The new type of notifier.
   * @param {array} fields - The new field values to configure the notifier.
   * @returns {boolean} success - Whether the update notifier operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {Notifier} notifier - If successful, the updated notifier with name sanitized and updated field added/updated.
   */
  nModule.updateNotifier = (req, res) => {
    User.getUser('_moloch_shared', (err, sharedUser) => {
      if (!sharedUser) {
        return res.serverError(404, 'Cannot find notifer to udpate');
      }

      sharedUser.notifiers = sharedUser.notifiers || {};

      if (!sharedUser.notifiers[req.params.name]) {
        return res.serverError(404, 'Cannot find notifer to udpate');
      }

      if (!req.body.name) {
        return res.serverError(403, 'Missing a unique notifier name');
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

      if (!internals.notifiers) { buildNotifiers(); }

      let foundNotifier;
      for (const n in internals.notifiers) {
        const notifier = internals.notifiers[n];
        if (notifier.type === req.body.type) {
          foundNotifier = notifier;
        }
      }

      if (!foundNotifier) { return res.serverError(403, 'Unknown notifier type'); }

      // check that required notifier fields exist
      for (const field of foundNotifier.fields) {
        if (field.required) {
          for (const sentField of req.body.fields) {
            if (sentField.name === field.name && !sentField.value) {
              return res.serverError(403, `Missing a value for ${field.name}`);
            }
          }
        }
      }

      sharedUser.notifiers[req.body.name] = {
        name: req.body.name, // update name
        type: sharedUser.notifiers[req.params.name].type, // type can't change
        user: sharedUser.notifiers[req.params.name].user, // user can't change
        created: sharedUser.notifiers[req.params.name].created, // created can't change
        updated: Math.floor(Date.now() / 1000), // update/add updated time
        fields: req.body.fields // update fields
      };

      // delete the old notifier if the name has changed
      if (sharedUser.notifiers[req.params.name] && req.body.name !== req.params.name) {
        sharedUser.notifiers[req.params.name] = null;
        delete sharedUser.notifiers[req.params.name];
      }

      User.setUser('_moloch_shared', sharedUser, (err, info) => {
        if (err) {
          console.log(`ERROR - ${req.method} /api/notifier/${req.params.name}`, util.inspect(err, false, 50), info);
          return res.serverError(500, 'Updating notifier failed');
        }
        return res.send(JSON.stringify({
          success: true,
          text: 'Successfully updated notifier',
          notifier: { ...sharedUser.notifiers[req.body.name], key: req.body.name }
        }));
      });
    });
  };

  /**
   * DELETE - /api/notifier/:name
   *
   * Deletes an existing notifier (admin only).
   * @name /notifier/:name
   * @returns {boolean} success - Whether the delete notifier operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {string} name - If successful, the name of the deleted notifier.
   */
  nModule.deleteNotifier = (req, res) => {
    User.getUser('_moloch_shared', (err, sharedUser) => {
      if (!sharedUser) {
        return res.serverError(404, 'Cannot find notifer to remove');
      }

      sharedUser.notifiers = sharedUser.notifiers || {};

      if (!sharedUser.notifiers[req.params.name]) {
        return res.serverError(404, 'Cannot find notifer to remove');
      }

      sharedUser.notifiers[req.params.name] = undefined;

      User.setUser('_moloch_shared', sharedUser, (err, info) => {
        if (err) {
          console.log(`ERROR - ${req.method} /api/notifier/${req.params.name}`, util.inspect(err, false, 50), info);
          return res.serverError(500, 'Deleting notifier failed');
        }
        return res.send(JSON.stringify({
          success: true,
          text: 'Successfully deleted notifier',
          name: req.params.name
        }));
      });
    });
  };

  /**
   * POST - /api/notifier/:name/test
   *
   * Tests an existing notifier (admin only).
   * @name /notifier/:name/test
   * @returns {boolean} success - Whether the test notifier operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  nModule.testNotifier = (req, res) => {
    function continueProcess (err) {
      if (err) {
        return res.serverError(500, `Error testing alert: ${err}`);
      }

      return res.send(JSON.stringify({
        success: true,
        text: `Successfully issued alert using the ${req.params.name} notifier.`
      }));
    }

    nModule.issueAlert(req.params.name, `Test alert from ${req.user.userId}`, continueProcess);
  };

  return nModule;
};
