#!/usr/bin/env node
'use strict';

// 1 - first version of parliament
// 2 - cluster.type instead of cluster.disabled/multiviewer

const version = 2;

module.exports = {

  /**
   * Upgrades the parliament object to the latest version
   * @param {object} parliament the parliament object to upgrade
   */
  upgrade: function (parliament) {
    parliament.version = version;

    // nothing to upgrade, just return parliament with new version
    if (!parliament.groups) { return parliament; }

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

    return parliament;
  }

}
