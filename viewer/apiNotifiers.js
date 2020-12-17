'use strict';

const glob = require('glob');

module.exports = (Db, internals) => {
  const module = {};

  module.buildNotifiers = () => {
    internals.notifiers = {};

    const api = {
      register: function (str, info) {
        internals.notifiers[str] = info;
      }
    };

    // look for all notifier providers and initialize them
    const files = glob.sync(`${__dirname}/../notifiers/provider.*.js`);
    files.forEach((file) => {
      const plugin = require(file);
      plugin.init(api);
    });
  };

  module.issueAlert = (notifierName, alertMessage, continueProcess) => {
    if (!notifierName) { return continueProcess('No name supplied for notifier'); }

    if (!internals.notifiers) { module.buildNotifiers(); }

    // find notifier
    Db.getUser('_moloch_shared', (err, sharedUser) => {
      if (!sharedUser || !sharedUser.found) {
        console.log('Cannot find notifier, no alert can be issued');
        return continueProcess('Cannot find notifier, no alert can be issued');
      }

      sharedUser = sharedUser._source;

      sharedUser.notifiers = sharedUser.notifiers || {};

      const notifier = sharedUser.notifiers[notifierName];

      if (!notifier) {
        console.log('Cannot find notifier, no alert can be issued');
        return continueProcess('Cannot find notifier, no alert can be issued');
      }

      let notifierDefinition;
      for (const n in internals.notifiers) {
        if (internals.notifiers[n].type === notifier.type) {
          notifierDefinition = internals.notifiers[n];
        }
      }
      if (!notifierDefinition) {
        console.log('Cannot find notifier definition, no alert can be issued');
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
          console.log(`Cannot find notifier field value: ${field.name}, no alert can be issued`);
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

  return module;
};
