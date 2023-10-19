#!/usr/bin/env node
/******************************************************************************/
/* parliament.js
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const MIN_PARLIAMENT_VERSION = 7;
const MIN_DB_VERSION = 79;

// ----------------------------------------------------------------------------
// DEPENDENCIES
// ----------------------------------------------------------------------------
const express = require('express');
const https = require('https');
const fs = require('fs');
const favicon = require('serve-favicon');
const bp = require('body-parser');
const os = require('os');
const helmet = require('helmet');
const uuid = require('uuid').v4;
const upgrade = require('./upgrade');
const path = require('path');
const axios = require('axios');
const LRU = require('lru-cache');
const dayMs = 60000 * 60 * 24;
const User = require('../common/user');
const Auth = require('../common/auth');
const version = require('../common/version');
const Notifier = require('../common/notifier');
const ArkimeUtil = require('../common/arkimeUtil');
const ArkimeConfig = require('../common/arkimeConfig');

// ----------------------------------------------------------------------------
// APP SETUP
// ----------------------------------------------------------------------------
const app = express();

const issueTypes = {
  esRed: { on: true, name: 'ES Red', text: 'ES is red', severity: 'red', description: 'ES status is red' },
  esDown: { on: true, name: 'ES Down', text: ' ES is down', severity: 'red', description: 'ES is unreachable' },
  esDropped: { on: true, name: 'ES Dropped', text: 'ES is dropping bulk inserts', severity: 'yellow', description: 'the capture node is overloading ES' },
  outOfDate: { on: true, name: 'Out of Date', text: 'has not checked in since', severity: 'red', description: 'the capture node has not checked in' },
  noPackets: { on: true, name: 'Low Packets', text: 'is not receiving many packets', severity: 'red', description: 'the capture node is not receiving many packets' }
};

const parliamentReadError = `
You must fix this before you can run Parliament.
Try using parliament.example.json as a starting point.
Use the "file" setting in your Parliament config to point to your Parliament JSON file.
See https://arkime.com/settings#parliament for more information.
`;

const internals = {
  stats: {},
  parliamentName: 'parliament',
  httpsAgent: new https.Agent({ rejectUnauthorized: !ArkimeConfig.insecure })
};

// ----------------------------------------------------------------------------
// CONFIG
// ----------------------------------------------------------------------------
(function () {
  for (let i = 0, ilen = process.argv.length; i < ilen; i++) {
    if (process.argv[i] === '-o') {
      i++;
      const equal = process.argv[i].indexOf('=');
      if (equal === -1) {
        console.log('Missing equal sign in', process.argv[i]);
        process.exit(1);
      }
      ArkimeConfig.setOverride(process.argv[i].slice(0, equal), process.argv[i].slice(equal + 1));
    } else if (process.argv[i] === '-n' || process.argv[i] === '--name') {
      internals.parliamentName = process.argv[++i];
    } else if (process.argv[i] === '--help') {
      console.log('parliament.js [<config options>]\n');
      console.log('Config Options:');
      console.log('  -c, --config                Parliament config file to use');
      console.log('  -n, --name <name>           Name of the Parliament for if you have multiple parliaments (defaults to "Parliament")');
      console.log('  -o <section>.<key>=<value>  Override the config file');
      console.log('  --debug                     Increase debug level, multiple are supported');
      console.log('  --insecure                  Disable certificate verification for https calls');

      process.exit(0);
    }
  }
}());

if (ArkimeConfig.regressionTests) {
  app.post('/regressionTests/shutdown', function (req, res) {
    process.exit(0);
  });
}

// parliament object!
let parliamentFile;
// issues object!
let issues;

// save noPackets issues so that the time of issue can be compared to the
// noPacketsLength user setting (only issue alerts when the time the issue
// was encounterd exceeds the noPacketsLength user setting)
const noPacketsMap = {};

// super secret
app.use(helmet.hidePoweredBy());
app.use(helmet.xssFilter());
app.use(helmet.hsts({
  maxAge: 31536000,
  includeSubDomains: true
}));
// calculate nonce
app.use((req, res, next) => {
  res.locals.nonce = Buffer.from(uuid()).toString('base64');
  next();
});
// define csp headers
const cspDirectives = {
  defaultSrc: ["'self'"],
  styleSrc: ["'self'"],
  // need unsafe-eval for vue full build: https://vuejs.org/v2/guide/installation.html#CSP-environments
  scriptSrc: ["'self'", "'unsafe-eval'", (req, res) => `'nonce-${res.locals.nonce}'`],
  objectSrc: ["'none'"],
  imgSrc: ["'self'"]
};
if (process.env.NODE_ENV === 'development') {
  // need unsafe inline styles for hot module replacement
  cspDirectives.styleSrc.push("'unsafe-inline'");
}
const cspHeader = helmet.contentSecurityPolicy({
  directives: cspDirectives
});
app.use(cspHeader);

function setCookie (req, res, next) {
  const cookieOptions = {
    path: '/',
    sameSite: 'Strict',
    overwrite: true
  };
  // make cookie secure on https
  if (ArkimeConfig.get('keyFile') && ArkimeConfig.get('certFile')) { cookieOptions.secure = true; }

  res.cookie( // send cookie for basic, non admin functions
    'PARLIAMENT-COOKIE',
    Auth.obj2auth({
      date: Date.now(),
      pid: process.pid,
      userId: req.user.userId
    }),
    cookieOptions
  );

  return next();
}

function checkCookieToken (req, res, next) {
  if (!req.headers['x-parliament-cookie']) {
    return res.serverError(500, 'Missing token');
  }

  const cookie = req.headers['x-parliament-cookie'];
  req.token = Auth.auth2obj(cookie);
  const diff = Math.abs(Date.now() - req.token.date);
  if (diff > 2400000 || req.token.userId !== req.user.userId) {
    console.trace('bad token', req.token, diff, req.token.userId, req.user.userId);
    return res.serverError(500, 'Timeout - Please try reloading page and repeating the action');
  }

  return next();
}

// using fallthrough: false because there is no 404 endpoint (client router
// handles 404s) and sending index.html is confusing
app.use('/parliament/font-awesome', express.static(
  path.join(__dirname, '/../node_modules/font-awesome'),
  { maxAge: dayMs, fallthrough: false }
), ArkimeUtil.missingResource);
app.use('/parliament/assets', express.static(
  path.join(__dirname, '/../assets'),
  { maxAge: dayMs, fallthrough: false }
), ArkimeUtil.missingResource);

// log requests
ArkimeUtil.logger(app);

app.use(favicon(path.join(__dirname, '/favicon.ico')));

// Set up auth, all APIs registered below will use passport
Auth.app(app);

app.use(ArkimeUtil.jsonParser);
app.use(bp.urlencoded({ extended: true }));

function newError (code, msg) {
  const error = new Error(msg);
  error.httpStatusCode = code;
  return error;
}

// ----------------------------------------------------------------------------
// PARLIAMENT CLASS
// ----------------------------------------------------------------------------
class Parliament {
  static name;
  static #esclient;
  static #parliamentIndex;
  static #cache = new LRU({ max: 1000, maxAge: 1000 * 60 });

  static settingsDefault = {
    general: {
      noPackets: 0,
      noPacketsLength: 10,
      outOfDate: 30,
      esQueryTimeout: 5,
      removeIssuesAfter: 60,
      removeAcknowledgedAfter: 15
    }
  };

  static async initialize (options) {
    Parliament.name = options.name;
    Parliament.#esclient = options.esclient;

    const prefix = ArkimeUtil.formatPrefix(options.prefix);

    Parliament.#parliamentIndex = `${prefix}parliament`;
  }

  // DB INTERACTIONS ---------------------------------------------------------
  static async getParliament () {
    return Parliament.#esclient.get({
      index: Parliament.#parliamentIndex, id: Parliament.name
    });
  }

  static async createParliament (parliament) {
    return Parliament.#esclient.create({
      index: Parliament.#parliamentIndex, body: parliament, id: Parliament.name, timeout: '10m'
    });
  }

  static async setParliament (parliament) {
    try {
      const response = await Parliament.#esclient.index({
        index: Parliament.#parliamentIndex, body: parliament, id: Parliament.name, refresh: true, timeout: '10m'
      });

      Parliament.#cache.set('parliament', parliament);
      return response;
    } catch (err) {
      if (ArkimeConfig.debug) {
        console.log('Error setting parliament', err);
      }
      throw err;
    }
  }

  // APIS --------------------------------------------------------------------
  /**
   * The Parliament configuration continaing all the settings, groups, and clusters.
   * @typedef Parliament
   * @type {object}
   * @property {string} name - The name of the Parliament.
   * @property {ParliamentSettings} settings - The settings for Parliament.
   * @property {Array.<ArkimeGroup>} groups - The groups for Parliament.
   */

  /**
   * The Parliament settings.
   * @typedef ParliamentSettings
   * @type {object}
   * @property {boolean} noPackets - The minimum number of packets that the capture node must receive. If the capture node is not receiving enough packets, a Low Packets issue is added to the cluster. You can set this value to -1 to ignore this issue altogether.
   * @property {number} noPacketsLength - The time range for how long the no packets issue must persist before adding an issue to the cluster. The default for this setting is 0 packets for 10 seconds.
   * @property {boolean} outOfDate - How behind a node's cluster's timestamp can be from the current time. If the timestamp exceeds this time setting, an Out Of Date issue is added to the cluster. The default for this setting is 30 seconds.
   * @property {number} esQueryTimeout - The maximum Elasticsearch status query duration. If the query exceeds this time setting, an ES Down issue is added to the cluster. The default for this setting is 5 seconds.
   * @property {number} removeIssuesAfter - When an issue is removed if it has not occurred again. The issue is removed from the cluster after this time expires as long as the issue has not occurred again. The default for this setting is 60 minutes.
   * @property {number} removeAcknowledgedAfter - When an acknowledged issue is removed. The issue is removed from the cluster after this time expires (so you don't have to remove issues manually with the trashcan button). The default for this setting is 15 minutes.
   * @property {string} hostname - The hostname of the Parliament instance. Configure the Parliament's hostname to add a link to the Parliament Dashbaord to every alert.
   */

  /**
   * The Groups within your Parliament
   * @typedef ArkimeGroup
   * @type {object}
   * @property {string} title - The title of the Group.
   * @property {string} description - The description of the Group.
   * @property {Array.<ArkimeClusters>} clusters - The clusters in the Group.
   */

  /**
   * The Clusters within your Parliament
   * @typedef ArkimeCluster
   * @type {object}
   * @property {string} title - The title of the Cluster.
   * @property {string} description - The description of the Cluster.
   * @property {string} url - The url of the Cluster.
   * @property {string} localUrl - The local url of the Cluster.
   * @property {string} type - The type of the Cluster.
   * @property {string} id - The unique ID of the Cluster.
   * @property {string} hideDeltaBPS - Whether to hide the delta bits per second of the Cluster.
   * @property {string} hideDeltaTDPS - Whether to hide the delta packet drops per second of the Cluster.
   * @property {string} hideMonitoring - Whether to hide number of sessions being recorded of the Cluster.
   * @property {string} hideMolochNodes - Whether to hide the number of Arkime nodes of the Cluster.
   * @property {string} hideDataNodes - Whether to hide the number of data nodes of the Cluster.
   * @property {string} hideTotalNodes - Whether to hide the number of total nodes of the Cluster.
   */

  /**
   * GET - /api/parliament
   *
   * Retrieves a parliament by id (name).
   * @name /parliament
   * @returns {Parliament} parliament - The requested parliament
   */
  static async apiGetParliament (req, res) {
    try {
      const { body: { _source: parliament } } = await Parliament.getParliament();

      Parliament.#cache.set('parliament', parliament);

      const parliamentClone = JSON.parse(JSON.stringify(parliament));

      if (!req.user.hasRole('parliamentAdmin')) {
        delete parliamentClone.settings;
      }

      return res.json(parliamentClone);
    } catch (err) {
      if (ArkimeConfig.debug) {
        console.log('Error fetching parliament', err);
      }
      return res.serverError(500, 'Error fetching parliament');
    }
  }

  /**
   * PUT - /api/parliament/settings
   *
   * Updates the parliament settings. Requires parliamentAdmin role.
   * @name /parliament/settings
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async apiUpdateSettings (req, res, next) {
    if (!req.body.settings?.general) {
      return res.serverError(422, 'You must provide the settings to update.');
    }

    try {
      const { body: { _source: parliament } } = await Parliament.getParliament();

      // save general settings
      for (const s in req.body.settings.general) {
        let setting = req.body.settings.general[s];

        if (s === 'hostname') { // hostname must be a string
          if (!ArkimeUtil.isString(setting)) {
            return res.serverError(422, 'hostname must be a string.');
          }
        } else if (s === 'wiseUrl' || s === 'cont3xtUrl') { // urls must be strings or empty
          if (setting && !ArkimeUtil.isString(setting)) {
            return res.serverError(422, `${s} must be a string.`);
          }
        } else if (s === 'includeUrl') { // include url must be a bool
          if (typeof setting !== 'boolean') {
            return res.serverError(422, 'includeUrl must be a boolean.');
          }
        } else { // all other settings are numbers
          if (isNaN(setting)) {
            return res.serverError(422, `${s} must be a number.`);
          } else {
            setting = parseInt(setting);
          }
        }

        parliament.settings.general[s] = setting;
      }

      await Parliament.setParliament(parliament);
      res.json({ success: true, text: 'Successfully updated settings.' });
    } catch (e) {
      if (ArkimeConfig.debug) {
        console.log('Error updating settings', e);
      }
      res.serverError(500, 'Unable to update settings.');
    }
  }

  /**
   * PUT - /api/settings/restoreDefaults
   *
   * Restores the default settings. Requires parliamentAdmin role.
   * @name /settings/restoreDefaults
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */

  static async apiRestoreDefaultSettings (req, res, next) {
    try {
      const { body: { _source: parliament } } = await Parliament.getParliament();

      parliament.settings = Parliament.settingsDefault;

      await Parliament.setParliament(parliament);
      res.json({ success: true, text: 'Successfully restored default settings.', settings: parliament.settings });
    } catch (e) {
      if (ArkimeConfig.debug) {
        console.log('Error restoring default settings', e);
      }
      res.serverError(500, 'Unable to restore default settings.');
    }
  }

  /**
   * PUT - /api/parliament
   *
   * Updates a parliament's order of groups/clusters. Requires parliamentAdmin role.
   * @name /parliament
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {Parliament} parliament - The updated parliament.
   */
  static async apiUpdateParliamentOrder (req, res) {
    try {
      const { body: { _source: parliament } } = await Parliament.getParliament();

      if (isNaN(req.body.oldIdx) || isNaN(req.body.newIdx)) {
        return res.serverError(500, 'Error updating parliament order. Need old and new indexes!');
      }

      if (req.body.newGroupId) { // we're rearranging clusters
        if (!ArkimeUtil.isString(req.body.newGroupId) || !ArkimeUtil.isString(req.body.oldGroupId)) {
          return res.serverError(422, 'Error updating parliament order. Old and new group ids must be strings!');
        }

        const newGroup = parliament.groups.filter(group => group.id === req.body.newGroupId);
        if (!newGroup.length) { return res.serverError(500, 'Error updating parliament order. Can\'t find group to place cluster.'); }

        const oldGroup = parliament.groups.filter(group => group.id === req.body.oldGroupId);
        if (!oldGroup.length) { return res.serverError(500, 'Error updating parliament order. Can\'t find group to move cluster from.'); }

        const cluster = oldGroup[0].clusters[req.body.oldIdx];
        if (!cluster) { return res.serverError(500, 'Error updating parliament order. Can\'t find cluster to move.'); }

        oldGroup[0].clusters.splice(req.body.oldIdx, 1);
        newGroup[0].clusters.splice(req.body.newIdx, 0, cluster);
      } else { // we're rearranging groups
        const group = parliament.groups[req.body.oldIdx];
        if (!group) {
          return res.serverError(500, 'Error updating parliament order. Can\'t find group to move.');
        }
        parliament.groups.splice(req.body.oldIdx, 1);
        parliament.groups.splice(req.body.newIdx, 0, group);
      }

      await Parliament.setParliament(parliament);
      return res.json({ success: true, text: 'Parliament updated successfully' });
    } catch (err) {
      if (ArkimeConfig.debug) {
        console.log('Error updating parliament', err);
      }
      return res.serverError(500, 'Error updating parliament');
    }
  }

  /**
   * POST - /api/groups
   *
   * Creates a new group in the parliament. Requires parliamentAdmin role.
   * @name /groups
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {Group} group - The new group including its unique id (if successful).
   */
  static async apiCreateGroup (req, res, next) {
    if (!ArkimeUtil.isString(req.body.title)) {
      return res.serverError(422, 'A group must have a title');
    }

    if (req.body.description && !ArkimeUtil.isString(req.body.description)) {
      return res.serverError(422, 'A group must have a string description.');
    }

    const newGroup = { title: req.body.title, id: uuid(), clusters: [] };
    newGroup.description ??= req.body.description;

    try {
      const { body: { _source: parliament } } = await Parliament.getParliament();
      parliament.groups.push(newGroup);

      await Parliament.setParliament(parliament);
      res.json({ success: true, group: newGroup, text: 'Successfully added new group.' });
    } catch (e) {
      if (ArkimeConfig.debug) {
        console.log('Error adding new group', e);
      }
      res.serverError(500, 'Unable to add new group.');
    }
  }

  /**
   * DELETE - /api/groups/:id
   *
   * Deletes a group from the parliament. Requires parliamentAdmin role.
   * @name /groups/:id
   * @param {string} id - The id of the group to delete.
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async apiDeleteGroup (req, res, next) {
    try {
      const { body: { _source: parliament } } = await Parliament.getParliament();

      let index = 0;
      let foundGroup = false;
      for (const group of parliament.groups) {
        if (group.id === req.params.id) {
          parliament.groups.splice(index, 1);
          foundGroup = true;
          break;
        }
        ++index;
      }

      if (!foundGroup) {
        return res.serverError(500, 'Unable to find group to delete.');
      }

      await Parliament.setParliament(parliament);
      res.json({ success: true, text: 'Successfully removed group.' });
    } catch (e) {
      if (ArkimeConfig.debug) {
        console.log('Error removing group', e);
      }
      res.serverError(500, 'Unable to remove group.');
    }
  }

  /**
   * PUT - /api/groups/:id
   *
   * Updates a group in the parliament. Requires parliamentAdmin role.
   * @name /groups/:id
   * @param {string} id - The id of the group to update.
   * @param {Group} group - The updated group.
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */

  static async apiUpdateGroup (req, res, next) {
    if (!ArkimeUtil.isString(req.body.title)) {
      return res.serverError(422, 'A group must have a title.');
    }

    if (req.body.description && !ArkimeUtil.isString(req.body.description)) {
      return res.serverError(422, 'A group must have a string description.');
    }

    try {
      const { body: { _source: parliament } } = await Parliament.getParliament();

      let foundGroup = false;
      for (const group of parliament.groups) {
        if (group.id === req.params.id) {
          group.title = req.body.title;
          group.description = req.body.description;
          foundGroup = true;
          break;
        }
      }

      if (!foundGroup) {
        return res.serverError(500, 'Unable to find group to edit.');
      }

      await Parliament.setParliament(parliament);
      res.json({ success: true, text: 'Successfully updated the group.' });
    } catch (e) {
      if (ArkimeConfig.debug) {
        console.log('Error updating group', e);
      }
      res.serverError(500, 'Unable to update group.');
    }
  }

  /**
   * POST /api/groups/:id/clusters
   *
   * Creates a new cluster in the group. Requires parliamentAdmin role.
   * @name /groups/:id/clusters
   * @param {string} id - The id of the group to add the cluster to.
   * @param {Cluster} cluster - The cluster to add to the group.
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {Cluster} cluster - The new cluster including its unique id (if successful).
   */
  static async apiCreateCluster (req, res, next) {
    if (!ArkimeUtil.isString(req.body.title)) {
      return res.serverError(422, 'A cluster must have a title.');
    }

    if (!ArkimeUtil.isString(req.body.url)) {
      return res.serverError(422, 'A cluster must have a url.');
    }

    if (req.body.description && !ArkimeUtil.isString(req.body.description)) {
      return res.serverError(422, 'A cluster must have a string description.');
    }

    if (req.body.localUrl && !ArkimeUtil.isString(req.body.localUrl)) {
      return res.serverError(422, 'A cluster must have a string localUrl.');
    }

    if (req.body.type && !ArkimeUtil.isString(req.body.type)) {
      return res.serverError(422, 'A cluster must have a string type.');
    }

    const newCluster = {
      title: req.body.title,
      description: req.body.description,
      url: req.body.url,
      localUrl: req.body.localUrl,
      id: uuid(),
      type: req.body.type,
      hideDeltaBPS: false,
      hideDeltaTDPS: false,
      hideMonitoring: false,
      hideMolochNodes: false,
      hideDataNodes: false,
      hideTotalNodes: false
    };

    try {
      const { body: { _source: parliament } } = await Parliament.getParliament();

      let foundGroup = false;
      for (const group of parliament.groups) {
        if (group.id === req.params.id) {
          group.clusters.push(newCluster);
          foundGroup = true;
          break;
        }
      }

      if (!foundGroup) {
        return res.serverError(500, 'Unable to find group to place cluster.');
      }

      await Parliament.setParliament(parliament);
      res.json({ success: true, cluster: newCluster, text: 'Successfully added the cluster.' });
    } catch (e) {
      if (ArkimeConfig.debug) {
        console.log('Error creating cluster', e);
      }
      res.serverError(500, 'Unable to create cluster.');
    }
  }

  /**
   * DELETE - /api/groups/:id/clusters/:clusterId
   *
   * Deletes a cluster from the group. Requires parliamentAdmin role.
   * @name /groups/:id/clusters/:clusterId
   * @param {string} id - The id of the group to delete the cluster from.
   * @param {string} clusterId - The id of the cluster to delete.
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async apiDeleteCluster (req, res, next) {
    try {
      const { body: { _source: parliament } } = await Parliament.getParliament();

      let clusterIndex = 0;
      let foundCluster = false;
      for (const group of parliament.groups) {
        if (group.id === req.params.groupId) {
          for (const cluster of group.clusters) {
            if (cluster.id === req.params.clusterId) {
              group.clusters.splice(clusterIndex, 1);
              foundCluster = true;
              break;
            }
            ++clusterIndex;
          }
        }
      }

      if (!foundCluster) {
        return res.serverError(500, 'Unable to find cluster to delete.');
      }

      await Parliament.setParliament(parliament);
      res.json({ success: true, text: 'Successfully removed the cluster.' });
    } catch (e) {
      if (ArkimeConfig.debug) {
        console.log('Error removing cluster', e);
      }
      res.serverError(500, 'Unable to remove cluster.');
    }
  }

  /**
   * PUT - /api/groups/:groupId/clusters/:clusterId
   *
   * Updates a cluster in the group. Requires parliamentAdmin role.
   * @name /groups/:groupId/clusters/:clusterId
   * @param {string} groupId - The id of the group to update the cluster in.
   * @param {string} clusterId - The id of the cluster to update.
   * @param {Cluster} cluster - The updated cluster.
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async apiUpdateCluster (req, res, next) {
    if (!ArkimeUtil.isString(req.body.title)) {
      return res.serverError(422, 'A cluster must have a title.');
    }

    if (!ArkimeUtil.isString(req.body.url)) {
      return res.serverError(422, 'A cluster must have a url.');
    }

    if (req.body.description && !ArkimeUtil.isString(req.body.description)) {
      return res.serverError(422, 'A cluster must have a string description.');
    }

    if (req.body.localUrl && !ArkimeUtil.isString(req.body.localUrl)) {
      return res.serverError(422, 'A cluster must have a string localUrl.');
    }

    if (req.body.type && !ArkimeUtil.isString(req.body.type)) {
      return res.serverError(422, 'A cluster must have a string type.');
    }

    try {
      const { body: { _source: parliament } } = await Parliament.getParliament();

      let foundCluster = false;
      for (const group of parliament.groups) {
        if (group.id === req.params.groupId) {
          for (const cluster of group.clusters) {
            if (cluster.id === req.params.clusterId) {
              cluster.url = req.body.url;
              cluster.title = req.body.title;
              cluster.localUrl = req.body.localUrl;
              cluster.description = req.body.description;
              cluster.hideDeltaBPS = typeof req.body.hideDeltaBPS === 'boolean' ? req.body.hideDeltaBPS : undefined;
              cluster.hideDataNodes = typeof req.body.hideDataNodes === 'boolean' ? req.body.hideDataNodes : undefined;
              cluster.hideDeltaTDPS = typeof req.body.hideDeltaTDPS === 'boolean' ? req.body.hideDeltaTDPS : undefined;
              cluster.hideTotalNodes = typeof req.body.hideTotalNodes === 'boolean' ? req.body.hideTotalNodes : undefined;
              cluster.hideMonitoring = typeof req.body.hideMonitoring === 'boolean' ? req.body.hideMonitoring : undefined;
              cluster.hideMolochNodes = typeof req.body.hideMolochNodes === 'boolean' ? req.body.hideMolochNodes : undefined;
              cluster.type = req.body.type;

              foundCluster = true;
              break;
            }
          }
        }
      }

      if (!foundCluster) {
        return res.serverError(500, 'Unable to find cluster to update.');
      }

      await Parliament.setParliament(parliament);
      res.json({ success: true, text: 'Successfully updated the cluster.' });
    } catch (e) {
      if (ArkimeConfig.debug) {
        console.log('Error updating cluster', e);
      }
      res.serverError(500, 'Unable to update cluster.');
    }
  }

  // HELPERS -----------------------------------------------------------------
  /**
   * Retrieves a general setting from the parliament
   * Caches it for 1 minute
   * @param {string} type - The type of setting to retrieve.
   */
  static async getGeneralSetting (type) {
    let parliament = Parliament.#cache.get('parliament');

    if (!parliament) {
      const { body: { _source: updatedParliament } } = await Parliament.getParliament();
      Parliament.#cache.set('parliament', updatedParliament);
      parliament = updatedParliament;
    }

    return parliament?.settings?.general[type] ?? Parliament.settingsDefault.general[type];
  }
}

// ----------------------------------------------------------------------------
// MIDDLEWARE
// ----------------------------------------------------------------------------
// Replace the default express error handler
app.use((err, req, res, next) => {
  console.log(ArkimeUtil.sanitizeStr(err.stack));
  res.status(err.httpStatusCode ?? 500).json({
    success: false,
    text: err.message ?? 'Error'
  });
});

app.use((req, res, next) => {
  res.serverError = ArkimeUtil.serverError;
  return next();
});

function isUser (req, res, next) {
  if (req.user.hasRole('parliamentUser')) {
    return next();
  }

  res.status(403).json({
    success: false,
    text: 'Permission Denied: Not a Parliament user'
  });
}

function isAdmin (req, res, next) {
  if (req.user.hasRole('parliamentAdmin')) {
    return next();
  }

  res.status(403).json({
    success: false,
    text: 'Permission Denied: Not a Parliament admin'
  });
}

// ----------------------------------------------------------------------------
// HELPERS
// ----------------------------------------------------------------------------
// list of alerts that will be sent at every 10 seconds
let alerts = [];
// sends alerts in the alerts list
async function sendAlerts () {
  const hostname = await Parliament.getGeneralSetting('hostname');
  const promise = new Promise((resolve, reject) => {
    for (let index = 0, len = alerts.length; index < len; index++) {
      (function (i) {
        // timeout so that alerts are alerted in order
        setTimeout(() => {
          const alertToSend = alerts[i];
          const links = [];
          if (Parliament.getGeneralSetting('includeUrl')) {
            links.push({
              text: 'Parliament Dashboard',
              url: `${hostname}?searchTerm=${alertToSend.cluster}`
            });
          }
          alertToSend.notifier.sendAlert(alertToSend.config, alertToSend.message, links);
          if (ArkimeConfig.debug) {
            console.log('Sending alert:', alertToSend.message, JSON.stringify(alertToSend.config, null, 2));
          }
          if (i === len - 1) { resolve(); }
        }, 250 * i);
      })(index);
    }
  });

  promise.then(() => {
    alerts = []; // clear the queue
  });
}

// sorts the list of alerts by cluster title then sends them
// assumes that the alert message starts with the cluster title
function processAlerts () {
  if (alerts && alerts.length) {
    alerts.sort((a, b) => {
      return a.message.localeCompare(b.message);
    });

    sendAlerts();
  }
}

function formatIssueMessage (cluster, issue) {
  let message = '';

  if (issue.node) { message += `${issue.node} `; }

  message += `${issue.text}`;

  if (issue.value !== undefined) {
    let value = ': ';

    if (issue.type === 'esDropped') {
      value += issue.value.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ',');
    } else if (issue.type === 'outOfDate') {
      value += new Date(issue.value);
    } else {
      value += issue.value;
    }

    message += `${value}`;
  }

  return message;
}

async function buildAlert (cluster, issue) {
  const { body: notifiers } = await Notifier.searchNotifiers({ query: { match_all: {} } });

  // if there are no notifiers set, skip everything, there's nowhere to alert
  if (notifiers.hits.total === 0) { return; }

  issue.alerted = Date.now();

  const message = `${cluster.title} - ${issue.message}`;

  for (const n in notifiers.hits.hits) {
    const setNotifier = notifiers.hits.hits[n]._source;

    // keep looking for notifiers if the notifier is off
    if (!setNotifier || !setNotifier.on) { continue; }

    // quit before sending the alert if the alert is off
    if (!setNotifier.alerts[issue.type]) { continue; }

    const config = {};
    const notifierDef = Notifier.notifierTypes[setNotifier.type];

    for (const f in notifierDef.fields) {
      const fieldDef = notifierDef.fields[f];
      const field = setNotifier.fields.find(fd => fieldDef.name === fd.name);
      if (!field || (fieldDef.required && !field.value)) {
        // field doesn't exist, or field is required and doesn't have a value
        console.log(`Missing the ${field.name} field for ${n} alerting. Add it on the settings page.`);
        continue;
      }
      config[fieldDef.name] = field.value;
    }

    alerts.push({
      config,
      message,
      notifier: notifierDef,
      cluster: cluster.title
    });
  }
}

// Finds an issue in a cluster
function findIssue (clusterId, issueType, node) {
  for (const issue of issues) {
    if (issue.clusterId === clusterId &&
      issue.type === issueType &&
      issue.node === node) {
      return issue;
    }
  }
}

// Initializes the parliament with ids for each group and cluster
// Upgrades the parliament if necessary
async function initializeParliament () {
  ArkimeUtil.checkArkimeSchemaVersion(User.getClient(), ArkimeConfig.get('usersPrefix'), MIN_DB_VERSION);
  Notifier.initialize({
    issueTypes,
    prefix: ArkimeConfig.get('usersPrefix'),
    esclient: User.getClient()
  });

  Parliament.initialize({
    esclient: User.getClient(),
    name: internals.parliamentName,
    prefix: ArkimeConfig.get('usersPrefix')
  });

  // fetch parliament file if it exists
  try {
    parliamentFile = require(`${ArkimeConfig.get('file')}`);
  } catch (err) {}

  // if there's a parliament file, check that it is the correct version
  if (parliamentFile && (parliamentFile.version === undefined || parliamentFile.version < MIN_PARLIAMENT_VERSION)) {
    console.log( // notify of upgrade
      `WARNING - Current parliament version (${parliamentFile.version ?? 1}) is less then required version (${MIN_PARLIAMENT_VERSION})
        Upgrading your Parliament...\n`
    );

    // do the upgrade
    const upgraded = await upgrade.upgrade(parliamentFile, issues, Parliament);
    parliamentFile = upgraded.parliament;
    issues = upgraded.issues;

    try { // write the upgraded files
      if (Buffer.from(JSON.stringify(parliamentFile, null, 2)).length > 100) {
        fs.writeFileSync(ArkimeConfig.get('file'), JSON.stringify(parliamentFile, null, 2), 'utf8');
      }
      if (!validateIssues()) {
        fs.writeFileSync(app.get('issuesfile'), JSON.stringify(issues, null, 2), 'utf8');
      }
    } catch (e) { // notify of error saving upgraded parliament and exit
      console.log('Error upgrading Parliament:\n\n', ArkimeUtil.sanitizeStr(e.stack));
      if (ArkimeConfig.debug) {
        console.log(parliamentReadError);
      }
      throw new Error(e);
    }

    // notify of upgrade success
    console.log(`SUCCESS - Parliament upgraded to version ${MIN_PARLIAMENT_VERSION}`);
  }

  // create parliament in db if it doesn't exist
  // (there was no parliament file file to upgrade from)
  try {
    await Parliament.getParliament();
  } catch (err) {
    if (err.meta?.statusCode === 404) {
      console.log('Parliament does not exist exist in DB. creating!');
      await Parliament.createParliament({
        groups: [],
        name: internals.parliamentName,
        settings: Parliament.settingsDefault
      });
    } else {
      console.error('ERROR - Error fetching Parliament', err);
    }
  }

  if (parliamentFile && parliamentFile.version < 7) {
    if (!parliamentFile.groups) { parliamentFile.groups = []; }

    // set id for each group/cluster
    for (const group of parliamentFile.groups) {
      group.id = uuid();
      if (group.clusters) {
        for (const cluster of group.clusters) {
          cluster.id = uuid();
        }
      }
    }

    if (!parliamentFile.settings) {
      parliamentFile.settings = Parliament.settingsDefault;
    }
    if (!parliamentFile.settings.general) {
      parliamentFile.settings.general = Parliament.settingsDefault.general;
    }
    if (!parliamentFile.settings.general.outOfDate) {
      parliamentFile.settings.general.outOfDate = Parliament.settingsDefault.general.outOfDate;
    }
    if (!parliamentFile.settings.general.noPackets) {
      parliamentFile.settings.general.noPackets = Parliament.settingsDefault.general.noPackets;
    }
    if (!parliamentFile.settings.general.noPacketsLength) {
      parliamentFile.settings.general.noPacketsLength = Parliament.settingsDefault.general.noPacketsLength;
    }
    if (!parliamentFile.settings.general.esQueryTimeout) {
      parliamentFile.settings.general.esQueryTimeout = Parliament.settingsDefault.general.esQueryTimeout;
    }
    if (!parliamentFile.settings.general.removeIssuesAfter) {
      parliamentFile.settings.general.removeIssuesAfter = Parliament.settingsDefault.general.removeIssuesAfter;
    }
    if (!parliamentFile.settings.general.removeAcknowledgedAfter) {
      parliamentFile.settings.general.removeAcknowledgedAfter = Parliament.settingsDefault.general.removeAcknowledgedAfter;
    }
    if (!parliamentFile.settings.general.hostname) {
      parliamentFile.settings.general.hostname = os.hostname();
    }
  }

  if (ArkimeConfig.debug) {
    console.log('Parliament initialized!');
  }
}

function cleanUpIssues () {
  let issuesRemoved = false;

  let len = issues.length;
  while (len--) {
    const issue = issues[len];
    const timeSinceLastNoticed = Date.now() - issue.lastNoticed || issue.firstNoticed;
    const removeIssuesAfter = Parliament.getGeneralSetting('removeIssuesAfter') * 1000 * 60;
    const removeAcknowledgedAfter = Parliament.getGeneralSetting('removeAcknowledgedAfter') * 1000 * 60;

    if (!issue.ignoreUntil) { // don't clean up any ignored issues, wait for the ignore to expire
      // remove issues that are provisional that haven't been seen since the last cycle
      if (issue.provisional && timeSinceLastNoticed >= 10000) {
        issuesRemoved = true;
        issues.splice(len, 1);
      }

      // remove all issues that have not been seen again for the removeIssuesAfter time, and
      // remove all acknowledged issues that have not been seen again for the removeAcknowledgedAfter time
      if ((!issue.acknowledged && timeSinceLastNoticed > removeIssuesAfter) ||
          (issue.acknowledged && timeSinceLastNoticed > removeAcknowledgedAfter)) {
        issuesRemoved = true;
        issues.splice(len, 1);
      }

      // if the issue was acknowledged but still persists, unacknowledge and alert again
      if (issue.acknowledged && (Date.now() - issue.acknowledged) > removeAcknowledgedAfter) {
        issue.alerted = undefined;
        issue.acknowledged = undefined;
      }
    }
  }

  return issuesRemoved;
}

// Updates an existing issue or pushes a new issue onto the issue array
function setIssue (cluster, newIssue) {
  // build issue
  const issueType = issueTypes[newIssue.type];
  newIssue.text = issueType.text;
  newIssue.title = issueType.name;
  newIssue.severity = issueType.severity;
  newIssue.clusterId = cluster.id;
  newIssue.cluster = cluster.title;
  newIssue.message = formatIssueMessage(cluster, newIssue);
  newIssue.provisional = true;

  let existingIssue = false;

  // don't duplicate existing issues, update them
  for (const issue of issues) {
    if (issue.clusterId === newIssue.clusterId &&
        issue.type === newIssue.type &&
        issue.node === newIssue.node) {
      existingIssue = true;

      // this is at least the second time we've seen this issue
      // so it must be a persistent issue
      issue.provisional = false;

      if (Date.now() > issue.ignoreUntil && issue.ignoreUntil !== -1) {
        // the ignore has expired, so alert!
        issue.ignoreUntil = undefined;
        issue.alerted = undefined;
      }

      issue.lastNoticed = Date.now();

      // if the issue has not been acknowledged, ignored, or alerted, or
      // if the cluster is not a no alert cluster or multiviewer cluster,
      // build and issue an alert
      if (!issue.acknowledged && !issue.ignoreUntil &&
        !issue.alerted && cluster.type !== 'noAlerts' &&
        cluster.type !== 'multiviewer') {
        buildAlert(cluster, issue);
      }
    }
  }

  if (!existingIssue) {
    // this is the first time we've seen this issue
    // don't alert yet, but create the issue
    newIssue.firstNoticed = Date.now();
    newIssue.lastNoticed = Date.now();
    issues.push(newIssue);
  }

  if (ArkimeConfig.debug > 1) {
    console.log('Setting issue:', JSON.stringify(newIssue, null, 2));
  }

  const issuesError = validateIssues();
  if (!issuesError) {
    fs.writeFile(app.get('issuesfile'), JSON.stringify(issues, null, 2), 'utf8',
      (err) => {
        if (err) {
          console.log('Unable to write issue:', err.message ?? err);
        }
      }
    );
  }
}

function getHealth (cluster) {
  Parliament.getGeneralSetting('esQueryTimeout');

  return new Promise((resolve, reject) => {
    const timeout = Parliament.getGeneralSetting('esQueryTimeout') * 1000;

    const options = {
      url: `${cluster.localUrl ?? cluster.url}/eshealth.json`,
      method: 'GET',
      httpsAgent: internals.httpsAgent,
      timeout
    };

    axios(options).then((response) => {
      cluster.healthError = undefined;

      let health;
      try {
        health = response.data;
      } catch (e) {
        cluster.healthError = 'ES health parse failure';
        console.log('Bad response for es health', cluster.localUrl ?? cluster.url);
        return resolve({ cluster });
      }

      if (health) {
        cluster.status = health.status;
        cluster.totalNodes = health.number_of_nodes;
        cluster.dataNodes = health.number_of_data_nodes;

        if (cluster.status === 'red') { // alert on red es status
          setIssue(cluster, { type: 'esRed' });
        }
      }

      return resolve({ cluster });
    }).catch((error) => {
      const message = error.message ?? error;

      setIssue(cluster, { type: 'esDown', value: message });

      cluster.healthError = message;

      if (ArkimeConfig.debug) {
        console.log('HEALTH ERROR:', options.url, message);
      }

      return resolve({ cluster });
    });
  });
}

async function getStats (cluster) {
  await Parliament.getGeneralSetting('esQueryTimeout');

  return new Promise((resolve, reject) => {
    const timeout = Parliament.getGeneralSetting('esQueryTimeout') * 1000;

    const options = {
      url: `${cluster.localUrl ?? cluster.url}/api/parliament`,
      method: 'GET',
      httpsAgent: internals.httpsAgent,
      timeout
    };

    // Get now before the query since we don't know how long query/response will take
    const now = Date.now() / 1000;
    axios(options).then((response) => {
      cluster.statsError = undefined;

      if (response.data.bsqErr) {
        cluster.statsError = response.data.bsqErr;
        console.log('Get stats error', response.data.bsqErr);
        return resolve({ cluster });
      }

      let stats;
      try {
        stats = response.data;
      } catch (e) {
        cluster.statsError = 'ES stats parse failure';
        console.log('Bad response for stats', cluster.localUrl ?? cluster.url);
        return resolve({ cluster });
      }

      if (!stats || !stats.data) { return resolve({ cluster }); }

      cluster.deltaBPS = 0;
      cluster.deltaTDPS = 0;
      cluster.arkimeNodes = 0;
      cluster.monitoring = 0;

      const outOfDate = Parliament.getGeneralSetting('outOfDate');

      for (const stat of stats.data) {
        // sum delta bytes per second
        if (stat.deltaBytesPerSec) {
          cluster.deltaBPS += stat.deltaBytesPerSec;
        }

        // sum delta total dropped per second
        if (stat.deltaTotalDroppedPerSec) {
          cluster.deltaTDPS += stat.deltaTotalDroppedPerSec;
        }

        if (stat.monitoring) {
          cluster.monitoring += stat.monitoring;
        }

        if ((now - stat.currentTime) <= outOfDate && stat.deltaPacketsPerSec > 0) {
          cluster.arkimeNodes++;
        }

        // Look for issues
        if ((now - stat.currentTime) > outOfDate) {
          setIssue(cluster, {
            type: 'outOfDate',
            node: stat.nodeName,
            value: stat.currentTime * 1000
          });
        }

        // look for no packets issue
        if (stat.deltaPacketsPerSec <= Parliament.getGeneralSetting('noPackets')) {
          const id = cluster.title + stat.nodeName;

          // only set the noPackets issue if there is a record of this cluster/node
          // having noPackets and that issue has persisted for the set length of time
          if (noPacketsMap[id] &&
            Date.now() - noPacketsMap[id] >= (Parliament.getGeneralSetting('noPacketsLength') * 1000)) {
            setIssue(cluster, {
              type: 'noPackets',
              node: stat.nodeName,
              value: stat.deltaPacketsPerSec
            });
          } else if (!noPacketsMap[id]) {
            // if this issue has not been encountered yet, make a record of it
            noPacketsMap[id] = Date.now();
          }
        }

        if (stat.deltaESDroppedPerSec > 0) {
          setIssue(cluster, {
            type: 'esDropped',
            node: stat.nodeName,
            value: stat.deltaESDroppedPerSec
          });
        }
      }

      return resolve({ cluster });
    }).catch((error) => {
      const message = error.message ?? error;

      setIssue(cluster, { type: 'esDown', value: message });

      cluster.statsError = message;

      if (ArkimeConfig.debug) {
        console.log('STATS ERROR:', options.url, message);
      }

      return resolve({ cluster });
    });
  });
}

// Chains all promises for requests for health and stats for each cluster in the parliament
// this also sets all the issues in the issues.json file
async function updateParliament () {
  const { body: { _source: parliament } } = await Parliament.getParliament();

  return new Promise((resolve, reject) => {
    const promises = [];
    for (const group of parliament.groups) {
      if (group.clusters) {
        for (const cluster of group.clusters) {
          // only get health for online clusters
          if (cluster.type !== 'disabled') {
            promises.push(getHealth(cluster));
          }
          // don't get stats for multiviewers or offline clusters
          if (cluster.type !== 'multiviewer' && cluster.type !== 'disabled') {
            promises.push(getStats(cluster));
          }
        }
      }
    }

    const issuesRemoved = cleanUpIssues();

    Promise.all(promises).then((results) => {
      if (issuesRemoved) { // save the issues that were removed
        const issuesError = validateIssues();
        if (!issuesError) {
          fs.writeFile(app.get('issuesfile'), JSON.stringify(issues, null, 2), 'utf8',
            (err) => {
              if (err) {
                console.log('Unable to write issue:', err.message ?? err);
              }
            }
          );
        }
      }

      for (const result of results) {
        internals.stats[result.cluster.id] = result.cluster;
      }

      if (ArkimeConfig.debug) {
        console.log('Parliament stats updated!');
        if (issuesRemoved) {
          console.log('Issues updated!');
        }
      }

      return resolve();
    }).catch((error) => {
      console.log('Parliament update error:', error.message ?? error);
      return resolve();
    });
  });
}

function removeIssue (issueType, clusterId, nodeId) {
  let foundIssue = false;
  let len = issues.length;

  while (len--) {
    const issue = issues[len];
    if (issue.clusterId === clusterId &&
      issue.type === issueType &&
      issue.node === nodeId) {
      foundIssue = true;
      issues.splice(len, 1);
      if (issue.type === 'noPackets') {
        // also remove it from the no packets record
        delete noPacketsMap[issue.cluster + nodeId];
      }
    }
  }

  return foundIssue;
}

// Validates that issues exist
// Use this before writing the issues file
function validateIssues (next) {
  const len = Buffer.from(JSON.stringify(issues, null, 2)).length;
  if (len < 2) {
    // if it's an empty file, don't save it, return an error
    const errorMsg = 'Error writing issue data: empty issues';
    console.log(errorMsg);
    if (next) {
      return newError(500, errorMsg);
    }
    return errorMsg;
  }
  return false;
}

// Writes the issues to the issues json file then sends success or error
function writeIssues (req, res, next, successObj, errorText, sendIssues) {
  const issuesError = validateIssues(next);
  if (issuesError) {
    return next(issuesError);
  }

  fs.writeFile(app.get('issuesfile'), JSON.stringify(issues, null, 2), 'utf8',
    (err) => {
      if (ArkimeConfig.debug) {
        console.log('Wrote issues file', err ?? '');
      }

      if (err) {
        const errorMsg = `Unable to write issue data: ${err.message ?? err}`;
        console.log(errorMsg);
        return res.serverError(500, errorMsg);
      }

      // send the updated issues with the response
      if (sendIssues && successObj.issues) {
        successObj.issues = issues;
      }

      return res.json(successObj);
    }
  );
}

// ----------------------------------------------------------------------------
// APIS
// ----------------------------------------------------------------------------
if (ArkimeConfig.regressionTests) {
  app.get('/parliament/api/regressionTests/makeToken', (req, res, next) => {
    req.user = {
      userId: req.query.arkimeRegressionUser ?? 'anonymous'
    };
    setCookie(req, res, next);
    return res.end();
  });
}

// Get whether authentication is set
app.get('/parliament/api/auth', setCookie, (req, res, next) => {
  return res.json({
    isUser: req.user.hasRole('parliamentUser'),
    isAdmin: req.user.hasRole('parliamentAdmin')
  });
});

// Update the parliament general settings object
app.put('/parliament/api/settings', [isAdmin, checkCookieToken], Parliament.apiUpdateSettings);

// Update the parliament general settings object to the defaults
app.put('/parliament/api/settings/restoreDefaults', [isAdmin, checkCookieToken], Parliament.apiRestoreDefaultSettings);

// user roles endpoint
app.get('/parliament/api/user/roles', [ArkimeUtil.noCacheJson, checkCookieToken], User.apiRoles);

// fetch notifier types endpoint
app.get('/parliament/api/notifierTypes', [ArkimeUtil.noCacheJson, isAdmin, setCookie], Notifier.apiGetNotifierTypes);

// fetch configured notifiers endpoint
app.get('/parliament/api/notifiers', [ArkimeUtil.noCacheJson, isAdmin, checkCookieToken], Notifier.apiGetNotifiers);

// Create a new notifier endpoint
app.post('/parliament/api/notifier', [ArkimeUtil.noCacheJson, isAdmin, checkCookieToken], Notifier.apiCreateNotifier);

// Update an existing notifier endpoint
app.put('/parliament/api/notifier/:id', [ArkimeUtil.noCacheJson, isAdmin, checkCookieToken], Notifier.apiUpdateNotifier);

// Remove a notifier endpoint
app.delete('/parliament/api/notifier/:id', [ArkimeUtil.noCacheJson, isAdmin, checkCookieToken], Notifier.apiDeleteNotifier);

// issue a test alert to a specified notifier
app.post('/parliament/api/notifier/:id/test', [ArkimeUtil.noCacheJson, isAdmin, checkCookieToken], Notifier.apiTestNotifier);

// fetch the parliament object
app.get('/parliament/api/parliament', Parliament.apiGetParliament);

// get the parliament stats (updated every 10 seconds by updateParliament)
app.get('/parliament/api/parliament/stats', (req, res) => { return res.json({ results: internals.stats }); });

// updates the parliament order of groups or clusters
app.put('/parliament/api/parliament/order', [isAdmin, checkCookieToken], Parliament.apiUpdateParliamentOrder);

// Create a new group in the parliament
app.post('/parliament/api/groups', [isAdmin, checkCookieToken], Parliament.apiCreateGroup);

// Delete a group in the parliament
app.delete('/parliament/api/groups/:id', [isAdmin, checkCookieToken], Parliament.apiDeleteGroup);

// Update a group in the parliament
app.put('/parliament/api/groups/:id', [isAdmin, checkCookieToken], Parliament.apiUpdateGroup);

// Create a new cluster within an existing group
app.post('/parliament/api/groups/:id/clusters', [isAdmin, checkCookieToken], Parliament.apiCreateCluster);

// Delete a cluster
app.delete('/parliament/api/groups/:groupId/clusters/:clusterId', [isAdmin, checkCookieToken], Parliament.apiDeleteCluster);

// Update a cluster
app.put('/parliament/api/groups/:groupId/clusters/:clusterId', [isAdmin, checkCookieToken], Parliament.apiUpdateCluster);

// Get a list of issues
app.get('/parliament/api/issues', (req, res, next) => {
  let issuesClone = JSON.parse(JSON.stringify(issues));

  if (req.query.map) {
    const results = {};
    for (const issue of issuesClone) {
      if (!issue.acknowledged && !issue.ignoreUntil && !issue.provisional) {
        if (!results[issue.clusterId]) {
          results[issue.clusterId] = [];
        }
        results[issue.clusterId].push(issue);
      }
    }
    return res.json({ results });
  }

  issuesClone = issuesClone.filter((issue) => {
    // always filter out provisional issues
    if (issue.provisional) { return false; }

    // filter ack'd issues
    if (req.query.hideAckd && req.query.hideAckd === 'true' && issue.acknowledged) {
      return false;
    }

    // filter ignored issues
    if (req.query.hideIgnored && req.query.hideIgnored === 'true' && issue.ignoreUntil) {
      return false;
    }

    // filter issues by type
    if (req.query.hideEsRed && req.query.hideEsRed === 'true' && issue.type === 'esRed') {
      return false;
    }
    if (req.query.hideEsDown && req.query.hideEsDown === 'true' && issue.type === 'esDown') {
      return false;
    }
    if (req.query.hideEsDropped && req.query.hideEsDropped === 'true' && issue.type === 'esDropped') {
      return false;
    }
    if (req.query.hideOutOfDate && req.query.hideOutOfDate === 'true' && issue.type === 'outOfDate') {
      return false;
    }
    if (req.query.hideNoPackets && req.query.hideNoPackets === 'true' && issue.type === 'noPackets') {
      return false;
    }

    // filter by search term
    if (req.query.filter) {
      const searchTerm = req.query.filter.toLowerCase();
      return issue.severity.toLowerCase().includes(searchTerm) ||
          (issue.node && issue.node.toLowerCase().includes(searchTerm)) ||
          issue.cluster.toLowerCase().includes(searchTerm) ||
          issue.message.toLowerCase().includes(searchTerm) ||
          issue.title.toLowerCase().includes(searchTerm) ||
          issue.text.toLowerCase().includes(searchTerm);
    }

    // we got past all the filters! include this issue in the results
    return true;
  });

  let type = 'string';
  const sortBy = req.query.sort;
  if (sortBy === 'ignoreUntil' ||
    sortBy === 'firstNoticed' ||
    sortBy === 'lastNoticed' ||
    sortBy === 'acknowledged') {
    type = 'number';
  }

  if (sortBy) {
    const order = req.query.order ?? 'desc';
    issuesClone.sort((a, b) => {
      if (type === 'number') {
        let aVal = 0;
        let bVal = 0;

        if (b[sortBy] !== undefined) { bVal = b[sortBy]; }
        if (a[sortBy] !== undefined) { aVal = a[sortBy]; }

        return order === 'asc' ? bVal - aVal : aVal - bVal;
      } else { // assume it's a string
        let aVal = '';
        let bVal = '';

        if (b[sortBy] !== undefined) { bVal = b[sortBy]; }
        if (a[sortBy] !== undefined) { aVal = a[sortBy]; }

        return order === 'asc' ? bVal.localeCompare(aVal) : aVal.localeCompare(bVal);
      }
    });
  }

  const recordsFiltered = issuesClone.length;

  if (req.query.length && !isNaN(req.query.length)) { // paging
    const len = parseInt(req.query.length);
    const start = !req.query.start ? 0 : parseInt(req.query.start);

    issuesClone = issuesClone.slice(start, len + start);
  }

  return res.json({
    issues: issuesClone,
    recordsFiltered
  });
});

// acknowledge one or more issues
app.put('/parliament/api/acknowledgeIssues', [isUser, checkCookieToken], (req, res, next) => {
  if (!Array.isArray(req.body.issues) || !req.body.issues.length) {
    return res.serverError(422, 'Must specify the issue(s) to acknowledge.');
  }

  const now = Date.now();
  let count = 0;

  for (const i of req.body.issues) {
    const issue = findIssue(i.clusterId, i.type, i.node);
    if (issue) {
      issue.acknowledged = now;
      count++;
    }
  }

  let errorText;
  if (!count) {
    errorText = 'Unable to acknowledge requested issue';
    if (req.body.issues.length > 1) { errorText += 's'; }
    return res.serverError(500, errorText);
  }

  let successText = `Successfully acknowledged ${count} requested issue`;
  errorText = 'Unable to acknowledge the requested issue';
  if (count > 1) {
    successText += 's';
    errorText += 's';
  }

  const successObj = { success: true, text: successText, acknowledged: now };
  writeIssues(req, res, next, successObj, errorText);
});

// ignore one or more issues
app.put('/parliament/api/ignoreIssues', [isUser, checkCookieToken], (req, res, next) => {
  if (!Array.isArray(req.body.issues) || !req.body.issues.length) {
    const message = 'Must specify the issue(s) to ignore.';
    return res.serverError(422, message);
  }

  const ms = req.body.ms ?? 3600000; // Default to 1 hour
  let ignoreUntil = Date.now() + ms;
  if (ms === -1) { ignoreUntil = -1; } // -1 means ignore it forever

  let count = 0;

  for (const i of req.body.issues) {
    const issue = findIssue(i.clusterId, i.type, i.node);
    if (issue) {
      issue.ignoreUntil = ignoreUntil;
      count++;
    }
  }

  let errorText;
  if (!count) {
    errorText = 'Unable to ignore requested issue';
    if (req.body.issues.length > 1) { errorText += 's'; }
    return res.serverError(500, errorText);
  }

  let successText = `Successfully ignored ${count} requested issue`;
  errorText = 'Unable to ignore the requested issue';
  if (count > 1) {
    successText += 's';
    errorText += 's';
  }

  const successObj = { success: true, text: successText, ignoreUntil };
  writeIssues(req, res, next, successObj, errorText);
});

// unignore one or more issues
app.put('/parliament/api/removeIgnoreIssues', [isUser, checkCookieToken], (req, res, next) => {
  if (!Array.isArray(req.body.issues) || !req.body.issues.length) {
    const message = 'Must specify the issue(s) to unignore.';
    return res.serverError(422, message);
  }

  let count = 0;

  for (const i of req.body.issues) {
    const issue = findIssue(i.clusterId, i.type, i.node);
    if (issue) {
      issue.ignoreUntil = undefined;
      issue.alerted = undefined; // reset alert time so it can alert again
      count++;
    }
  }

  let errorText;
  if (!count) {
    errorText = 'Unable to unignore requested issue';
    if (req.body.issues.length > 1) { errorText += 's'; }
    return res.serverError(500, errorText);
  }

  let successText = `Successfully unignored ${count} requested issue`;
  errorText = 'Unable to unignore the requested issue';
  if (count > 1) {
    successText += 's';
    errorText += 's';
  }

  const successObj = { success: true, text: successText };
  writeIssues(req, res, next, successObj, errorText);
});

// Remove an issue with a cluster
app.put('/parliament/api/groups/:groupId/clusters/:clusterId/removeIssue', [isUser, checkCookieToken], (req, res, next) => {
  if (!ArkimeUtil.isString(req.body.type)) {
    const message = 'Must specify the issue type to remove.';
    return res.serverError(422, message);
  }

  const foundIssue = removeIssue(req.body.type, req.params.clusterId, req.body.node);

  if (!foundIssue) {
    return res.serverError(500, 'Unable to find issue to remove. Maybe it was already removed.');
  }

  const successObj = { success: true, text: 'Successfully removed the requested issue.' };
  const errorText = 'Unable to remove that issue.';
  writeIssues(req, res, next, successObj, errorText);
});

// Remove all acknowledged all issues
app.put('/parliament/api/issues/removeAllAcknowledgedIssues', [isUser, checkCookieToken], (req, res, next) => {
  let count = 0;

  let len = issues.length;
  while (len--) {
    const issue = issues[len];
    if (issue.acknowledged) {
      count++;
      issues.splice(len, 1);
    }
  }

  if (!count) {
    return res.serverError(400, 'There are no acknowledged issues to remove.');
  }

  const successObj = { success: true, text: `Successfully removed ${count} acknowledged issues.` };
  const errorText = 'Unable to remove acknowledged issues.';
  writeIssues(req, res, next, successObj, errorText, true);
});

// remove one or more acknowledged issues
app.put('/parliament/api/removeSelectedAcknowledgedIssues', [isUser, checkCookieToken], (req, res, next) => {
  if (!Array.isArray(req.body.issues) || !req.body.issues.length) {
    const message = 'Must specify the acknowledged issue(s) to remove.';
    return res.serverError(422, message);
  }

  let count = 0;

  // mark issues to remove
  for (const i of req.body.issues) {
    const issue = findIssue(i.clusterId, i.type, i.node);
    if (issue && issue.acknowledged) {
      count++;
      issue.remove = true;
    }
  }

  if (!count) {
    return res.serverError(400, 'There are no acknowledged issues to remove.');
  }

  count = 0;
  let len = issues.length;
  while (len--) {
    const issue = issues[len];
    if (issue.remove) {
      count++;
      issues.splice(len, 1);
    }
  }

  let errorText;
  if (!count) {
    errorText = 'Unable to remove requested issue';
    if (req.body.issues.length > 1) { errorText += 's'; }
    return res.serverError(500, errorText);
  }

  let successText = `Successfully removed ${count} requested issue`;
  errorText = 'Unable to remove the requested issue';
  if (count > 1) {
    successText += 's';
    errorText += 's';
  }

  const successObj = { success: true, text: successText };
  writeIssues(req, res, next, successObj, errorText);
});

// ----------------------------------------------------------------------------
// INITIALIZE
// ----------------------------------------------------------------------------
async function setupAuth () {
  Auth.initialize({
    appAdminRole: 'parliamentAdmin',
    passwordSecretSection: 'parliament'
  });

  User.initialize({
    insecure: ArkimeConfig.insecure,
    node: ArkimeConfig.get('usersElasticsearch', 'http://localhost:9200'),
    prefix: ArkimeConfig.get('usersPrefix', ArkimeConfig.get('prefix', 'arkime')),
    apiKey: ArkimeConfig.get('usersElasticsearchAPIKey'),
    basicAuth: ArkimeConfig.get('usersElasticsearchBasicAuth')
  });
}

/* SIGNALS! ----------------------------------------------------------------- */
// Explicit sigint handler for running under docker
// See https://github.com/nodejs/node/issues/4182
process.on('SIGINT', function () {
  process.exit();
});

/* LISTEN! ----------------------------------------------------------------- */
// using fallthrough: false because there is no 404 endpoint (client router
// handles 404s) and sending index.html is confusing
// expose vue bundles (prod)
app.use(['/static', '/parliament/static'], express.static(
  path.join(__dirname, '/vueapp/dist/static'),
  { maxAge: dayMs, fallthrough: false }
), ArkimeUtil.missingResource);
// expose vue bundle (dev)
app.use(['/app.js', '/parliament/app.js'], express.static(
  path.join(__dirname, '/vueapp/dist/app.js'),
  { fallthrough: false }
), ArkimeUtil.missingResource);
app.use(['/app.js.map', '/parliament/app.js.map'], express.static(
  path.join(__dirname, '/vueapp/dist/app.js.map'),
  { fallthrough: false }
), ArkimeUtil.missingResource);

// vue index page
const Vue = require('vue');
const vueServerRenderer = require('vue-server-renderer');

// Factory function to create fresh Vue apps
function createApp () {
  return new Vue({
    template: '<div id="app"></div>'
  });
}

app.use((req, res, next) => {
  const renderer = vueServerRenderer.createRenderer({
    template: fs.readFileSync(path.join(__dirname, '/vueapp/dist/index.html'), 'utf-8')
  });

  const appContext = {
    authMode: Auth.mode,
    nonce: res.locals.nonce,
    version: version.version,
    path: ArkimeConfig.get('webBasePath', '/')
  };

  // Create a fresh Vue app instance
  const vueApp = createApp();

  // Render the Vue instance to HTML
  renderer.renderToString(vueApp, appContext, (err, html) => {
    if (err) {
      console.log('ERROR - fetching vue index page:', err);
      if (err.code === 404) {
        res.status(404).end('Page not found');
      } else {
        res.status(500).end('Internal Server Error');
      }
      return;
    }

    res.send(html);
  });
});

// ----------------------------------------------------------------------------
// MAIN
// ----------------------------------------------------------------------------
async function main () {
  try {
    await ArkimeConfig.initialize({
      defaultConfigFile: `${version.config_prefix}/etc/parliament.ini`,
      defaultSections: 'parliament'
    });

    internals.webBasePath = ArkimeConfig.get('webBasePath', '/');
  } catch (err) {
    console.log(err);
    process.exit();
  }

  // ERROR OUT if there's no parliament config
  if (!ArkimeConfig.getSection('parliament')) {
    console.error('ERROR - No parliament config file. Please create a parliament config file. See https://arkime.com/settings#parliament\nExiting.\n');
    process.exit(1);
  }

  // construct the issues file name
  let issuesFilename = 'issues.json';
  if (ArkimeConfig.get('file').indexOf('.json') > -1) {
    const filename = ArkimeConfig.get('file').replace(/\.json/g, '');
    issuesFilename = `${filename}.issues.json`;
  }
  app.set('issuesfile', issuesFilename);

  // get the issues file or create it if it doesn't exist
  try {
    issues = require(issuesFilename);
  } catch (err) {
    issues = [];
  }

  await setupAuth();

  const parliamentHost = ArkimeConfig.get('parliamentHost');
  if (Auth.mode === 'header' && parliamentHost !== 'localhost' && parliamentHost !== '127.0.0.1') {
    console.log('SECURITY WARNING - When using header auth, parliamentHost should be localhost or use iptables');
  }

  ArkimeUtil.createHttpServer(app, parliamentHost, ArkimeConfig.get('port', 8008), async () => {
    if (ArkimeConfig.debug) {
      console.log('Parliament file:', ArkimeConfig.get('file'));
      console.log('Issues file:', issuesFilename);
    }

    try {
      await initializeParliament();
    } catch (err) {
      console.log('ERROR - never mind, couldn\'t initialize Parliament\n', err);
      process.exit(1);
    }

    setInterval(() => {
      updateParliament();
      processAlerts();
    }, 10000);
  });
}

main();
