'use strict';

const fs = require('fs');
const stylus = require('stylus');

module.exports = (Config, Db, internals, ViewerUtils) => {
  const module = {};

  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------
  function saveSharedView (req, res, user, view, endpoint, successMessage, errorMessage) {
    Db.getUser('_moloch_shared', (err, sharedUser) => {
      if (!sharedUser || !sharedUser.found) {
        // sharing for the first time
        sharedUser = {
          userId: '_moloch_shared',
          userName: '_moloch_shared',
          enabled: false,
          webEnabled: false,
          emailSearch: false,
          headerAuthEnabled: false,
          createEnabled: false,
          removeEnabled: false,
          packetSearch: false,
          views: {}
        };
      } else {
        sharedUser = sharedUser._source;
      }

      sharedUser.views = sharedUser.views || {};

      if (sharedUser.views[req.body.name]) {
        console.log('Trying to add duplicate shared view', sharedUser);
        return res.molochError(403, 'Shared view already exists');
      }

      sharedUser.views[req.body.name] = view;

      Db.setUser('_moloch_shared', sharedUser, (err, info) => {
        if (err) {
          console.log(endpoint, 'failed', err, info);
          return res.molochError(500, errorMessage);
        }

        return res.send(JSON.stringify({
          success: true,
          text: successMessage,
          viewName: req.body.name,
          view: view
        }));
      });
    });
  }

  // removes a view from the user that created the view and adds it to the shared user
  function shareView (req, res, user, endpoint, successMessage, errorMessage) {
    const view = user.views[req.body.name];
    view.shared = true;

    delete user.views[req.body.name]; // remove the view from the

    Db.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log(endpoint, 'failed', err, info);
        return res.molochError(500, errorMessage);
      }

      // save the view on the shared user
      return saveSharedView(req, res, user, view, endpoint, successMessage, errorMessage);
    });
  }

  // removes a view from the shared user and adds it to the user that created the view
  function unshareView (req, res, user, sharedUser, endpoint, successMessage, errorMessage) {
    Db.setUser('_moloch_shared', sharedUser, (err, info) => {
      if (err) {
        console.log(endpoint, 'failed', err, info);
        return res.molochError(500, errorMessage);
      }

      if (user.views[req.body.name]) { // the user already has a view with this name
        return res.molochError(403, 'A view already exists with this name.');
      }

      user.views[req.body.name] = {
        expression: req.body.expression,
        user: req.body.user, // keep the user so we know who created it
        shared: false,
        sessionsColConfig: req.body.sessionsColConfig
      };

      Db.setUser(user.userId, user, (err, info) => {
        if (err) {
          console.log(endpoint, 'failed', err, info);
          return res.molochError(500, errorMessage);
        }

        return res.send(JSON.stringify({
          success: true,
          text: successMessage
        }));
      });
    });
  }

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
   * A database view that can be applied to any search.
   *
   * @typedef ArkimeView
   * @type {object}
   * @param {string} expression - The search expression to filter sessions.
   * @param {ArkimeColumnConfig} sessionsColConfig - The Sessions column configuration to apply to the Sessions table when applying the view.
   * @param {boolean} shared - Whether the view is shared with other users in the Arkime cluster.
   * @param {string} user - The user ID of the user who created the view.
   */

  /**
   * A database view that can be applied to any search.
   *
   * @typedef ArkimeColumnConfig
   * @type {object}
   * @param {Array[]} order=[["firstPacket","desc"]] - What to sort the Sessions table by. The table is sorted by the first item in the array first, then the second, and so on. Each element in the array includes first the sort field followed by whether to sort descending (["firstPacket", "desc"]).
   * @param {Array} visibleHeaders=["firstPacket","lastPacket","src","srcPort","dst","dstPort","totPackets","dbby","node"] - The list of Sessions table columns.
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
      if (req.user[prop]) {
        clone[prop] = req.user[prop];
      }
    }

    clone.canUpload = internals.allowUploads;

    // If esAdminUser is set use that, other wise use createEnable privilege
    if (internals.esAdminUsersSet) {
      clone.esAdminUser = internals.esAdminUsers.includes(req.user.userId);
    } else {
      clone.esAdminUser = req.user.createEnabled && Config.get('multiES', false) === false;
    }

    // If no settings, use defaults
    if (clone.settings === undefined) { clone.settings = internals.settingDefaults; }

    // Use settingsDefaults for any settings that are missing
    for (const item in internals.settingDefaults) {
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

    Db.getUser(req.body.userId, (err, user) => {
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
   * DELETE - /api/user/:id
   *
   * Deletes an Arkime user (admin only).
   * @name /user/:id
   * @returns {boolean} success - Whether the delete user operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.deleteUser = (req, res) => {
    const userId = req.body.userId || req.params.id;
    if (userId === req.user.userId) {
      return res.molochError(403, 'Can not delete yourself');
    }

    Db.deleteUser(userId, (err, data) => {
      setTimeout(() => {
        res.send(JSON.stringify({
          success: true, text: 'User deleted successfully'
        }));
      }, 200);
    });
  };

  /**
   * POST - /api/user/:id
   *
   * Updates an Arkime user (admin only).
   * @name /user/:id
   * @returns {boolean} success - Whether the update user operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.updateUser = (req, res) => {
    const userId = req.body.userId || req.params.id;

    if (!userId) {
      return res.molochError(403, 'Missing userId');
    }

    if (userId === '_moloch_shared') {
      return res.molochError(403, '_moloch_shared is a shared user. This users settings cannot be updated');
    }

    Db.getUser(userId, (err, user) => {
      if (err || !user.found) {
        console.log('update user failed', err, user);
        return res.molochError(403, 'User not found');
      }

      user = user._source;

      user.enabled = req.body.enabled === true;

      if (req.body.expression !== undefined) {
        if (req.body.expression.match(/^\s*$/)) {
          delete user.expression;
        } else {
          user.expression = req.body.expression;
        }
      }

      if (req.body.userName !== undefined) {
        if (req.body.userName.match(/^\s*$/)) {
          console.log('ERROR - empty username', req.body);
          return res.molochError(403, 'Username can not be empty');
        } else {
          user.userName = req.body.userName;
        }
      }

      user.webEnabled = req.body.webEnabled === true;
      user.emailSearch = req.body.emailSearch === true;
      user.headerAuthEnabled = req.body.headerAuthEnabled === true;
      user.removeEnabled = req.body.removeEnabled === true;
      user.packetSearch = req.body.packetSearch === true;
      user.hideStats = req.body.hideStats === true;
      user.hideFiles = req.body.hideFiles === true;
      user.hidePcap = req.body.hidePcap === true;
      user.disablePcapDownload = req.body.disablePcapDownload === true;
      user.timeLimit = req.body.timeLimit ? parseInt(req.body.timeLimit) : undefined;

      // Can only change createEnabled if it is currently turned on
      if (req.body.createEnabled !== undefined && req.user.createEnabled) {
        user.createEnabled = req.body.createEnabled === true;
      }

      Db.setUser(userId, user, (err, info) => {
        if (Config.debug) {
          console.log('setUser', user, err, info);
        }

        return res.send(JSON.stringify({
          success: true,
          text: `User ${userId} updated successfully`
        }));
      });
    });
  };

  /**
   * POST - /api/user/password
   *
   * Update user password.
   * @name /user/password
   * @returns {boolean} success - Whether the update password operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.updateUserPassword = (req, res) => {
    if (!req.body.newPassword || req.body.newPassword.length < 3) {
      return res.molochError(403, 'New password needs to be at least 3 characters');
    }

    if (!req.user.createEnabled && (Config.store2ha1(req.user.passStore) !==
      Config.store2ha1(Config.pass2store(req.token.userId, req.body.currentPassword)) ||
      req.token.userId !== req.user.userId)) {
      return res.molochError(403, 'New password mismatch');
    }

    const user = req.settingUser;
    user.passStore = Config.pass2store(user.userId, req.body.newPassword);

    Db.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log('/api/user/password update error', err, info);
        return res.molochError(500, 'Password update failed');
      }

      return res.send(JSON.stringify({
        success: true,
        text: 'Changed password successfully'
      }));
    });
  };

  /**
   * GET - /api/user/css OR /api/user.css
   *
   * Retrieves custom user css for the user's custom theme.
   * @name /user/css
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

      style.render((err, css) => {
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
      console.log('/api/users failed', err);
      return res.send({
        recordsTotal: 0, recordsFiltered: 0, data: []
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
  module.getUserSettings = (req, res) => {
    const settings = (req.settingUser.settings)
      ? Object.assign(JSON.parse(JSON.stringify(internals.settingDefaults)), JSON.parse(JSON.stringify(req.settingUser.settings)))
      : JSON.parse(JSON.stringify(internals.settingDefaults));

    const cookieOptions = {
      path: Config.basePath(),
      sameSite: 'Strict'
    };

    if (Config.isHTTPS()) { cookieOptions.secure = true; }

    res.cookie(
      'MOLOCH-COOKIE',
      Config.obj2auth({
        date: Date.now(), pid: process.pid, userId: req.user.userId
      }, true),
      cookieOptions
    );

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
  module.updateUserSettings = (req, res) => {
    req.settingUser.settings = req.body;
    delete req.settingUser.settings.token;

    Db.setUser(req.settingUser.userId, req.settingUser, (err, info) => {
      if (err) {
        console.log('/api/user/settings update error', err, info);
        return res.molochError(500, 'User settings update failed');
      }

      return res.send(JSON.stringify({
        success: true,
        text: 'Updated user settings successfully'
      }));
    });
  };

  /**
   * GET - /api/user/views
   *
   * Retrieves an Arkime user's views.
   * @name /user/views
   * @returns {ArkimeView[]} views - A list of views a user has configured or has been shared.
   */
  module.getUserViews = (req, res) => {
    if (!req.settingUser) { return res.send({}); }

    // Clone the views so we don't modify that cached user
    const views = JSON.parse(JSON.stringify(req.settingUser.views || {}));

    Db.getUser('_moloch_shared', (err, sharedUser) => {
      if (sharedUser && sharedUser.found) {
        sharedUser = sharedUser._source;
        for (const viewName in sharedUser.views) {
          // check for views with the same name as a shared view so user specific views don't get overwritten
          let sharedViewName = viewName;
          if (views[sharedViewName] && !views[sharedViewName].shared) {
            sharedViewName = `shared:${sharedViewName}`;
          }
          views[sharedViewName] = sharedUser.views[viewName];
        }
      }

      return res.send(views);
    });
  };

  /**
   * POST - /api/user/view
   *
   * Creates an Arkime view for a user.
   * @name /user/view
   * @returns {boolean} success - Whether the create view operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {string} viewName - The name of the new view.
   * @returns {ArkimeView} view - The new view data.
   */
  module.createUserView = (req, res) => {
    if (!req.body.name) {
      return res.molochError(403, 'Missing view name');
    }

    if (!req.body.expression) {
      return res.molochError(403, 'Missing view expression');
    }

    const user = req.settingUser;
    user.views = user.views || {};

    const newView = {
      expression: req.body.expression,
      user: user.userId
    };

    if (req.body.shared) {
      // save the view on the shared user
      newView.shared = true;
      saveSharedView(req, res, user, newView, '/api/user/view', 'Created shared view successfully', 'Create shared view failed');
    } else {
      newView.shared = false;
      if (user.views[req.body.name]) {
        return res.molochError(403, 'A view already exists with this name.');
      } else {
        user.views[req.body.name] = newView;
      }

      if (req.body.sessionsColConfig) {
        user.views[req.body.name].sessionsColConfig = req.body.sessionsColConfig;
      } else if (user.views[req.body.name].sessionsColConfig && !req.body.sessionsColConfig) {
        user.views[req.body.name].sessionsColConfig = undefined;
      }

      Db.setUser(user.userId, user, (err, info) => {
        if (err) {
          console.log('/api/user/view create error', err, info);
          return res.molochError(500, 'Create view failed');
        }

        return res.send(JSON.stringify({
          success: true,
          text: 'Created view successfully',
          viewName: req.body.name,
          view: newView
        }));
      });
    }
  };

  /**
   * DELETE - /api/user/view/:name
   *
   * Deletes an Arkime view for a user.
   * @name /user/view/:name
   * @returns {boolean} success - Whether the delete view operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.deleteUserView = (req, res) => {
    const name = req.body.name || req.params.name;
    if (!name) {
      return res.molochError(403, 'Missing view name');
    }

    const user = req.settingUser;
    user.views = user.views || {};

    if (req.body.shared) {
      Db.getUser('_moloch_shared', (err, sharedUser) => {
        if (sharedUser && sharedUser.found) {
          sharedUser = sharedUser._source;
          sharedUser.views = sharedUser.views || {};
          if (sharedUser.views[name] === undefined) {
            return res.molochError(404, 'View not found');
          }
          // only admins or the user that created the view can delete the shared view
          if (!user.createEnabled && sharedUser.views[name].user !== user.userId) {
            return res.molochError(401, 'Need admin privelages to delete another user\'s shared view');
          }
          delete sharedUser.views[name];
        }

        Db.setUser('_moloch_shared', sharedUser, (err, info) => {
          if (err) {
            console.log('/api/user/view delete failed', err, info);
            return res.molochError(500, 'Delete shared view failed');
          }

          return res.send(JSON.stringify({
            success: true,
            text: 'Deleted shared view successfully'
          }));
        });
      });
    } else {
      if (user.views[name] === undefined) {
        return res.molochError(404, 'View not found');
      }
      delete user.views[name];

      Db.setUser(user.userId, user, (err, info) => {
        if (err) {
          console.log('/api/user/view delete failed', err, info);
          return res.molochError(500, 'Delete view failed');
        }

        return res.send(JSON.stringify({
          success: true,
          text: 'Deleted view successfully'
        }));
      });
    }
  };

  /**
   * POST - /api/user/view/:name/toggleshare
   *
   * Toggles sharing an Arkime view for a user.
   * @name /user/view/:name/toggleshare
   * @returns {boolean} success - Whether the share view operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.userViewToggleShare = (req, res) => {
    const name = req.params.name || req.body.name;
    if (!name) {
      return res.molochError(403, 'Missing view name');
    }

    if (!req.body.expression) {
      return res.molochError(403, 'Missing view expression');
    }

    const share = req.body.shared;
    const user = req.settingUser;
    user.views = user.views || {};

    if (share && user.views[name] === undefined) {
      return res.molochError(404, 'View not found');
    }

    Db.getUser('_moloch_shared', (err, sharedUser) => {
      if (!sharedUser || !sharedUser.found) {
        // the shared user has not been created yet so there is no chance of duplicate views
        if (share) { // add the view to the shared user
          return shareView(req, res, user, '/api/user/views/toggleshare', 'Shared view successfully', 'Sharing view failed');
        }
        // if it not already a shared view and it's trying to be unshared, something went wrong, can't do it
        return res.molochError(404, 'Shared user not found. Cannot unshare a view without a shared user.');
      }

      sharedUser = sharedUser._source;
      sharedUser.views = sharedUser.views || {};

      if (share) { // if sharing, make sure the view doesn't already exist
        if (sharedUser.views[name]) { // duplicate detected
          return res.molochError(403, 'A shared view already exists with this name.');
        }
        return shareView(req, res, user, '/api/user/view/toggleshare', 'Shared view successfully', 'Sharing view failed');
      } else {
        // if unsharing, remove it from shared user and add it to current user
        if (sharedUser.views[name] === undefined) { return res.molochError(404, 'View not found'); }
        // only admins or the user that created the view can update the shared view
        if (!user.createEnabled && sharedUser.views[name].user !== user.userId) {
          return res.molochError(401, 'Need admin privelages to unshare another user\'s shared view');
        }
        // delete the shared view
        delete sharedUser.views[name];
        return unshareView(req, res, user, sharedUser, '/api/user/view/toggleshare', 'Unshared view successfully', 'Unsharing view failed');
      }
    });
  };

  /**
   * PUT - /api/user/view/:key
   *
   * Updates an Arkime view for a user.
   * @name /user/view/:key
   * @returns {boolean} success - Whether the update view operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.updateUserView = (req, res) => {
    const key = req.body.key || req.params.key;

    if (!key) {
      return res.molochError(403, 'Missing view key');
    }

    if (!req.body.name) {
      return res.molochError(403, 'Missing view name');
    }

    if (!req.body.expression) {
      return res.molochError(403, 'Missing view expression');
    }

    const user = req.settingUser;
    user.views = user.views || {};

    if (req.body.shared) {
      Db.getUser('_moloch_shared', (err, sharedUser) => {
        if (sharedUser && sharedUser.found) {
          sharedUser = sharedUser._source;
          sharedUser.views = sharedUser.views || {};
          if (sharedUser.views[key] === undefined) {
            return res.molochError(404, 'View not found');
          }
          // only admins or the user that created the view can update the shared view
          if (!user.createEnabled && sharedUser.views[req.body.name].user !== user.userId) {
            return res.molochError(401, 'Need admin privelages to update another user\'s shared view');
          }
          sharedUser.views[req.body.name] = {
            expression: req.body.expression,
            user: user.userId,
            shared: true,
            sessionsColConfig: req.body.sessionsColConfig
          };
          // delete the old one if the key (view name) has changed
          if (sharedUser.views[key] && req.body.name !== key) {
            sharedUser.views[key] = null;
            delete sharedUser.views[key];
          }
        }

        Db.setUser('_moloch_shared', sharedUser, (err, info) => {
          if (err) {
            console.log('/api/user/view update failed', err, info);
            return res.molochError(500, 'Update shared view failed');
          }

          return res.send(JSON.stringify({
            success: true,
            text: 'Updated shared view successfully'
          }));
        });
      });
    } else {
      if (user.views[req.body.name]) {
        user.views[req.body.name].expression = req.body.expression;
        user.views[req.body.name].sessionsColConfig = req.body.sessionsColConfig;
      } else { // the name has changed, so create a new entry
        user.views[req.body.name] = {
          expression: req.body.expression,
          user: user.userId,
          shared: false,
          sessionsColConfig: req.body.sessionsColConfig
        };
      }

      // delete the old one if the key (view name) has changed
      if (user.views[key] && req.body.name !== key) {
        user.views[key] = null;
        delete user.views[key];
      }

      Db.setUser(user.userId, user, (err, info) => {
        if (err) {
          console.log('/api/user/view update failed', err, info);
          return res.molochError(500, 'Updating view failed');
        }

        return res.send(JSON.stringify({
          success: true,
          text: 'Updated view successfully'
        }));
      });
    }
  };

  /**
   * GET - /api/user/crons
   *
   * Retrieves cron queries for a user.
   * @name /user/crons
   * @returns {object} queries - A list of cron query objects.
   */
  module.getUserCron = (req, res) => {
    if (!req.settingUser) {
      return res.molochError(403, 'Unknown user');
    }

    const user = req.settingUser;
    if (user.settings === undefined) { user.settings = {}; }

    const query = { size: 1000, query: { term: { creator: user.userId } } };

    Db.search('queries', 'query', query, (err, data) => {
      if (err || data.error) {
        console.log('/api/user/crons error', err || data.error);
      }

      const queries = {};

      if (data && data.hits && data.hits.hits) {
        data.hits.hits.forEach((item) => {
          queries[item._id] = item._source;
        });
      }

      res.send(queries);
    });
  };

  /**
   * POST - /api/user/cron
   *
   * Create a new cron query for a user.
   * @name /user/cron
   * @returns {boolean} success - Whether the create cron operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {string} key - The cron query id
   */
  module.createUserCron = (req, res) => {
    if (!req.body.name) {
      return res.molochError(403, 'Missing cron query name');
    }
    if (!req.body.query) {
      return res.molochError(403, 'Missing cron query expression');
    }
    if (!req.body.action) {
      return res.molochError(403, 'Missing cron query action');
    }
    if (!req.body.tags) {
      return res.molochError(403, 'Missing cron query tag(s)');
    }

    const document = {
      doc: {
        enabled: true,
        name: req.body.name,
        query: req.body.query,
        tags: req.body.tags,
        action: req.body.action
      }
    };

    if (req.body.notifier) {
      document.doc.notifier = req.body.notifier;
    }

    const userId = req.settingUser.userId;

    Db.getMinValue('sessions2-*', 'timestamp', (err, minTimestamp) => {
      if (err || minTimestamp === 0 || minTimestamp === null) {
        minTimestamp = Math.floor(Date.now() / 1000);
      } else {
        minTimestamp = Math.floor(minTimestamp / 1000);
      }

      if (+req.body.since === -1) {
        document.doc.lpValue = document.doc.lastRun = minTimestamp;
      } else {
        document.doc.lpValue = document.doc.lastRun =
           Math.max(minTimestamp, Math.floor(Date.now() / 1000) - 60 * 60 * parseInt(req.body.since || '0', 10));
      }

      document.doc.count = 0;
      document.doc.creator = userId || 'anonymous';

      Db.indexNow('queries', 'query', null, document.doc, (err, info) => {
        if (err) {
          console.log('/api/user/cron create error', err, info);
          return res.molochError(500, 'Create cron query failed');
        }

        if (Config.get('cronQueries', false)) {
          internals.processCronQueries();
        }

        return res.send(JSON.stringify({
          success: true,
          text: 'Created cron query successfully',
          key: info._id
        }));
      });
    });
  };

  /**
   * DELETE - /api/user/cron/:key
   *
   * Delete a cron query for a user.
   * @name /user/cron/:key
   * @returns {boolean} success - Whether the delete cron operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.deleteUserCron = (req, res) => {
    const key = req.body.key || req.params.key;
    if (!key) {
      return res.molochError(403, 'Missing cron query key');
    }

    Db.deleteDocument('queries', 'query', key, { refresh: true }, (err, sq) => {
      if (err) {
        console.log('/api/user/cron delete error', err, sq);
        return res.molochError(500, 'Delete cron query failed');
      }
      res.send(JSON.stringify({
        success: true,
        text: 'Deleted cron query successfully'
      }));
    });
  };

  /**
   * POST - /api/user/cron/:key
   *
   * Update a cron query for a user.
   * @name /user/cron/:key
   * @returns {boolean} success - Whether the update cron operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.updateUserCron = (req, res) => {
    const key = req.body.key || req.params.key;
    if (!key) {
      return res.molochError(403, 'Missing cron query key');
    }
    if (!req.body.name) {
      return res.molochError(403, 'Missing cron query name');
    }
    if (!req.body.query) {
      return res.molochError(403, 'Missing cron query expression');
    }
    if (!req.body.action) {
      return res.molochError(403, 'Missing cron query action');
    }
    if (!req.body.tags) {
      return res.molochError(403, 'Missing cron query tag(s)');
    }

    const document = {
      doc: {
        enabled: req.body.enabled,
        name: req.body.name,
        query: req.body.query,
        tags: req.body.tags,
        action: req.body.action,
        notifier: undefined
      }
    };

    if (req.body.notifier) {
      document.doc.notifier = req.body.notifier;
    }

    Db.get('queries', 'query', key, (err, sq) => {
      if (err || !sq.found) {
        console.log('/user/cron update failed', err, sq);
        return res.molochError(403, 'Unknown query');
      }

      Db.update('queries', 'query', key, document, { refresh: true }, (err, data) => {
        if (err) {
          console.log('/user/cron update error', err, document, data);
          return res.molochError(500, 'Cron query update failed');
        }

        if (Config.get('cronQueries', false)) {
          internals.processCronQueries();
        }

        return res.send(JSON.stringify({
          success: true,
          text: 'Updated cron query successfully'
        }));
      });
    });
  };

  /**
   * GET - /api/user/columns
   *
   * Retrieves user configured custom Sessions column configurations.
   * @name /user/columns
   * @returns {ArkimeColumnConfig[]} columnConfigs - The custom Sessions column configurations.
   */
  module.getUserColumns = (req, res) => {
    if (!req.settingUser) { return res.send([]); }

    // Fix for new names
    if (req.settingUser.columnConfigs) {
      for (const key in req.settingUser.columnConfigs) {
        const item = req.settingUser.columnConfigs[key];
        item.columns = item.columns.map(ViewerUtils.oldDB2newDB);
        if (item.order && item.order.length > 0) {
          item.order[0][0] = ViewerUtils.oldDB2newDB(item.order[0][0]);
        }
      }
    }

    return res.send(req.settingUser.columnConfigs || []);
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
  module.createUserColumns = (req, res) => {
    if (!req.body.name) {
      return res.molochError(403, 'Missing custom column configuration name');
    }
    if (!req.body.columns) {
      return res.molochError(403, 'Missing columns');
    }
    if (!req.body.order) {
      return res.molochError(403, 'Missing sort order');
    }

    req.body.name = req.body.name.replace(/[^-a-zA-Z0-9\s_:]/g, '');
    if (req.body.name.length < 1) {
      return res.molochError(403, 'Invalid custom column configuration name');
    }

    const user = req.settingUser;
    user.columnConfigs = user.columnConfigs || [];

    // don't let user use duplicate names
    for (const config of user.columnConfigs) {
      if (req.body.name === config.name) {
        return res.molochError(403, 'There is already a custom column with that name');
      }
    }

    user.columnConfigs.push({
      name: req.body.name,
      columns: req.body.columns,
      order: req.body.order
    });

    Db.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log('/api/user/column error', err, info);
        return res.molochError(500, 'Create custom column configuration failed');
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
  module.updateUserColumns = (req, res) => {
    const name = req.body.name || req.params.name;
    if (!name) {
      return res.molochError(403, 'Missing custom column configuration name');
    }
    if (!req.body.columns) {
      return res.molochError(403, 'Missing columns');
    }
    if (!req.body.order) {
      return res.molochError(403, 'Missing sort order');
    }

    const user = req.settingUser;
    user.columnConfigs = user.columnConfigs || [];

    // find the custom column configuration to update
    let found = false;
    for (let i = 0, len = user.columnConfigs.length; i < len; i++) {
      if (name === user.columnConfigs[i].name) {
        user.columnConfigs[i] = req.body;
        found = true;
        break;
      }
    }

    if (!found) {
      return res.molochError(200, 'Custom column configuration not found');
    }

    Db.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log('/api/user/column udpate error', err, info);
        return res.molochError(500, 'Update custom column configuration failed');
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
  module.deleteUserColumns = (req, res) => {
    const name = req.body.name || req.params.name;
    if (!name) {
      return res.molochError(403, 'Missing custom column configuration name');
    }

    const user = req.settingUser;
    user.columnConfigs = user.columnConfigs || [];

    let found = false;
    for (let i = 0, ilen = user.columnConfigs.length; i < ilen; ++i) {
      if (name === user.columnConfigs[i].name) {
        user.columnConfigs.splice(i, 1);
        found = true;
        break;
      }
    }

    if (!found) {
      return res.molochError(200, 'Custom column configuration not found');
    }

    Db.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log('/api/user/column delete error', err, info);
        return res.molochError(500, 'Delete custom column configuration failed');
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
  module.getUserSpiviewFields = (req, res) => {
    if (!req.settingUser) { return res.send([]); }

    return res.send(req.settingUser.spiviewFieldConfigs || []);
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
  module.createUserSpiviewFields = (req, res) => {
    if (!req.body.name) {
      return res.molochError(403, 'Missing custom SPI View fields configuration name');
    }
    if (!req.body.fields) {
      return res.molochError(403, 'Missing fields');
    }

    req.body.name = req.body.name.replace(/[^-a-zA-Z0-9\s_:]/g, '');

    if (req.body.name.length < 1) {
      return res.molochError(403, 'Invalid custom SPI View fields configuration name');
    }

    const user = req.settingUser;
    user.spiviewFieldConfigs = user.spiviewFieldConfigs || [];

    // don't let user use duplicate names
    for (const config of user.spiviewFieldConfigs) {
      if (req.body.name === config.name) {
        return res.molochError(403, 'There is already a custom SPI View fieldss configuration with that name');
      }
    }

    user.spiviewFieldConfigs.push({
      name: req.body.name,
      fields: req.body.fields
    });

    Db.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log('/api/user/spiview create error', err, info);
        return res.molochError(500, 'Create custom SPI View fields configuration failed');
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
  module.updateUserSpiviewFields = (req, res) => {
    const name = req.body.name || req.params.name;
    if (!name) {
      return res.molochError(403, 'Missing custom SPI View fields configuration name');
    }
    if (!req.body.fields) {
      return res.molochError(403, 'Missing fields');
    }

    const user = req.settingUser;
    user.spiviewFieldConfigs = user.spiviewFieldConfigs || [];

    // find the custom SPI View fields configuration to update
    let found = false;
    for (let i = 0, len = user.spiviewFieldConfigs.length; i < len; i++) {
      if (name === user.spiviewFieldConfigs[i].name) {
        user.spiviewFieldConfigs[i] = req.body;
        found = true;
        break;
      }
    }

    if (!found) {
      return res.molochError(200, 'Custom SPI View fields configuration not found');
    }

    Db.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log('/api/user/spiview udpate error', err, info);
        return res.molochError(500, 'Update SPI View fields configuration failed');
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
  module.deleteUserSpiviewFields = (req, res) => {
    const name = req.params.name || req.body.name;
    if (!name) {
      return res.molochError(403, 'Missing custom SPI View fields configuration name');
    }

    const user = req.settingUser;
    user.spiviewFieldConfigs = user.spiviewFieldConfigs || [];

    let found = false;
    for (let i = 0, ilen = user.spiviewFieldConfigs.length; i < ilen; ++i) {
      if (name === user.spiviewFieldConfigs[i].name) {
        user.spiviewFieldConfigs.splice(i, 1);
        found = true;
        break;
      }
    }

    if (!found) {
      return res.molochError(200, 'SPI View fields not found');
    }

    Db.setUser(user.userId, user, (err, info) => {
      if (err) {
        console.log('/api/user/spiview delete failed', err, info);
        return res.molochError(500, 'Delete custom SPI View fields configuration failed');
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
  module.acknowledgeMsg = (req, res) => {
    if (!req.body.msgNum) {
      return res.molochError(403, 'Message number required');
    }

    if (req.params.userId !== req.user.userId) {
      return res.molochError(403, 'Can not change other users msg');
    }

    Db.getUser(req.params.userId, (err, user) => {
      if (err || !user.found) {
        console.log('update user failed', err, user);
        return res.molochError(403, 'User not found');
      }

      user = user._source;

      user.welcomeMsgNum = parseInt(req.body.msgNum);

      Db.setUser(req.params.userId, user, (err, info) => {
        if (Config.debug) {
          console.log('setUser', user, err, info);
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
  module.getUserState = (req, res) => {
    if (!req.user.tableStates || !req.user.tableStates[req.params.name]) {
      return res.send('{}');
    }

    // Fix for new names
    if (req.params.name === 'sessionsNew' && req.user.tableStates && req.user.tableStates.sessionsNew) {
      const item = req.user.tableStates.sessionsNew;
      if (item.visibleHeaders) {
        item.visibleHeaders = item.visibleHeaders.map(ViewerUtils.oldDB2newDB);
      }
      if (item.order && item.order.length > 0) {
        item.order[0][0] = ViewerUtils.oldDB2newDB(item.order[0][0]);
      }
    }

    return res.send(req.user.tableStates[req.params.name]);
  };

  /**
   * POST - /api/user/state/:name
   *
   * Updates or creates a user table state object. These are used to save the states of tables within the UI (sessions, files, stats, etc).
   * @name /user/state/:name
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.updateUserState = (req, res) => {
    Db.getUser(req.user.userId, (err, user) => {
      if (err || !user.found) {
        console.log('save state failed', err, user);
        return res.molochError(403, 'Unknown user');
      }

      user = user._source;

      if (!user.tableStates) {
        user.tableStates = {};
      }

      user.tableStates[req.params.name] = req.body;

      Db.setUser(user.userId, user, (err, info) => {
        if (err) {
          console.log('state error', err, info);
          return res.molochError(403, 'state update failed');
        }

        return res.send(JSON.stringify({
          success: true,
          text: 'updated state successfully'
        }));
      });
    });
  };

  return module;
};
