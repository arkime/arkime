#!/usr/bin/env node
'use strict';

// 1 - first version of parliament
// 2 - cluster.type instead of cluster.disabled/multiviewer
// 3 - more than one of each type of notifier

const version = 3;

module.exports = {
  /**
   * Upgrades the parliament object to the latest version
   * @param {object} parliament the parliament object to upgrade
   */
  upgrade: function (parliament, notifierDefs) {
    // fix cluster types
    if (parliament.groups) {
      for (let group of parliament.groups) {
        if (group.clusters) {
          for (let cluster of group.clusters) {
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
      for (let n in parliament.settings.notifiers) {
        let notifier = parliament.settings.notifiers[n];
        let hasValues = false;

        // check for values in notifiers
        for (let f in notifier.fields) {
          if (notifier.fields[f].value) {
            hasValues = true;
            break;
          }
        }

        // add or lowercase the type
        if (notifier.type) { notifier.type = notifier.type.toLowerCase(); }
        else { notifier.type = n; }

        // if the notifier has no values, it's not being used, so remove it
        if (!hasValues) { parliament.settings.notifiers[n] = undefined; }
      }
    }

    // update version
    parliament.version = version;

    return parliament;
  }
}
