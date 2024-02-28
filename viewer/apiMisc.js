/******************************************************************************/
/* apiMisc.js -- api calls for misc parts of viewer
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const Config = require('./config.js');
const Db = require('./db.js');
const dns = require('dns');
const fs = require('fs');
const unzipper = require('unzipper');
const util = require('util');
const ArkimeUtil = require('../common/arkimeUtil');
const ViewerUtils = require('./viewerUtils');
const ArkimeConfig = require('../common/arkimeConfig');
const User = require('../common/user');
const View = require('./apiViews');
const internals = require('./internals');
const UserAPIs = require('./apiUsers');
const SessionAPIs = require('./apiSessions');

class MiscAPIs {
  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------
  static async #getClusters () {
    const clusters = { active: [], inactive: [] };
    if (Config.get('multiES', false)) {
      try {
        const { body: results } = await Db.getClusterDetails();
        clusters.active = results.active;
        clusters.inactive = results.inactive;
        return clusters;
      } catch (err) {
        console.log('ERROR - getClusters', util.inspect(err, false, 50));
        return clusters;
      }
    } else {
      return clusters;
    }
  }

  // --------------------------------------------------------------------------
  static #remoteClusters () {
    function cloneClusters (clusters) {
      const clone = {};

      for (const key in clusters) {
        if (clusters[key]) {
          const cluster = clusters[key];
          clone[key] = {
            name: cluster.name,
            url: cluster.url
          };
        }
      }

      return clone;
    }

    if (!internals.remoteClusters) {
      return {};
    }

    return cloneClusters(internals.remoteClusters);
  }

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  // field apis ---------------------------------------------------------------
  /**
   * GET - /api/fields
   *
   * Gets available database field objects pertaining to sessions.
   * @name /fields
   * @param {boolean} array=false Whether to return an array of fields, otherwise returns a map
   * @returns {array/map} The map or list of database fields
   */
  static getFields (req, res) {
    if (!internals.fieldsMap) {
      res.status(404);
      res.send('Cannot locate fields');
    }

    if (req.query && req.query.array) {
      res.send(internals.fieldsArr);
    } else {
      res.send(internals.fieldsMap);
    }
  };

  // file apis ----------------------------------------------------------------
  /**
   * GET - /api/files
   *
   * Gets a list of PCAP files that Arkime knows about.
   * @name /files
   * @param {number} length=100 - The number of items to return. Defaults to 500, Max is 10,000
   * @param {number} start=0 - The entry to start at. Defaults to 0
   * @returns {Array} data - The list of files
   * @returns {number} recordsTotal - The total number of files Arkime knows about
   * @returns {number} recordsFiltered - The number of files returned in this result
   */
  static getFiles (req, res) {
    const columns = ['num', 'node', 'name', 'locked', 'first', 'filesize', 'encoding', 'packetPosEncoding', 'packets', 'packetsSize', 'uncompressedBits', 'compression'];

    const query = {
      _source: columns,
      from: +req.query.start || 0,
      size: +req.query.length || 10,
      sort: {}
    };

    query.sort[req.query.sortField || 'num'] = {
      order: req.query.desc === 'true' ? 'desc' : 'asc'
    };

    if (req.query.filter) {
      query.query = { wildcard: { name: `*${req.query.filter}*` } };
    }

    ViewerUtils.addCluster(req.query.cluster, query);

    Promise.all([
      Db.search('files', 'file', query),
      Db.numberOfDocuments('files', { cluster: req.query.cluster })
    ]).then(([files, total]) => {
      if (files.error) { throw files.error; }

      const results = { total: files.hits.total, results: [] };
      for (const file of files.hits.hits) {
        const fields = file._source || files.fields;
        if (fields.locked === undefined) {
          fields.locked = 0;
        }
        fields.cratio = fields.packetsSize ? Math.round(100 - (100 * fields.filesize / fields.packetsSize)) : 0;
        fields.id = fields._id;
        results.results.push(fields);
      }

      const r = {
        recordsTotal: total.count,
        recordsFiltered: results.total,
        data: results.results
      };

      res.logCounts(r.data.length, r.recordsFiltered, r.total);
      res.send(r);
    }).catch((err) => {
      console.log(`ERROR - ${req.method} /api/files`, util.inspect(err, false, 50));
      return res.send({ recordsTotal: 0, recordsFiltered: 0, data: [] });
    });
  };

  // --------------------------------------------------------------------------
  /**
   * GET - /api/:nodeName/:fileNum/filesize
   *
   * Retrieves the filesize of a PCAP file.
   * @name /:nodeName/:fileNum/filesize
   * @returns {number} filesize - The size of the file (-1 if the file cannot be found).
   */
  static getFileSize (req, res) {
    Db.fileIdToFile(req.params.nodeName, req.params.fileNum, (file) => {
      if (!file) {
        return res.send({ filesize: -1 });
      }

      fs.stat(file.name, (err, stats) => {
        if (err || !stats) {
          return res.send({ filesize: -1 });
        } else {
          return res.send({ filesize: stats.size });
        }
      });
    });
  };

  // value actions apis -------------------------------------------------------
  /**
   * GET - /api/valueactions
   *
   * Retrives the actions that can be preformed on meta data values.
   * @name /valueactions
   * @returns {object} - The list of actions that can be preformed on data values.
   */
  static getValueActions (req, res) {
    if (!req.user || !req.user.userId) {
      return res.send({});
    }

    const actions = {};

    actions.httpAuthorizationDecode = {
      fields: 'http.authorization', func: `{
      if (value.substring(0,5) === "Basic")
        return {name: "Decoded:", value: atob(value.substring(6))};
      return undefined;
    }`
    };
    actions.reverseDNS = { category: 'ip', name: 'Get Reverse DNS', url: 'api/reversedns?ip=%TEXT%', actionType: 'fetch' };
    actions.bodyHashMd5 = { category: 'md5', url: 'api/session/%NODE%/%ID%/bodyHash/%TEXT%', name: 'Download File' };
    actions.bodyHashSha256 = { category: 'sha256', url: 'api/session/%NODE%/%ID%/bodyHash/%TEXT%', name: 'Download File' };

    for (const key in internals.rightClicks) {
      const rc = internals.rightClicks[key];
      // If we are one of the notUsers, then we don't get the action
      if (rc.notUsers && rc.notUsers[req.user.userId]) {
        continue;
      }

      // If we are one of the users that can see the action, add to our list and remove users so we don't leak
      if (!rc.users || rc.users[req.user.userId]) {
        actions[key] = JSON.parse(JSON.stringify(rc));
        delete actions[key].users;
        delete actions[key].notUsers;
      }
    }

    return res.send(actions);
  };

  // --------------------------------------------------------------------------
  /**
   * GET - /api/fieldactions
   *
   * Retrives the actions that can be preformed on fields.
   * @name /fieldactions
   * @returns {object} - The list of actions that can be preformed on fields.
   */
  static getFieldActions (req, res) {
    if (!req.user || !req.user.userId) {
      return res.send({});
    }

    const actions = {};

    for (const key in internals.fieldActions) {
      const action = internals.fieldActions[key];
      // If we are one of the notUsers, then we don't get the action
      if (action.notUsers && action.notUsers[req.user.userId]) {
        continue;
      }

      // If we are one of the users that can see the action, add to our list and remove users so we don't leak
      if (!action.users || action.users[req.user.userId]) {
        actions[key] = JSON.parse(JSON.stringify(action));
        delete actions[key].users;
        delete actions[key].notUsers;
      }
    }

    return res.send(actions);
  };

  // reverse dns apis ---------------------------------------------------------
  /**
   * GET - /api/reversedns
   *
   * Retrives the domain names associated with an IP address.
   * @name /reversedns
   * @param {string} ip - The IP to search domain names for.
   * @returns {string} domains - A comma separated string list of all the matching domain names.
   */
  static getReverseDNS (req, res) {
    dns.reverse(req.query.ip, (err, data) => {
      if (err) {
        return res.send('reverse error');
      }
      return res.send(data.join(', '));
    });
  };

  // upload apis --------------------------------------------------------------
  /**
   * POST - /api/upload
   *
   * Uploads PCAP files to Arkime. This API is really only useful for demo sites and very special cases.
   * Instead you almost always should just run capture locally, which will be much more
   * efficient and not duplicate the PCAP. See https://arkime.com/faq#how-do-i-import-existing-pcaps
   * @name /upload
   * @param {string} tags - A comma separated list of tags to add to each session created.
   */
  static upload (req, res) {
    const exec = require('child_process').exec;
    const uploadCommand = Config.get('uploadCommand');

    if (!uploadCommand) {
      const msg = 'Need to set https://arkime.com/settings#uploadcommand in config file for uploads to work. However if you are trying to import pcap files from the command line, just use capture instead, https://arkime.com/faq#how-do-i-import-existing-pcaps';
      res.status(500);
      res.end(msg);
      console.log('ERROR -', msg);
      return;
    }

    if (!req.user.hasRole(internals.uploadRoles)) {
      res.status(403);
      return res.end('Not covered by role');
    }

    if (req.file === undefined) {
      res.status(403);
      return res.end('Missing file');
    }

    let tags = '';
    if (ArkimeUtil.isString(req.body.tags)) {
      const t = req.body.tags.replace(/[^-a-zA-Z0-9_:,]/g, '').split(',');
      t.forEach((tag) => {
        if (tag.length > 0) {
          tags += ' --tag ' + tag;
        }
      });
    }

    const cmd = uploadCommand
      .replace(/{TAGS}/g, tags)
      .replace(/{NODE}/g, Config.nodeName())
      .replace(/{TMPFILE}/g, req.file.path)
      .replace(/{INSECURE-ORIGINALNAME}/g, req.file.originalname)
      .replace(/{CONFIG}/g, ArkimeConfig.configFile);

    console.log('upload command: ', cmd);
    exec(cmd, (error, stdout, stderr) => {
      if (error !== null) {
        console.log(`ERROR - ${req.method} /api/upload`, util.inspect(error, false, 50));
        res.status(500);
        res.write('<b>Upload command failed:</b><br>');
      }
      res.write(ArkimeUtil.safeStr(cmd));
      res.write('<br>');
      res.write('<pre>');
      res.write(stdout);
      res.end('</pre>');
      fs.unlinkSync(req.file.path);
    });
  };

  // cluster apis -------------------------------------------------------------
  /**
   * GET - /api/clusters
   *
   * Retrieves a list of known configured Arkime clusters (if in
   * <a href="https://arkime.com/settings#multi-viewer-settings">Mulit Viewer mode</a>).
   * @name /clusters
   * @returns {Array} active - The active Arkime clusters.
   * @returns {Array} inactive - The inactive Arkime clusters.
   */
  static async getClusters (req, res) {
    const clusters = await MiscAPIs.#getClusters();
    res.send(clusters);
  };

  /**
   * GET - /api/remoteclusters
   *
   * Retrieves a list of known configured remote Arkime clusters.
   * @name /remoteclusters
   * @returns {Object} remoteclusters - Key/value pairs of remote Arkime clusters, the key being the name of the cluster
   */
  static getRemoteClusters (req, res) {
    const clusters = MiscAPIs.#remoteClusters();

    if (!Object.keys(clusters).length) {
      res.status(404);
      return res.send('Cannot locate remote clusters');
    }

    return res.send(clusters);
  };

  // app info apis ------------------------------------------------------------
  /**
   * GET - /api/appinfo
   *
   * Retrieves information that the app uses on every page:
   * eshealth, currentuser, views, remoteclusters, clusters, fields, fieldsmap, fieldshistory
   * @name /appinfo
   * @returns {ESHealth} eshealth - The OpenSearch/Elasticsearch cluster health status and information.
   * @returns {ArkimeUser} currentuser - The currently logged in user
   * @returns {ArkimeView[]} views - A list of views accessible to the logged in user
   * @returns {Object} remoteclusters - A list of known remote Arkime clusters
   * @returns {Array} clusters - A list of known configured Arkime clusters (if in Mulit Viewer mode)
   * @returns {Array} fields - Available database field objects pertaining to sessions
   * @returns {Array} fieldsmap - Available database field objects pertaining to sessions
   * @returns {Object} fieldshistory - The user's field history for the search expression input
   */
  static async getAppInfo (req, res) {
    try {
      let esHealth, esHealthError;
      try { // deal with es health errors
        esHealth = await Db.healthCache();
      } catch (err) {
        esHealthError = err.toString();
      }

      // these always returns something and never return an error
      const clusters = await MiscAPIs.#getClusters(); // { active: [], inactive: [] }
      const remoteclusters = MiscAPIs.#remoteClusters(); // {}
      const fieldhistory = UserAPIs.findUserState('fieldHistory', req.user); // {}
      const { data: views } = await View.getViews(req);
      const roles = await User.getRoles();

      // can't fetch user or fields is FATAL, so let it fall through to outer
      // catch and send an error to the client
      const user = await User.getCurrentUser(req);
      const fieldsArr = internals.fieldsArr;
      const fieldsMap = JSON.parse(internals.fieldsMap);
      const userSettingDefaults = internals.settingDefaults;

      return res.send({
        esHealth,
        esHealthError,
        views,
        fieldhistory,
        remoteclusters,
        clusters,
        user,
        fieldsArr,
        fieldsMap,
        roles,
        userSettingDefaults
      });
    } catch (err) {
      console.log('ERROR - /api/appinfo', err);
      return res.serverError(500, err.toString());
    }
  };

  // cyberchef apis -----------------------------------------------------------
  /**
   * @ignore
   *
   * Retrieves the source or destination packets for a session for CyberChef.
   * @name /cyberchef/:nodeName/session/:id
   * @param {string} type=src - Whether to send the source (src) or destination (dst) packets.
   */
  static cyberChef (req, res) {
    SessionAPIs.processSessionIdAndDecode(req.params.id, 10000, (err, session, results) => {
      if (err) {
        console.log(`ERROR - ${req.method} /%s/session/%s/cyberchef`, ArkimeUtil.sanitizeStr(req.params.nodeName), ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
        return res.end('Error - ' + err);
      }

      let data = '';
      for (let i = (req.query.type !== 'dst' ? 0 : 1), ilen = results.length; i < ilen; i += 2) {
        data += results[i].data.toString('hex');
      }

      res.send({ data });
    });
  };

  // --------------------------------------------------------------------------
  /**
   * @ignore
   *
   * Loads the CyberChef UI.
   * @name /cyberchef
   */
  static getCyberChefUI (req, res) {
    let found = false;
    let path = req.path.substring(1);

    if (req.baseUrl === '/modules') {
      res.setHeader('Content-Type', 'application/javascript; charset=UTF-8');
      path = 'modules/' + path;
    }

    if (path === '') {
      path = `CyberChef_v${internals.CYBERCHEFVERSION}.html`;
    }

    if (path === 'assets/main.js') {
      res.setHeader('Content-Type', 'application/javascript; charset=UTF-8');
    } else if (path === 'assets/main.css') {
      res.setHeader('Content-Type', 'text/css');
    } else if (path.endsWith('.png')) {
      res.setHeader('Content-Type', 'image/png');
    }

    fs.createReadStream(
      `public/CyberChef_v${internals.CYBERCHEFVERSION}.zip`
    ).pipe(unzipper.Parse()).on('entry', (entry) => {
      if (entry.path === path) {
        entry.pipe(res);
        found = true;
      } else {
        entry.autodrain();
      }
    }).on('finish', () => {
      if (!found) {
        res.status(404).end('Page not found');
      }
    });
  };
};

module.exports = MiscAPIs;
