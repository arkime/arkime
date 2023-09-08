'use strict';

class Parliament {
  static #debug;
  static #esclient;
  static #parliamentIndex;

  static initialize (options) {
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
  static async getParliament (id) {
    return Parliament.#esclient.get({
      index: Parliament.#parliamentIndex, id
    });
  }

  static async searchParliaments (query) {
    return Parliament.#esclient.search({
      index: Parliament.#parliamentIndex, body: query, rest_total_hits_as_int: true
    });
  }

  static async createParliament (parliament) {
    return Parliament.#esclient.create({
      index: Parliament.#parliamentIndex, body: parliament, id: parliament.name, timeout: '10m'
    });
  }

  static async deleteParliament (id) {
    return Parliament.#esclient.delete({
      index: Parliament.#parliamentIndex, id, refresh: true, timeout: '10m'
    });
  }

  static async setParliament (id, parliament) {
    return Parliament.#esclient.index({
      index: Parliament.#parliamentIndex, body: parliament, id, refresh: true, timeout: '10m'
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
   * @property {string} type - The type of the Cluster.
   * @property {string} healthError - The healthError of the Cluster.
   */

  /**
   * GET - /api/parliament/:id
   *
   * Retrieves a parliament by id (name).
   * @name /parliament/:id
   * @returns {Parliament} parliament - The requested parliament
   */
  static async apiGetParliament (req, res) {
    return await Parliament.getParliament(req.params.id);
  }

  /**
   * POST - /api/parliament/:id
   *
   * Creates a parliament by id (name).
   * @name /parliament/:id
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async apiCreateParliament (req, res) {
    // TODO validate parliament
    return await Parliament.createParliament(req.body);
  }

  /**
   * PUT - /api/parliament/:id
   *
   * Updates a parliament by id (name).
   * @name /parliament/:id
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {Parliament} parliament - The updated parliament.
   */
  static async apiUpdateParliament (req, res) {
    // TODO VALIDATE PARLIAMENT
    return await Parliament.setParliament(req.params.id, req.body);
  }
}

module.exports = Parliament;
