'use strict';

const glob = require('glob');
const path = require('path');

module.exports = (Config, Db, internals) => {
  const module = {};

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
    const files = glob.sync(path.join(__dirname, '/../notifiers/provider.*.js'));
    files.forEach((file) => {
      const plugin = require(file);
      plugin.init(api);
    });
  };

  module.issueAlert = (notifierName, alertMessage, continueProcess) => {
    if (!notifierName) {
      return continueProcess('No name supplied for notifier');
    }

    if (!internals.notifiers) { buildNotifiers(); }

    // find notifier
    Db.getUser('_moloch_shared', (err, sharedUser) => {
      if (!sharedUser || !sharedUser.found) {
        if (Config.debug) {
          console.log('Cannot find notifier, no alert can be issued');
        }
        return continueProcess('Cannot find notifier, no alert can be issued');
      }

      sharedUser = sharedUser._source;

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
   */

  /**
   * GET - /api/notifiertypes
   *
   * Retrieves notifier types (admin only).
   * @name /notifiertypes
   * @returns {object} notifiers - The notifiers that Arkime knows about.
   */
  module.getNotifierTypes = (req, res) => {
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
  module.getNotifiers = (req, res) => {
    function cloneNotifiers (notifiers) {
      const clone = {};

      for (const key in notifiers) {
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

    Db.getUser('_moloch_shared', (err, sharedUser) => {
      if (!sharedUser || !sharedUser.found) {
        return res.send({});
      } else {
        sharedUser = sharedUser._source;
      }

      if (req.user.createEnabled) {
        return res.send(sharedUser.notifiers);
      }

      return res.send(cloneNotifiers(sharedUser.notifiers));
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
   * @returns {string} name - If successful, the name of the new notifier.
   */
  module.createNotifier = (req, res) => {
    if (!req.body.name) {
      return res.molochError(403, 'Missing a unique notifier name');
    }

    if (!req.body.type) {
      return res.molochError(403, 'Missing notifier type');
    }

    if (!req.body.fields) {
      return res.molochError(403, 'Missing notifier fields');
    }

    if (!Array.isArray(req.body.fields)) {
      return res.molochError(403, 'Notifier fields must be an array');
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
      return res.molochError(403, 'Unknown notifier type');
    }

    // check that required notifier fields exist
    for (const field of foundNotifier.fields) {
      if (field.required) {
        for (const sentField of req.body.fields) {
          if (sentField.name === field.name && !sentField.value) {
            return res.molochError(403, `Missing a value for ${field.name}`);
          }
        }
      }
    }

    // save the notifier on the shared user
    Db.getUser('_moloch_shared', (err, sharedUser) => {
      if (!sharedUser || !sharedUser.found) {
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
      } else {
        sharedUser = sharedUser._source;
      }

      sharedUser.notifiers = sharedUser.notifiers || {};

      if (sharedUser.notifiers[req.body.name]) {
        if (Config.debug) {
          console.log('Trying to add duplicate notifier', sharedUser);
        }
        return res.molochError(403, 'Notifier already exists');
      }

      sharedUser.notifiers[req.body.name] = req.body;

      Db.setUser('_moloch_shared', sharedUser, (err, info) => {
        if (err) {
          console.log('/api/notifiers failed', err, info);
          return res.molochError(500, 'Creating notifier failed');
        }
        return res.send(JSON.stringify({
          success: true,
          text: 'Successfully created notifier',
          name: req.body.name
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
   * @returns {string} name - If successful, the name of the updated notifier.
   */
  module.updateNotifier = (req, res) => {
    Db.getUser('_moloch_shared', (err, sharedUser) => {
      if (!sharedUser || !sharedUser.found) {
        return res.molochError(404, 'Cannot find notifer to udpate');
      } else {
        sharedUser = sharedUser._source;
      }

      sharedUser.notifiers = sharedUser.notifiers || {};

      if (!sharedUser.notifiers[req.params.name]) {
        return res.molochError(404, 'Cannot find notifer to udpate');
      }

      if (!req.body.name) {
        return res.molochError(403, 'Missing a unique notifier name');
      }

      if (!req.body.type) {
        return res.molochError(403, 'Missing notifier type');
      }

      if (!req.body.fields) {
        return res.molochError(403, 'Missing notifier fields');
      }

      if (!Array.isArray(req.body.fields)) {
        return res.molochError(403, 'Notifier fields must be an array');
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

      if (!foundNotifier) { return res.molochError(403, 'Unknown notifier type'); }

      // check that required notifier fields exist
      for (const field of foundNotifier.fields) {
        if (field.required) {
          for (const sentField of req.body.fields) {
            if (sentField.name === field.name && !sentField.value) {
              return res.molochError(403, `Missing a value for ${field.name}`);
            }
          }
        }
      }

      sharedUser.notifiers[req.body.name] = {
        name: req.body.name,
        type: req.body.type,
        fields: req.body.fields
      };

      // delete the old notifier if the name has changed
      if (sharedUser.notifiers[req.params.name] && req.body.name !== req.params.name) {
        sharedUser.notifiers[req.params.name] = null;
        delete sharedUser.notifiers[req.params.name];
      }

      Db.setUser('_moloch_shared', sharedUser, (err, info) => {
        if (err) {
          console.log('/api/notifier update failed', err, info);
          return res.molochError(500, 'Updating notifier failed');
        }
        return res.send(JSON.stringify({
          success: true,
          text: 'Successfully updated notifier',
          name: req.body.name
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
  module.deleteNotifier = (req, res) => {
    Db.getUser('_moloch_shared', (err, sharedUser) => {
      if (!sharedUser || !sharedUser.found) {
        return res.molochError(404, 'Cannot find notifer to remove');
      } else {
        sharedUser = sharedUser._source;
      }

      sharedUser.notifiers = sharedUser.notifiers || {};

      if (!sharedUser.notifiers[req.params.name]) {
        return res.molochError(404, 'Cannot find notifer to remove');
      }

      sharedUser.notifiers[req.params.name] = undefined;

      Db.setUser('_moloch_shared', sharedUser, (err, info) => {
        if (err) {
          console.log('/api/notifier delete failed', err, info);
          return res.molochError(500, 'Deleting notifier failed');
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
  module.testNotifier = (req, res) => {
    function continueProcess (err) {
      if (err) {
        return res.molochError(500, `Error testing alert: ${err}`);
      }

      return res.send(JSON.stringify({
        success: true,
        text: `Successfully issued alert using the ${req.params.name} notifier.`
      }));
    }

    module.issueAlert(req.params.name, 'Test alert', continueProcess);
  };

  return module;
};
