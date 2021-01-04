'use strict';

const fs = require('fs');
const stylus = require('stylus');

module.exports = (app, Config, Db, internals, ViewerUtils) => {
  const module = {};

  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  /**
   * The user object.
   *
   * @typedef ArkimeUser
   * @type {object}
   * @param {string} userId - The ID of the user.
   * @param {string} userName - The name of the user (to be displayed in the UI).
   * @param {boolean} enabled=true - Whether the user is enabled (or disabled). Disabled users cannot access the UI or APIs.
   * @param {boolean} createEnabled=false - Can create new accounts and change the settings for other accounts and other administrative tasks.
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
   * @param {object} views - A list of views that the user can apply to their search.
   * @param {object} notifiers - A list of notifiers taht the user can use.
   * @param {object} columnConfigs - A list of sessions table column configurations that a user has created.
   * @param {object} spiviewFieldConfigs - A list of SPIView page field configurations that a user has created.
   * @param {object} tableStates - A list of table states used to render Arkime tables as the user has configured them.
   * @param {number} welcomeMsgNum=0 - The message number that a user is on. Gets incremented when a user dismisses a message.
   * @param {number} lastUsed - The date that the user last used Arkime. Format is milliseconds since Unix EPOC.
   * @param {number} timeLimit - Limits the time range a user can query for.
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
   * @param {string} connSrcField=srcIp - The default connections graph source node field.
   * @param {string} connDstField=ip.dst:port - The default connections graph destination node field.
   * @param {string} numPackets=last - The number of packets to show in the session packet area.
   * @param {string} theme=default-theme - The color theme to apply to the UI. Can be a name of a predefined field or a list of color codes if using a custom theme.
   * @param {boolean} manualQuery=false - Whether to load the sessions data by default or wait for a user to hit search manually.
   * @param {array} timelineDataFilters=['totPackets','totBytes','totDataBytes'] - The filters to display on the sessions timeline graph to change the graphs data.
   * @param {string} logo - The optionally configurable logo to show in the top navbar.
   */

  /**
   * GET - /api/user
   *
   * Retrieves the currently logged in user.
   * @name /user
   * @returns {ArkimeUser} user - The currently logged in user.
   */
  module.getUser = (req, res) => {
    const userProps = [
      'createEnabled', 'emailSearch', 'enabled', 'removeEnabled',
      'headerAuthEnabled', 'settings', 'userId', 'userName', 'webEnabled',
      'packetSearch', 'hideStats', 'hideFiles', 'hidePcap',
      'disablePcapDownload', 'welcomeMsgNum', 'lastUsed', 'timeLimit'
    ];

    const clone = {};

    for (const prop of userProps) {
      if (req.user.hasOwnProperty(prop)) {
        clone[prop] = req.user[prop];
      }
    }

    clone.canUpload = app.locals.allowUploads;

    // If esAdminUser is set use that, other wise use createEnable privilege
    if (internals.esAdminUsersSet) {
      clone.esAdminUser = internals.esAdminUsers.includes(req.user.userId);
    } else {
      clone.esAdminUser = req.user.createEnabled && Config.get('multiES', false) === false;
    }

    // If no settings, use defaults
    if (clone.settings === undefined) { clone.settings = internals.settingDefaults; }

    // Use settingsDefaults for any settings that are missing
    for (let item in internals.settingDefaults) {
      if (clone.settings[item] === undefined) {
        clone.settings[item] = internals.settingDefaults[item];
      }
    }

    return res.send(clone);
  };

  /**
   * POST - /api/user
   *
   * Creates a new Arkime user (admin only).
   * @name /user
   * @returns {boolean} success - Whether the add user operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.createUser = (req, res) => {
    if (!req.body || !req.body.userId || !req.body.userName || !req.body.password) {
      return res.molochError(403, 'Missing/Empty required fields');
    }

    if (req.body.userId.match(/[^@\w.-]/)) {
      return res.molochError(403, 'User ID must be word characters');
    }

    if (req.body.userId === '_moloch_shared') {
      return res.molochError(403, 'User ID cannot be the same as the shared moloch user');
    }

    Db.getUser(req.body.userId, function (err, user) {
      if (!user || user.found) {
        console.log('Trying to add duplicate user', err, user);
        return res.molochError(403, 'User already exists');
      }

      const nuser = {
        userId: req.body.userId,
        userName: req.body.userName,
        expression: req.body.expression,
        passStore: Config.pass2store(req.body.userId, req.body.password),
        enabled: req.body.enabled === true,
        webEnabled: req.body.webEnabled === true,
        emailSearch: req.body.emailSearch === true,
        headerAuthEnabled: req.body.headerAuthEnabled === true,
        createEnabled: req.body.createEnabled === true,
        removeEnabled: req.body.removeEnabled === true,
        packetSearch: req.body.packetSearch === true,
        timeLimit: req.body.timeLimit,
        hideStats: req.body.hideStats === true,
        hideFiles: req.body.hideFiles === true,
        hidePcap: req.body.hidePcap === true,
        disablePcapDownload: req.body.disablePcapDownload === true,
        welcomeMsgNum: 0
      };

      if (Config.debug) {
        console.log('Creating new user', nuser);
      }

      Db.setUser(req.body.userId, nuser, (err, info) => {
        if (!err) {
          return res.send(JSON.stringify({
            success: true,
            text: 'User created succesfully'
          }));
        } else {
          console.log('ERROR - add user', err, info);
          return res.molochError(403, err);
        }
      });
    });
  };

  /**
   * GET - /api/user.css
   *
   * Retrieves custom user css for the user's custom theme.
   * @name "/user.css"
   * @returns {css} css - The css file that includes user configured styles.
   */
  module.getUserCSS = (req, res) => {
    fs.readFile('./views/user.styl', 'utf8', (err, str) => {
      function error (msg) {
        console.log('ERROR - user.css -', msg);
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

      style.render(function (err, css) {
        if (err) { return error(err); }
        return res.send(css);
      });
    });
  };

  /**
   * POST - /api/users
   *
   * Retrieves a list of Arkime users (admin only).
   * @name /users
   * @returns {ArkimeUser[]} data - The list of users configured to access this Arkime cluster.
   * @returns {number} recordsTotal - The total number of users Arkime knows about.
   * @returns {number} recordsFiltered - The number of users returned in this result.
   */
  module.getUsers = (req, res) => {
    const columns = [
      'userId', 'userName', 'expression', 'enabled', 'createEnabled',
      'webEnabled', 'headerAuthEnabled', 'emailSearch', 'removeEnabled', 'packetSearch',
      'hideStats', 'hideFiles', 'hidePcap', 'disablePcapDownload', 'welcomeMsgNum',
      'lastUsed', 'timeLimit'
    ];

    const query = {
      _source: columns,
      sort: {},
      from: +req.body.start || 0,
      size: +req.body.length || 10000,
      query: { // exclude the shared user from results
        bool: { must_not: { term: { userId: '_moloch_shared' } } }
      }
    };

    if (req.body.filter) {
      query.query.bool.should = [
        { wildcard: { userName: '*' + req.body.filter + '*' } },
        { wildcard: { userId: '*' + req.body.filter + '*' } }
      ];
    }

    req.body.sortField = req.body.sortField || 'userId';
    query.sort[req.body.sortField] = { order: req.body.desc === true ? 'desc' : 'asc' };
    query.sort[req.body.sortField].missing = internals.usersMissing[req.body.sortField];

    Promise.all([
      Db.searchUsers(query),
      Db.numberOfUsers()
    ]).then(([users, total]) => {
      if (users.error) { throw users.error; }
      const results = { total: users.hits.total, results: [] };

      for (const user of users.hits.hits) {
        const fields = user._source || user.fields;
        fields.id = user._id;
        fields.expression = fields.expression || '';
        fields.headerAuthEnabled = fields.headerAuthEnabled || false;
        fields.emailSearch = fields.emailSearch || false;
        fields.removeEnabled = fields.removeEnabled || false;
        fields.userName = ViewerUtils.safeStr(fields.userName || '');
        fields.packetSearch = fields.packetSearch || false;
        fields.timeLimit = fields.timeLimit || undefined;
        results.results.push(fields);
      }

      res.send({
        recordsTotal: total.count,
        recordsFiltered: results.total,
        data: results.results
      });
    }).catch((err) => {
      console.log('ERROR - /user/list', err);
      return res.send({ recordsTotal: 0, recordsFiltered: 0, data: [] });
    });
  };

  return module;
};
