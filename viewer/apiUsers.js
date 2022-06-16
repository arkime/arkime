'use strict';

const fs = require('fs');
const util = require('util');
const stylus = require('stylus');
const User = require('../common/user');

module.exports = (Config, Db, internals, ViewerUtils) => {
  const userAPIs = {};

  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------
  function userColumns (user) {
    if (!user) { return []; }

    // Fix for new names
    if (user.columnConfigs) {
      for (const key in user.columnConfigs) {
        const item = user.columnConfigs[key];
        item.columns = item.columns.map(ViewerUtils.oldDB2newDB);
        if (item.order && item.order.length > 0) {
          item.order[0][0] = ViewerUtils.oldDB2newDB(item.order[0][0]);
        }
      }
    }

    return user.columnConfigs || [];
  }

  function userSpiview (user) {
    if (!user) { return []; }
    return user.spiviewFieldConfigs || [];
  }

  userAPIs.getCurrentUserCB = (user, clone) => {
    clone.canUpload = internals.allowUploads;

    // If esAdminUser is set use that, other wise use arkimeAdmin privilege
    if (internals.esAdminUsersSet) {
      clone.esAdminUser = internals.esAdminUsers.includes(user.userId);
    } else {
      clone.esAdminUser = user.hasRole('arkimeAdmin') && Config.get('multiES', false) === false;
    }

    // If no settings, use defaults
    if (clone.settings === undefined) { clone.settings = internals.settingDefaults; }

    // Use settingsDefaults for any settings that are missing
    for (const item in internals.settingDefaults) {
      if (clone.settings[item] === undefined) {
        clone.settings[item] = internals.settingDefaults[item];
      }
    }

    return clone;
  };

  userAPIs.findUserState = (stateName, user) => {
    if (!user.tableStates || !user.tableStates[stateName]) {
      return {};
    }

    // Fix for new names
    if (stateName === 'sessionsNew' && user.tableStates && user.tableStates.sessionsNew) {
      const item = user.tableStates.sessionsNew;
      if (item.visibleHeaders) {
        item.visibleHeaders = item.visibleHeaders.map(ViewerUtils.oldDB2newDB);
      }
      if (item.order && item.order.length > 0) {
        item.order[0][0] = ViewerUtils.oldDB2newDB(item.order[0][0]);
      }
    }

    return user.tableStates[stateName];
  };

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  /**
   * An Arkime Role
   *
   * Roles are assigned to users to give them access to Arkime content<br>
   * Default roles include:<br>
   * arkimeAdmin - has administrative access to Arkime (can configure and update Arkime)<br>
   * arkimeUser - has access to Arkime<br>
   * cont3xtAdmin - has administrative access to Cont3xt (can configure and update Cont3xt)<br>
   * cont3xtUser - has access to Cont3xt<br>
   * parliamentAdmin - has administrative access to Parliament (can configure and update Parliament)<br>
   * parliamentUser - has access to Parliament (can view and interact with Parliament Issues)<br>
   * superAdmin - has access to all the applications and can configure anything<br>
   * usersAdmin - has access to configure users<br>
   * wiseAdmin - has administrative access to WISE (can configure and update WISE)<br>
   * wiseUser - has access to WISE
   * @typedef ArkimeRole
   * @type {string}
   */

  /**
   * The user object.
   *
   * @typedef ArkimeUser
   * @type {object}
   * @param {string} userId - The ID of the user.
   * @param {string} userName - The name of the user (to be displayed in the UI).
   * @param {boolean} enabled=true - Whether the user is enabled (or disabled). Disabled users cannot access the UI or APIs.
   * @param {boolean} webEnabled=true - Can access the web interface. When off only APIs can be used.
   * @param {boolean} headerAuthEnabled=false - Can login using the web auth header. This setting doesn't disable the password so it should be scrambled.
   * @param {boolean} emailSearch=false - Can perform searches for fields relating to email.
   * @param {boolean} removeEnabled=false - Can delete tags or delete/scrub pcap data and other deletion operations.
   * @param {boolean} packetSearch=true - Can create a packet search job (hunt).
   * @param {boolean} hideStats=false - Hide the Stats page from this user.
   * @param {boolean} hideFiles=false - Hide the Files page from this user.
   * @param {boolean} hidePcap=false - Hide PCAP (and only show metadata/session detail) for this user when they open a Session.
   * @param {boolean} disablePcapDownload=false - Do not allow this user to download PCAP files.
   * @param {string} expression - An Arkime search expression that is silently added to all queries. Useful to limit what data a user can access (e.g. which nodes or IPs).
   * @param {ArkimeSettings} settings - The Arkime app settings.
   * @param {object} notifiers - A list of notifiers taht the user can use.
   * @param {object} columnConfigs - A list of sessions table column configurations that a user has created.
   * @param {object} spiviewFieldConfigs - A list of SPIView page field configurations that a user has created.
   * @param {object} tableStates - A list of table states used to render Arkime tables as the user has configured them.
   * @param {number} welcomeMsgNum=0 - The message number that a user is on. Gets incremented when a user dismisses a message.
   * @param {number} lastUsed - The date that the user last used Arkime. Format is milliseconds since Unix EPOC.
   * @param {number} timeLimit - Limits the time range a user can query for.
   * @param {array} roles - The list of Arkime roles assigned to this user.
   */

  /**
   * The settings object.
   *
   * @typedef ArkimeSettings
   * @type {object}
   * @param {string} timezone=local - The timezone applied to timestamps within the UI.
   * @param {string} detailFormat=last - The format to display the session packets. Options include: last used, natural, ascii, utf-8, hex.
   * @param {string} showTimestamps=last - Whether to display timestamps at the top of each packet.
   * @param {string} sortColumn=firstPacket - Which column to sort the sesssions table by default. Default is start time.
   * @param {string} sortDirection=desc - Whether to sort the sessions table ascending or descending.
   * @param {string} spiGraph=node - The default field to show spigraph data for.
   * @param {string} connSrcField=source.ip - The default connections graph source node field.
   * @param {string} connDstField=ip.dst:port - The default connections graph destination node field.
   * @param {string} numPackets=last - The number of packets to show in the session packet area.
   * @param {string} theme=default-theme - The color theme to apply to the UI. Can be a name of a predefined field or a list of color codes if using a custom theme.
   * @param {boolean} manualQuery=false - Whether to load the sessions data by default or wait for a user to hit search manually.
   * @param {array} timelineDataFilters=['network.packets','network.bytes','totDataBytes'] - The filters to display on the sessions timeline graph to change the graphs data.
   * @param {string} logo - The optionally configurable logo to show in the top navbar.
   */

  /**
   * A sessions table view that can be applied.
   *
   * @typedef ArkimeColumnConfig
   * @type {object}
   * @param {Array[]} order=[["firstPacket","desc"]] - What to sort the Sessions table by. The table is sorted by the first item in the array first, then the second, and so on. Each element in the array includes first the sort field followed by whether to sort descending (["firstPacket", "desc"]).
   * @param {Array} visibleHeaders=["firstPacket","lastPacket","src","source.port","dst","destination.port","network.packets","dbby","node"] - The list of Sessions table columns.
   */

  /**
   * A query to be run periodically that can perform actions on sessions that match the queries. The query runs against sessions delayed by 90 seconds to make sure all updates have been completed for that session.
   *
   * @typedef ArkimeQuery
   * @type {object}
   * @param {string} name - The name of the query
   * @param {boolean} enabled - Whether the query is enabled. If enabled, the query will run every 90 seconds.
   * @param {number} lpValue - The last packet timestamp that was searched. Used to query for the next group of sessions to search. Format is seconds since Unix EPOC.
   * @param {number} lastRun - The time that the query was last run. Format is seconds since Unix EPOC.
   * @param {number} count - The count of total sessions that have matched this query.
   * @param {number} lastCount - The count of sessions that have matched this query during its last run.
   * @param {string} query - The search expression to apply when searching for sessions.
   * @param {string} action=tag - The action to perform when sessions have matched. "tag" or "forward:clusterName".
   * @param {string} creator - The id of the user that created this query.
   * @param {string} tags - A comma separated list of tags to add to each session that matches this query.
   * @param {string} notifier - The name of the notifier to alert when there are matches for this query.
   * @param {number} lastNotified - The time that this query last sent a notification to the notifier. Only notifies every 10 mintues. Format is seconds since Unix EPOC.
   * @param {number} lastNotifiedCount - The count of sessions that matched since the last notification was sent.
   * @param {string} description - The description of this query.
   * @param {number} created - The time that this query was created. Format is seconds since Unix EPOC.
   * @param {number} lastToggled - The time that this query was enabled or disabled. Format is seconds since Unix EPOC.
   * @param {string} lastToggledBy - The user who last enabled or disabled this query.
   */

  /**
   * GET - /api/user/css OR /api/user.css
   *
   * Retrieves custom user css for the user's custom theme.
   * @name /user/css
   * @returns {css} css - The css file that includes user configured styles.
   */
  userAPIs.getUserCSS = (req, res) => {
    fs.readFile('./views/user.styl', 'utf8', (err, str) => {
      function error (msg) {
        console.log(`ERROR - ${req.method} /api/user/css`, msg);
        return res.status(404).end();
      }

      const date = new Date().toUTCString();
      res.setHeader('Content-Type', 'text/css');
      res.setHeader('Date', date);
      res.setHeader('Cache-Control', 'public, max-age=0');
      res.setHeader('Last-Modified', date);

      if (err) { return error(err); }
      if (!req.user.settings.theme) { return error('no custom theme defined'); }

      const theme = req.user.settings.theme.split(':');

      if (!theme[1]) { return error('custom theme corrupted'); }

      const style = stylus(str);

      const colors = theme[1].split(',');

      if (!colors) { return error('custom theme corrupted'); }

      style.define('colorBackground', new stylus.nodes.Literal(colors[0]));
      style.define('colorForeground', new stylus.nodes.Literal(colors[1]));
      style.define('colorForegroundAccent', new stylus.nodes.Literal(colors[2]));

      style.define('colorWhite', new stylus.nodes.Literal('#FFFFFF'));
      style.define('colorBlack', new stylus.nodes.Literal('#333333'));
      style.define('colorGray', new stylus.nodes.Literal('#CCCCCC'));
      style.define('colorGrayDark', new stylus.nodes.Literal('#777777'));
      style.define('colorGrayDarker', new stylus.nodes.Literal('#555555'));
      style.define('colorGrayLight', new stylus.nodes.Literal('#EEEEEE'));
      style.define('colorGrayLighter', new stylus.nodes.Literal('#F6F6F6'));

      style.define('colorPrimary', new stylus.nodes.Literal(colors[3]));
      style.define('colorPrimaryLightest', new stylus.nodes.Literal(colors[4]));
      style.define('colorSecondary', new stylus.nodes.Literal(colors[5]));
      style.define('colorSecondaryLightest', new stylus.nodes.Literal(colors[6]));
      style.define('colorTertiary', new stylus.nodes.Literal(colors[7]));
      style.define('colorTertiaryLightest', new stylus.nodes.Literal(colors[8]));
      style.define('colorQuaternary', new stylus.nodes.Literal(colors[9]));
      style.define('colorQuaternaryLightest', new stylus.nodes.Literal(colors[10]));

      style.define('colorWater', new stylus.nodes.Literal(colors[11]));
      style.define('colorLand', new stylus.nodes.Literal(colors[12]));
      style.define('colorSrc', new stylus.nodes.Literal(colors[13]));
      style.define('colorDst', new stylus.nodes.Literal(colors[14]));

      style.render((err, css) => {
        if (err) { return error(err); }
        return res.send(css);
      });
    });
  };

  /**
   * GET - /api/user/settings
   *
   * Retrieves an Arkime user's settings.
   * @name /user/settings
   * @returns {ArkimeSettings} settings - The user's configured settings
   */
  userAPIs.getUserSettings = (req, res) => {
    const settings = (req.settingUser.settings)
      ? Object.assign(JSON.parse(JSON.stringify(internals.settingDefaults)), JSON.parse(JSON.stringify(req.settingUser.settings)))
      : JSON.parse(JSON.stringify(internals.settingDefaults));

    return res.send(settings);
  };

  /**
   * POST - /api/user/settings
   *
   * Updates an Arkime user's settings.
   * @name /user/settings
   * @returns {boolean} success - Whether the update user settings operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  userAPIs.updateUserSettings = (req, res) => {
    req.settingUser.settings = req.body;
    delete req.settingUser.settings.token;

    User.setUser(req.settingUser.userId, req.settingUser, (err, info) => {
      if (err) {
        console.log(`ERROR - ${req.method} /api/user/settings update error`, util.inspect(err, false, 50), info);
        return res.serverError(500, 'User settings update failed');
      }

      return res.send(JSON.stringify({
        success: true,
        text: 'Updated user settings successfully'
      }));
    });
  };

  /**
   * GET - /api/user/crons
   *
   * Retrieves periodic queries for a user.
   * @name /user/crons
   * @returns {ArkimeQuery[]} queries - A list of query objects.
   */
  userAPIs.getUserCron = (req, res) => {
    if (!req.settingUser) {
      return res.serverError(403, 'Unknown user');
    }

    const user = req.settingUser;
    if (user.settings === undefined) { user.settings = {}; }

    const query = {
      size: 1000,
      sort: { created: { order: 'asc' } },
      query: { term: { creator: user.userId } }
    };

    Db.search('queries', 'query', query, (err, data) => {
      if (err || data.error) {
        console.log(`ERROR - ${req.method} /api/user/crons`, util.inspect(err || data.error, false, 50));
      }

      const queries = [];

      if (data && data.hits && data.hits.hits) {
        data.hits.hits.forEach((item) => {
          queries.push({ ...item._source, key: item._id });
        });
      }

      res.send(queries);
    });
  };

  /**
   * POST - /api/user/cron
   *
   * Create a new periodic query for a user.
   * @name /user/cron
   * @returns {boolean} success - Whether the create operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {ArkimeQuery} query - The new query
   */
  userAPIs.createUserCron = async (req, res) => {
    if (!req.body.name) {
      return res.serverError(403, 'Missing query name');
    }
    if (!req.body.query) {
      return res.serverError(403, 'Missing query expression');
    }
    if (!req.body.action) {
      return res.serverError(403, 'Missing query action');
    }
    if (!req.body.tags) {
      return res.serverError(403, 'Missing query tag(s)');
    }

    const doc = {
      doc: {
        enabled: true,
        name: req.body.name,
        query: req.body.query,
        tags: req.body.tags,
        action: req.body.action,
        created: Math.floor(Date.now() / 1000)
      }
    };

    if (req.body.description) {
      doc.doc.description = req.body.description;
    }

    if (req.body.notifier) {
      doc.doc.notifier = req.body.notifier;
    }

    const userId = req.settingUser.userId;

    let minTimestamp;
    try {
      const { body: data } = await Db.getMinValue(['sessions2-*', 'sessions3-*'], '@timestamp');
      minTimestamp = Math.floor(data.aggregations.min.value / 1000);
    } catch (err) {
      minTimestamp = Math.floor(Date.now() / 1000);
    }

    if (+req.body.since === -1) {
      doc.doc.lpValue = doc.doc.lastRun = minTimestamp;
    } else {
      doc.doc.lpValue = doc.doc.lastRun =
         Math.max(minTimestamp, Math.floor(Date.now() / 1000) - 60 * 60 * parseInt(req.body.since || '0', 10));
    }

    doc.doc.count = 0;
    doc.doc.creator = userId || 'anonymous';

    try {
      const { body: info } = await Db.indexNow('queries', 'query', null, doc.doc);

      if (Config.get('cronQueries', false)) {
        internals.processCronQueries();
      }

      doc.doc.key = info._id;

      return res.send(JSON.stringify({
        success: true,
        query: doc.doc,
        text: 'Created query successfully'
      }));
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/user/cron`, util.inspect(err, false, 50));
      return res.serverError(500, 'Create query failed');
    }
  };

  /**
   * DELETE - /api/user/cron/:key
   *
   * Delete a periodic query for a user.
   * @name /user/cron/:key
   * @returns {boolean} success - Whether the delete operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  userAPIs.deleteUserCron = async (req, res) => {
    const key = req.body.key;
    if (!key) {
      return res.serverError(403, 'Missing query key');
    }

    try {
      await Db.deleteDocument('queries', 'query', key, { refresh: true });
      res.send(JSON.stringify({
        success: true,
        text: 'Deleted query successfully'
      }));
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/user/cron/${key}`, util.inspect(err, false, 50));
      return res.serverError(500, 'Delete query failed');
    }
  };

  /**
   * POST - /api/user/cron/:key
   *
   * Update a periodic query for a user.
   * @name /user/cron/:key
   * @returns {boolean} success - Whether the update operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {ArkimeQuery} query - The updated query object
   */
  userAPIs.updateUserCron = async (req, res) => {
    const key = req.body.key;
    if (!key) {
      return res.serverError(403, 'Missing query key');
    }
    if (!req.body.name) {
      return res.serverError(403, 'Missing query name');
    }
    if (!req.body.query) {
      return res.serverError(403, 'Missing query expression');
    }
    if (!req.body.action) {
      return res.serverError(403, 'Missing query action');
    }
    if (!req.body.tags) {
      return res.serverError(403, 'Missing query tag(s)');
    }

    const doc = {
      doc: {
        enabled: req.body.enabled,
        name: req.body.name,
        query: req.body.query,
        tags: req.body.tags,
        action: req.body.action,
        notifier: undefined,
        description: ''
      }
    };

    if (req.body.notifier) {
      doc.doc.notifier = req.body.notifier;
    }

    if (req.body.description) {
      doc.doc.description = req.body.description;
    }

    try {
      const { body: { _source: cron } } = await Db.get('queries', 'query', key);

      if (doc.doc.enabled !== cron.enabled) { // the query was enabled or disabled
        doc.doc.lastToggledBy = req.settingUser.userId;
        doc.doc.lastToggled = Math.floor(Date.now() / 1000);
      }

      const query = { // last object property overwrites the previous one
        ...cron,
        ...doc.doc
      };

      try {
        await Db.update('queries', 'query', key, doc, { refresh: true });
      } catch (err) {
        console.log(`ERROR - ${req.method} /api/user/cron/${key}`, util.inspect(err, false, 50));
      }

      if (Config.get('cronQueries', false)) { internals.processCronQueries(); }

      query.key = key;

      return res.send(JSON.stringify({
        success: true,
        text: 'Updated query successfully',
        query
      }));
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/user/cron/${key}`, util.inspect(err, false, 50));
      return res.serverError(403, 'Query update failed');
    }
  };

  /**
   * GET - /api/user/columns
   *
   * Retrieves user configured custom Sessions column configurations.
   * @name /user/columns
   * @returns {ArkimeColumnConfig[]} columnConfigs - The custom Sessions column configurations.
   */
  userAPIs.getUserColumns = (req, res) => {
    return res.send(userColumns(req.settingUser));
  };

  /**
   * POST - /api/user/column
   *
   * Creates a new user configured custom Sessions column configuration.
   * @name /user/column
   * @returns {boolean} success - Whether the create column configuration operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {string} name - The name of the new custom Sessions column configuration.
   */
  userAPIs.createUserColumns = (req, res) => {
    if (!req.body.name) {
      return res.serverError(403, 'Missing custom column configuration name');
    }
    if (!req.body.columns) {
      return res.serverError(403, 'Missing columns');
    }
    if (!req.body.order) {
      return res.serverError(403, 'Missing sort order');
    }

    req.body.name = req.body.name.replace(/[^-a-zA-Z0-9\s_:]/g, '');
    if (req.body.name.length < 1) {
      return res.serverError(403, 'Invalid custom column configuration name');
    }

    const user = req.settingUser;
    user.columnConfigs = user.columnConfigs || [];

    // don't let user use duplicate names
    for (const config of user.columnConfigs) {
      if (req.body.name === config.name) {
        return res.serverError(403, 'There is already a custom column with that name');
      }
    }

    user.columnConfigs.push({
      name: req.body.name,
      columns: req.body.columns,
      order: req.body.order
    });

    User.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log(`ERROR - ${req.method} /api/user/column`, util.inspect(err, false, 50), info);
        return res.serverError(500, 'Create custom column configuration failed');
      }

      return res.send(JSON.stringify({
        success: true,
        text: 'Created custom column configuration successfully',
        name: req.body.name
      }));
    });
  };

  /**
   * PUT - /api/user/column/:name
   *
   * Updates a user configured custom Sessions column configuration.
   * @name /user/column/:name
   * @returns {boolean} success - Whether the update column configuration operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {ArkimeColumnConfig} colConfig - The udpated custom Sessions column configuration.
   */
  userAPIs.updateUserColumns = (req, res) => {
    const colName = req.body.name || req.params.name;
    if (!colName) {
      return res.serverError(403, 'Missing custom column configuration name');
    }
    if (!req.body.columns) {
      return res.serverError(403, 'Missing columns');
    }
    if (!req.body.order) {
      return res.serverError(403, 'Missing sort order');
    }

    const user = req.settingUser;
    user.columnConfigs = user.columnConfigs || [];

    // find the custom column configuration to update
    let found = false;
    for (let i = 0, len = user.columnConfigs.length; i < len; i++) {
      if (colName === user.columnConfigs[i].name) {
        user.columnConfigs[i] = req.body;
        found = true;
        break;
      }
    }

    if (!found) {
      return res.serverError(200, 'Custom column configuration not found');
    }

    User.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log(`ERROR - ${req.method} /api/user/column/${colName}`, util.inspect(err, false, 50), info);
        return res.serverError(500, 'Update custom column configuration failed');
      }

      return res.send(JSON.stringify({
        success: true,
        text: 'Updated column configuration',
        colConfig: req.body
      }));
    });
  };

  /**
   * DELETE - /api/user/column/:name
   *
   * Deletes a user configured custom Sessions column configuration.
   * @name /user/column/:name
   * @returns {boolean} success - Whether the delete Sessions column configuration operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  userAPIs.deleteUserColumns = (req, res) => {
    const colName = req.body.name || req.params.name;
    if (!colName) {
      return res.serverError(403, 'Missing custom column configuration name');
    }

    const user = req.settingUser;
    user.columnConfigs = user.columnConfigs || [];

    let found = false;
    for (let i = 0, ilen = user.columnConfigs.length; i < ilen; ++i) {
      if (colName === user.columnConfigs[i].name) {
        user.columnConfigs.splice(i, 1);
        found = true;
        break;
      }
    }

    if (!found) {
      return res.serverError(200, 'Custom column configuration not found');
    }

    User.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log(`ERROR - ${req.method} /api/user/column/${colName}`, util.inspect(err, false, 50), info);
        return res.serverError(500, 'Delete custom column configuration failed');
      }

      return res.send(JSON.stringify({
        success: true,
        text: 'Deleted custom column configuration successfully'
      }));
    });
  };

  /**
   * GET - /api/user/spiview
   *
   * Retrieves a user configured SPI View fields configuration.
   * @name /user/spiview
   * @returns {Array} spiviewFieldConfigs - User configured SPI View field configuration.
   */
  userAPIs.getUserSpiviewFields = (req, res) => {
    return res.send(userSpiview(req.settingUser));
  };

  /**
   * POST - /api/user/spiview
   *
   * Create a user configured SPI View fields configuration.
   * @name /user/spiview
   * @returns {boolean} success - Whether the update SPI View fields configuration operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {string} name - The name of the new SPI View fields configuration.
   */
  userAPIs.createUserSpiviewFields = (req, res) => {
    if (!req.body.name) {
      return res.serverError(403, 'Missing custom SPI View fields configuration name');
    }
    if (!req.body.fields) {
      return res.serverError(403, 'Missing fields');
    }

    req.body.name = req.body.name.replace(/[^-a-zA-Z0-9\s_:]/g, '');

    if (req.body.name.length < 1) {
      return res.serverError(403, 'Invalid custom SPI View fields configuration name');
    }

    const user = req.settingUser;
    user.spiviewFieldConfigs = user.spiviewFieldConfigs || [];

    // don't let user use duplicate names
    for (const config of user.spiviewFieldConfigs) {
      if (req.body.name === config.name) {
        return res.serverError(403, 'There is already a custom SPI View fieldss configuration with that name');
      }
    }

    user.spiviewFieldConfigs.push({
      name: req.body.name,
      fields: req.body.fields
    });

    User.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log(`ERROR - ${req.method} /api/user/spiview`, util.inspect(err, false, 50), info);
        return res.serverError(500, 'Create custom SPI View fields configuration failed');
      }

      return res.send(JSON.stringify({
        success: true,
        text: 'Created custom SPI View fieldss configuration successfully',
        name: req.body.name
      }));
    });
  };

  /**
   * PUT - /api/user/spiview/:name
   *
   * Updates a user configured SPI View fields configuration.
   * @name /user/spiview/:name
   * @returns {boolean} success - Whether the update SPI View fields configuration operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {object} colConfig - The udpated SPI View fields configuration.
   */
  userAPIs.updateUserSpiviewFields = (req, res) => {
    const spiName = req.body.name || req.params.name;
    if (!spiName) {
      return res.serverError(403, 'Missing custom SPI View fields configuration name');
    }
    if (!req.body.fields) {
      return res.serverError(403, 'Missing fields');
    }

    const user = req.settingUser;
    user.spiviewFieldConfigs = user.spiviewFieldConfigs || [];

    // find the custom SPI View fields configuration to update
    let found = false;
    for (let i = 0, len = user.spiviewFieldConfigs.length; i < len; i++) {
      if (spiName === user.spiviewFieldConfigs[i].name) {
        user.spiviewFieldConfigs[i] = req.body;
        found = true;
        break;
      }
    }

    if (!found) {
      return res.serverError(200, 'Custom SPI View fields configuration not found');
    }

    User.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log(`ERROR - ${req.method} /api/user/spiview/${spiName}`, util.inspect(err, false, 50), info);
        return res.serverError(500, 'Update SPI View fields configuration failed');
      }

      return res.send(JSON.stringify({
        success: true,
        text: 'Updated SPI View fields configuration',
        colConfig: req.body
      }));
    });
  };

  /**
   * DELETE - /api/user/spiview/:name
   *
   * Deletes a user configured SPI View fields configuration.
   * @name /user/spiview/:name
   * @returns {boolean} success - Whether the delete SPI View fields configuration operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  userAPIs.deleteUserSpiviewFields = (req, res) => {
    const spiName = req.params.name || req.body.name;
    if (!spiName) {
      return res.serverError(403, 'Missing custom SPI View fields configuration name');
    }

    const user = req.settingUser;
    user.spiviewFieldConfigs = user.spiviewFieldConfigs || [];

    let found = false;
    for (let i = 0, ilen = user.spiviewFieldConfigs.length; i < ilen; ++i) {
      if (spiName === user.spiviewFieldConfigs[i].name) {
        user.spiviewFieldConfigs.splice(i, 1);
        found = true;
        break;
      }
    }

    if (!found) {
      return res.serverError(200, 'SPI View fields not found');
    }

    User.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log(`ERROR - ${req.method} /api/user/spiview/${spiName}`, util.inspect(err, false, 50), info);
        return res.serverError(500, 'Delete custom SPI View fields configuration failed');
      }

      return res.send(JSON.stringify({
        success: true,
        text: 'Deleted custom SPI View fields configuration successfully'
      }));
    });
  };

  /**
   * PUT - /api/user/:userId/acknowledge
   *
   * Acknowledges a UI message for a user. Used to display help popups.
   * @name /user/:userId/acknowledge
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  userAPIs.acknowledgeMsg = (req, res) => {
    if (!req.body.msgNum) {
      return res.serverError(403, 'Message number required');
    }

    if (req.params.userId !== req.user.userId) {
      return res.serverError(403, 'Can not change other users msg');
    }

    User.getUser(req.params.userId, (err, user) => {
      if (err || !user) {
        console.log(`ERROR - ${req.method} /api/user/${req.params.userId}/acknowledge (getUser)`, util.inspect(err, false, 50), user);
        return res.serverError(403, 'User not found');
      }

      user.welcomeMsgNum = parseInt(req.body.msgNum);

      User.setUser(req.params.userId, user, (err, info) => {
        if (Config.debug) {
          console.log(`ERROR - ${req.method} /api/user/${req.params.userId}/acknowledge (setUser)`, util.inspect(err, false, 50), user, info);
        }

        return res.send(JSON.stringify({
          success: true,
          text: `User, ${req.params.userId}, dismissed message ${req.body.msgNum}`
        }));
      });
    });
  };

  /**
   * GET - /api/user/state/:name
   *
   * Retrieves a user table state object. These are used to save the states of tables within the UI (sessions, files, stats, etc).
   * @name /user/state/:name
   * @returns {object} tableState - The table state requested.
   */
  userAPIs.getUserState = (req, res) => {
    return res.send(userAPIs.findUserState(req.params.name, req.user));
  };

  /**
   * POST - /api/user/state/:name
   *
   * Updates or creates a user table state object. These are used to save the states of tables within the UI (sessions, files, stats, etc).
   * @name /user/state/:name
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  userAPIs.updateUserState = (req, res) => {
    User.getUser(req.user.userId, (err, user) => {
      if (err || !user) {
        console.log(`ERROR - ${req.method} /api/user/state/${req.params.name} (getUser)`, util.inspect(err, false, 50), user);
        return res.serverError(403, 'Unknown user');
      }

      if (!user.tableStates) {
        user.tableStates = {};
      }

      user.tableStates[req.params.name] = req.body;

      User.setUser(user.userId, user, (err, info) => {
        if (err) {
          console.log(`ERROR - ${req.method} /api/user/state/${req.params.name} (setUser)`, util.inspect(err, false, 50), info);
          return res.serverError(403, 'state update failed');
        }

        return res.send(JSON.stringify({
          success: true,
          text: 'updated state successfully'
        }));
      });
    });
  };

  /**
   * GET - /api/user/config/:page
   *
   * Fetches the configuration information for a UI page for a user.
   * @name /user/config/:page
   * @returns {object} config The configuration data for the page
   */
  userAPIs.getPageConfig = (req, res) => {
    switch (req.params.page) {
    case 'sessions': {
      const colConfigs = userColumns(req.settingUser);
      const tableState = userAPIs.findUserState('sessionsNew', req.user);
      const colWidths = userAPIs.findUserState('sessionsColWidths', req.user);
      return res.send({ colWidths, tableState, colConfigs });
    }
    case 'spiview': {
      const fieldConfigs = userSpiview(req.settingUser);
      const spiviewFields = userAPIs.findUserState('spiview', req.user);
      return res.send({ fieldConfigs, spiviewFields });
    }
    case 'connections': {
      const fieldHistoryConnectionsSrc = userAPIs.findUserState('fieldHistoryConnectionsSrc', req.user);
      const fieldHistoryConnectionsDst = userAPIs.findUserState('fieldHistoryConnectionsDst', req.user);
      return res.send({ fieldHistoryConnectionsSrc, fieldHistoryConnectionsDst });
    }
    case 'files': {
      const tableState = userAPIs.findUserState('fieldsCols', req.user);
      const columnWidths = userAPIs.findUserState('filesColWidths', req.user);
      return res.send({ tableState, columnWidths });
    }
    case 'captureStats': {
      const tableState = userAPIs.findUserState('captureStatsCols', req.user);
      const columnWidths = userAPIs.findUserState('captureStatsColWidths', req.user);
      return res.send({ tableState, columnWidths });
    }
    case 'esIndices': {
      const tableState = userAPIs.findUserState('esIndicesCols', req.user);
      const columnWidths = userAPIs.findUserState('esIndicesColWidths', req.user);
      return res.send({ tableState, columnWidths });
    }
    case 'esNodes': {
      const tableState = userAPIs.findUserState('esNodesCols', req.user);
      const columnWidths = userAPIs.findUserState('esNodesColWidths', req.user);
      return res.send({ tableState, columnWidths });
    }
    case 'esRecovery': {
      const tableState = userAPIs.findUserState('esRecoveryCols', req.user);
      const columnWidths = userAPIs.findUserState('esRecoveryColWidths', req.user);
      return res.send({ tableState, columnWidths });
    }
    case 'esTasks': {
      const tableState = userAPIs.findUserState('esTasksCols', req.user);
      const columnWidths = userAPIs.findUserState('esTasksColWidths', req.user);
      return res.send({ tableState, columnWidths });
    }
    default:
      return res.serverError(501, 'Requested page is not supported');
    }
  };

  return userAPIs;
};
