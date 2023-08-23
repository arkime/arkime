#!/usr/bin/env node
'use strict';

// 1 - first version of parliament
// 2 - cluster.type instead of cluster.disabled/multiviewer
// 3 - more than one of each type of notifier
// 4 - remove parliament password
// 5 - move settings to config

const version = 5;

module.exports = {
  /**
   * Upgrades the parliament object to the latest version
   * @param {object} parliament the parliament object to upgrade
   */
  upgrade: async function (parliament, notifierDefs, ArkimeConfig) {
    // fix cluster types
    if (parliament.groups) {
      for (const group of parliament.groups) {
        if (group.clusters) {
          for (const cluster of group.clusters) {
            if (cluster.multiviewer) {
              delete cluster.multiviewer;
              cluster.type = 'multiviewer';
            } else if (cluster.disabled) {
              delete cluster.disabled;
              cluster.type = 'disabled';
            }
          }
        }
      }
    }

    // fix notifiers
    if (parliament.settings && parliament.settings.notifiers) {
      // only save the notifiers with values
      for (const n in parliament.settings.notifiers) {
        const notifier = parliament.settings.notifiers[n];
        let hasValues = false;

        // check for values in notifiers
        for (const f in notifier.fields) {
          if (notifier.fields[f].value) {
            hasValues = true;
            break;
          }
        }

        // add or lowercase the type
        if (notifier.type) {
          notifier.type = notifier.type.toLowerCase();
        } else {
          notifier.type = n;
        }

        // if the notifier has no values, it's not being used, so remove it
        if (!hasValues) { parliament.settings.notifiers[n] = undefined; }
      }
    }

    // remove parliament password
    if (parliament.password) {
      delete parliament.password;
    }

    if (parliament.settings?.commonAuth) {
      if (!ArkimeConfig.canSave()) {
        console.log('ERROR - Can\'t save config file. You need to create it manually. See https://arkime.com/settings#parliament');
      }

      let config = {};
      if (ArkimeConfig.getSection('parliament')) {
        config = ArkimeConfig.getSection('parliament');
      }

      let changed = false;
      // use commonAuth settings if they exist to populate parliament auth config settings
      if (!config.authMode && parliament.settings.commonAuth?.authMode) {
        changed = true;
        config.authMode = parliament.settings.commonAuth.authMode;
      }
      if (!config.userNameHeader && parliament.settings.commonAuth?.userNameHeader) {
        changed = true;
        config.userNameHeader = parliament.settings.commonAuth.userNameHeader;
      }
      if (!config.usersElasticsearch && parliament.settings.commonAuth?.usersElasticsearch) {
        changed = true;
        config.usersElasticsearch = parliament.settings.commonAuth.usersElasticsearch;
      }
      if (!config.usersPrefix && parliament.settings.commonAuth?.usersPrefix) {
        changed = true;
        config.usersPrefix = parliament.settings.commonAuth.usersPrefix;
      }
      if (!config.usersElasticsearchAPIKey && parliament.settings.commonAuth?.usersElasticsearchAPIKey) {
        changed = true;
        config.usersElasticsearchAPIKey = parliament.settings.commonAuth.usersElasticsearchAPIKey;
      }
      if (!config.usersElasticsearchBasicAuth && parliament.settings.commonAuth?.usersElasticsearchBasicAuth) {
        changed = true;
        config.usersElasticsearchBasicAuth = parliament.settings.commonAuth.usersElasticsearchBasicAuth;
      }
      if (!config.passwordSecret && parliament.settings.commonAuth?.passwordSecret) {
        changed = true;
        config.passwordSecret = parliament.settings.commonAuth.passwordSecret;
      }
      if (!config.httpRealm && parliament.settings.commonAuth?.httpRealm) {
        changed = true;
        config.httpRealm = parliament.settings.commonAuth.httpRealm;
      }
      if (!config.parliamentHost && parliament.settings.commonAuth?.parliamentHost) {
        changed = true;
        config.parliamentHost = parliament.settings.commonAuth.parliamentHost;
      }

      if (changed) { // update the config if it was changed by the commonAuth settings
        console.log('Transferring Parliament auth settings to parliament.ini...');
        ArkimeConfig.replace({ parliament: config });
        ArkimeConfig.save((err) => {
          if (err) {
            console.error('ERROR - Couldn\'t save config file. Please update your parliament.ini file manually. See https://arkime.com/settings#parliament\n', err);
          } else {
            console.log('Transfer successful!\n');
          }
        });
      }

      delete parliament.settings.commonAuth;
    }

    // update version
    parliament.version = version;

    return parliament;
  }
};
