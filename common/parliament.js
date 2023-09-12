'use strict';

const LRU = require('lru-cache');
const ArkimeConfig = require('../common/arkimeConfig');

class Parliament {
  static #name;
  static #debug;
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
    Parliament.#name = options.name;
    Parliament.#debug = options.debug ?? 0;
    Parliament.#esclient = options.esclient;

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

    Parliament.#parliamentIndex = `${prefix}parliament`;
  }

  // --------------------------------------------------------------------------
  // DB INTERACTIONS
  // --------------------------------------------------------------------------
  static async getParliament () {
    return Parliament.#esclient.get({
      index: Parliament.#parliamentIndex, id: Parliament.#name
    });
  }

  static async searchParliaments (query) {
    return Parliament.#esclient.search({
      index: Parliament.#parliamentIndex, body: query, rest_total_hits_as_int: true
    });
  }

  static async createParliament (parliament) {
    return Parliament.#esclient.create({
      index: Parliament.#parliamentIndex, body: parliament, id: Parliament.#name, timeout: '10m'
    });
  }

  static async setParliament (parliament) {
    return Parliament.#esclient.index({
      index: Parliament.#parliamentIndex, body: parliament, id: Parliament.#name, refresh: true, timeout: '10m'
    });
  }

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
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

      if (!req.user.hasRole('parliamentAdmin')) {
        delete parliament.settings;
      }

      return res.json(parliament);
    } catch (err) {
      if (ArkimeConfig.debug) {
        console.log('Error fetching parliament', err);
      }
      return res.serverError(500, 'Error fetching parliament');
    }
  }

  /**
   * PUT - /api/parliament
   *
   * Updates a parliament by id (name).
   * @name /parliament
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {Parliament} parliament - The updated parliament.
   */
  static async apiUpdateParliament (req, res) {
    try {
      await Parliament.setParliament(req.body);
      return res.json({ success: true, text: 'Parliament updated successfully', parliament: req.body });
    } catch (err) {
      if (ArkimeConfig.debug) {
        console.log('Error updating parliament', err);
      }
      return res.serverError(500, 'Error updating parliament');
    }
  }

  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------
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

    let val = Parliament.settingsDefault.general[type];
    if (parliament?.settings?.general[type]) {
      val = parliament.settings.general[type];
    }

    return val;
  }
}

module.exports = Parliament;
