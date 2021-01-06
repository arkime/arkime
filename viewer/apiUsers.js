'use strict';

const fs = require('fs');
const stylus = require('stylus');

module.exports = (app, Config, Db, internals, ViewerUtils) => {
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
   * DELETE - /api/user
   *
   * Deletes an Arkime user (admin only).
   * @name /user
   * @returns {boolean} success - Whether the delete user operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.deleteUser = (req, res) => {
    if (req.body.userId === req.user.userId) {
      return res.molochError(403, 'Can not delete yourself');
    }

    Db.deleteUser(req.body.userId, (err, data) => {
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

      style.render((err, css) => {
        if (err) { return error(err); }
        return res.send(css);
      });
    });
  };

  /**
   * GET - /api/users
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

    const cookieOptions = { path: app.locals.basePath, sameSite: 'Strict' };
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
        console.log('/user/settings/update error', err, info);
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
   * @returns {Array} views - A list of views a user has configured or has been shared.
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
   * @returns {object} view - The new view data.
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
      saveSharedView(req, res, user, newView, '/user/views/create', 'Created shared view successfully', 'Create shared view failed');
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
          console.log('/api/user/views/create error', err, info);
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
   * DELETE - /api/user/view
   *
   * Deletes an Arkime view for a user.
   * @name /user/view
   * @returns {boolean} success - Whether the delete view operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.deleteUserView = (req, res) => {
    if (!req.body.name) { return res.molochError(403, 'Missing view name'); }

    const user = req.settingUser;
    user.views = user.views || {};

    if (req.body.shared) {
      Db.getUser('_moloch_shared', (err, sharedUser) => {
        if (sharedUser && sharedUser.found) {
          sharedUser = sharedUser._source;
          sharedUser.views = sharedUser.views || {};
          if (sharedUser.views[req.body.name] === undefined) { return res.molochError(404, 'View not found'); }
          // only admins or the user that created the view can delete the shared view
          if (!user.createEnabled && sharedUser.views[req.body.name].user !== user.userId) {
            return res.molochError(401, `Need admin privelages to delete another user's shared view`);
          }
          delete sharedUser.views[req.body.name];
        }

        Db.setUser('_moloch_shared', sharedUser, (err, info) => {
          if (err) {
            console.log('/user/views/delete failed', err, info);
            return res.molochError(500, 'Delete shared view failed');
          }
          return res.send(JSON.stringify({
            success: true,
            text: 'Deleted shared view successfully'
          }));
        });
      });
    } else {
      if (user.views[req.body.name] === undefined) { return res.molochError(404, 'View not found'); }
      delete user.views[req.body.name];

      Db.setUser(user.userId, user, (err, info) => {
        if (err) {
          console.log('/user/views/delete failed', err, info);
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
   * POST - /api/user/view/toggleshare
   *
   * Toggles sharing an Arkime view for a user.
   * @name /user/view/toggleshare
   * @returns {boolean} success - Whether the share view operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.userViewToggleShare = (req, res) => {
    if (!req.body.name) {
      return res.molochError(403, 'Missing view name');
    }

    if (!req.body.expression) {
      return res.molochError(403, 'Missing view expression');
    }

    const share = req.body.shared;
    const user = req.settingUser;
    user.views = user.views || {};

    if (share && user.views[req.body.name] === undefined) {
      return res.molochError(404, 'View not found');
    }

    Db.getUser('_moloch_shared', (err, sharedUser) => {
      if (!sharedUser || !sharedUser.found) {
        // the shared user has not been created yet so there is no chance of duplicate views
        if (share) { // add the view to the shared user
          return shareView(req, res, user, '/user/views/toggleShare', 'Shared view successfully', 'Sharing view failed');
        }
        // if it not already a shared view and it's trying to be unshared, something went wrong, can't do it
        return res.molochError(404, 'Shared user not found. Cannot unshare a view without a shared user.');
      }

      sharedUser = sharedUser._source;
      sharedUser.views = sharedUser.views || {};

      if (share) { // if sharing, make sure the view doesn't already exist
        if (sharedUser.views[req.body.name]) { // duplicate detected
          return res.molochError(403, 'A shared view already exists with this name.');
        }
        return shareView(req, res, user, '/api/user/view/toggleshare', 'Shared view successfully', 'Sharing view failed');
      } else {
        // if unsharing, remove it from shared user and add it to current user
        if (sharedUser.views[req.body.name] === undefined) { return res.molochError(404, 'View not found'); }
        // only admins or the user that created the view can update the shared view
        if (!user.createEnabled && sharedUser.views[req.body.name].user !== user.userId) {
          return res.molochError(401, `Need admin privelages to unshare another user's shared view`);
        }
        // delete the shared view
        delete sharedUser.views[req.body.name];
        return unshareView(req, res, user, sharedUser, '/api/user/view/toggleshare', 'Unshared view successfully', 'Unsharing view failed');
      }
    });
  };

  /**
   * POST - /api/user/view
   *
   * Updates an Arkime view for a user.
   * @name /user/view
   * @returns {boolean} success - Whether the update view operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.updateUserView = (req, res) => {
    if (!req.body.name) {
      return res.molochError(403, 'Missing view name');
    }

    if (!req.body.expression) {
      return res.molochError(403, 'Missing view expression');
    }

    if (!req.body.key) {
      return res.molochError(403, 'Missing view key');
    }

    const user = req.settingUser;
    user.views = user.views || {};

    if (req.body.shared) {
      Db.getUser('_moloch_shared', (err, sharedUser) => {
        if (sharedUser && sharedUser.found) {
          sharedUser = sharedUser._source;
          sharedUser.views = sharedUser.views || {};
          if (sharedUser.views[req.body.key] === undefined) {
            return res.molochError(404, 'View not found');
          }
          // only admins or the user that created the view can update the shared view
          if (!user.createEnabled && sharedUser.views[req.body.name].user !== user.userId) {
            return res.molochError(401, `Need admin privelages to update another user's shared view`);
          }
          sharedUser.views[req.body.name] = {
            expression: req.body.expression,
            user: user.userId,
            shared: true,
            sessionsColConfig: req.body.sessionsColConfig
          };
          // delete the old one if the key (view name) has changed
          if (sharedUser.views[req.body.key] && req.body.name !== req.body.key) {
            sharedUser.views[req.body.key] = null;
            delete sharedUser.views[req.body.key];
          }
        }

        Db.setUser('_moloch_shared', sharedUser, (err, info) => {
          if (err) {
            console.log('/api/user/view failed', err, info);
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
      if (user.views[req.body.key] && req.body.name !== req.body.key) {
        user.views[req.body.key] = null;
        delete user.views[req.body.key];
      }

      Db.setUser(user.userId, user, (err, info) => {
        if (err) {
          console.log('/api/user/view error', err, info);
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
        console.log('/api/user/cron error', err || data.error);
      }

      const queries = {};

      if (data && data.hits && data.hits.hits) {
        data.hits.hits.forEach(function (item) {
          queries[item._id] = item._source;
        });
      }

      res.send(queries);
    });
  };

  /**
   * POSt - /api/user/cron
   *
   * Create a new cron query for a user.
   * @name /user/cron
   * @returns {boolean} success - Whether the add user operation was successful.
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
          console.log('/user/cron/create error', err, info);
          return res.molochError(500, 'Create cron query failed');
        }

        if (Config.get('cronQueries', false)) {
          ViewerUtils.processCronQueries();
        }

        return res.send(JSON.stringify({
          success: true,
          text: 'Created cron query successfully',
          key: info._id
        }));
      });
    });
  };

  return module;
};
