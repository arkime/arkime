/******************************************************************************/
/* upgrade.js -- upgrade previous parliament config files
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

// 1 - first version of parliament
// 2 - cluster.type instead of cluster.disabled/multiviewer
// 3 - more than one of each type of notifier
// 4 - remove parliament password
// 5 - move settings to config
// 6 - combine notifiers with viewer
// 7 - remove parliament json

const uuid = require('uuid').v4;

const Notifier = require('../common/notifier');
const ArkimeConfig = require('../common/arkimeConfig');

const version = 7;

/**
 * Upgrades the parliament object to the latest version
 * @param {object} parliament the parliament object to upgrade
 * @param {object} issues the issues object to upgrade
 * @param {object} Parliament the Parliament class
 */
exports.upgrade = async function (parliament, issues, Parliament) {
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

  // add parliament notifiers to db
  if (parliament.settings && parliament.settings.notifiers) {
    for (const n in parliament.settings.notifiers) {
      const notifier = parliament.settings.notifiers[n];

      // get viewer notifiers that match the parliament notifier name
      const query = { bool: { must: { term: { name: notifier.name } } } };
      const { body: matchingNotifiers } = await Notifier.searchNotifiers({ query });

      // find out if there is a matching notifier in viewer
      let nameCollision = false;
      if (matchingNotifiers.hits.total > 0) {
        for (const hit of matchingNotifiers.hits.hits) {
          if (hit._source.name === notifier.name) {
            nameCollision = true;
            break;
          }
        }
      }

      if (nameCollision) { // update the name of the Parliament notifier if there is a name collision with viewer
        console.log(`WARNING - Notifier with name ${notifier.name} already exists. Renaming to "Parliament ${notifier.name}"`);
        notifier.name = `Parliament ${notifier.name}`;
      }

      // viewer uses fields array (parliament uses fields object)
      // map parliament notifier fields to an array
      const fields = [];
      for (const f in notifier.fields) {
        fields.push(notifier.fields[f]);
      }
      notifier.fields = fields;

      // parliament saved the entire alert object
      // let's just save the on/off state instead
      const updatedAlerts = {};
      for (const a in notifier.alerts) {
        const nAlert = notifier.alerts[a];
        if (typeof nAlert === 'object' && 'on' in nAlert) {
          updatedAlerts[a] = nAlert.on;
        } else {
          updatedAlerts[a] = nAlert;
        }
      }
      notifier.alerts = updatedAlerts;

      // parliament doesn't have these fields, add them
      notifier.roles ??= ['parliamentUser'];
      notifier.user ??= 'migrated from parliament';
      notifier.users ??= [];
      notifier.created ??= Math.floor(Date.now() / 1000);

      await Notifier.createNotifier(notifier);
    }

    delete parliament.settings.notifiers;
  }

  if (parliament) { // add parliament to db
    delete parliament.version; // don't need version anymore
    delete parliament.authMode; // don't need authmode anymore
    parliament.name = Parliament.name; // parliament name is the id

    // remove healtherror and statserror
    for (const group of parliament.groups) {
      group.id = uuid(); // generate a new id for the group
      for (const cluster of group.clusters) {
        const oldClusterId = cluster.id;
        cluster.id = uuid(); // generate a new id for the cluster
        for (const issue of issues) { // update issues with new id
          if (issue.clusterId === oldClusterId) {
            issue.clusterId = cluster.id;
          }
        }
        delete cluster.healthError;
        delete cluster.statsError;
      }
    }

    console.log('Adding Parliament to DB...');

    try {
      await Parliament.createParliament(parliament);
    } catch (err) {
      if (err.meta?.statusCode === 409) {
        console.log('Parliament already exists in DB. Skipping!', err);
      } else {
        console.error('ERROR - Couldn\'t add Parliament to DB.', err);
      }
    }
  }

  // update version
  parliament.version = version;

  return { parliament, issues };
};
