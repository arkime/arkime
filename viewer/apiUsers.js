/******************************************************************************/
/* apiUsers.js -- api calls for users tab
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const Config = require('./config.js');
const fs = require('fs');
const util = require('util');
const stylus = require('stylus');
const User = require('../common/user');
const ArkimeUtil = require('../common/arkimeUtil');
const internals = require('./internals');
const ViewerUtils = require('./viewerUtils');

class UserAPIs {
  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------
  static #userColumns (user) {
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

    return user.columnConfigs ?? [];
  }

  // --------------------------------------------------------------------------
  static #userInfoFields (user) {
    return user?.infoFieldConfigs ?? [];
  }

  // --------------------------------------------------------------------------
  static #userSpiview (user) {
    return user?.spiviewFieldConfigs ?? [];
  }

  // --------------------------------------------------------------------------
  static getCurrentUserCB (user, clone) {
    clone.canUpload = internals.allowUploads && user.hasRole(internals.uploadRoles);

    // If esAdminUser is set use that, otherwise use arkimeAdmin privilege
    if (internals.esAdminUsersSet) {
      clone.esAdminUser = internals.esAdminUsers.includes(user.userId);
    } else {
      clone.esAdminUser = user.hasRole('arkimeAdmin');
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

  // --------------------------------------------------------------------------
  static findUserState (stateName, user) {
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

  /**
   * validate and set the session column layout on the user
   *
   * @param {object} req - The request object.
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {string} name - The name of the new layout.
   */
  static #validateSessionColumnLayout (req) {
    if (!ArkimeUtil.isString(req.body.name)) {
      return { success: false, text: 'Missing name' };
    }
    if (!req.body.columns) {
      return { success: false, text: 'Missing columns' };
    }
    if (!req.body.order) {
      return { success: false, text: 'Missing sort order' };
    }

    const layoutName = req.body.name.replace(/[^-a-zA-Z0-9\s_:]/g, '');
    if (layoutName.length < 1) {
      return { success: false, text: 'Invalid name' };
    }

    return { layoutName, success: true };
  }

  /**
   * validate and set the session info field layout on the user
   *
   * @param {object} req - The request object.
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {string} name - The name of the new layout.
   */
  static #validateInfoFieldLayout (req) {
    if (!ArkimeUtil.isString(req.body.name)) {
      return { success: false, text: 'Missing name' };
    }
    if (!req.body.fields) {
      return { success: false, text: 'Missing fields' };
    }

    const layoutName = req.body.name.replace(/[^-a-zA-Z0-9\s_:]/g, '');
    if (layoutName.length < 1) {
      return { success: false, text: 'Invalid name' };
    }

    return { layoutName, success: true };
  }

  /**
   * validate and set the SPIView layout on the user
   *
   * @param {object} req - The request object.
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {string} name - The name of the new layout (if successful).
   */
  static #validateSPIViewLayout (req) {
    if (!ArkimeUtil.isString(req.body.name)) {
      return { success: false, text: 'Missing name' };
    }
    if (!req.body.fields) {
      return { success: false, text: 'Missing fields' };
    }

    const layoutName = req.body.name.replace(/[^-a-zA-Z0-9\s_:]/g, '');

    if (layoutName.length < 1) {
      return { success: false, text: 'Invalid name' };
    }

    return { layoutName, success: true };
  }

  /**
   * Validate and set the info field layout on the user.
   *
   * @param {object} req - The request object.
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {string} name - The name of the new layout.
   * @returns {object} user - The updated user object.
   */
  static #setInfoFieldLayout (req) {
    const result = UserAPIs.#validateInfoFieldLayout(req);
    if (!result.success) {
      return result;
    }

    const user = req.settingUser;
    user.infoFieldConfigs = user.infoFieldConfigs ?? [];

    // don't let user use duplicate names
    for (const config of user.infoFieldConfigs) {
      if (result.layoutName === config.name) {
        return { success: false, text: 'Duplicate name' };
      }
    }

    user.infoFieldConfigs.push({
      name: result.layoutName,
      fields: req.body.fields
    });

    return { user, layoutName: result.layoutName, success: true };
  };

  /**
   * Validate and set the sessions column layout on the user.
   *
   * @param {object} req - The request object.
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {string} name - The name of the new layout.
   * @returns {object} user - The updated user object.
   */
  static #setSessionColumnLayout (req) {
    const result = UserAPIs.#validateSessionColumnLayout(req);
    if (!result.success) {
      return result;
    }

    const user = req.settingUser;
    user.columnConfigs = user.columnConfigs ?? [];

    // don't let user use duplicate names
    for (const config of user.columnConfigs) {
      if (result.layoutName === config.name) {
        return { success: false, text: 'Duplicate name' };
      }
    }

    user.columnConfigs.push({
      order: req.body.order,
      name: result.layoutName,
      columns: req.body.columns
    });

    return { user, layoutName: result.layoutName, success: true };
  };

  /**
   * Validate and set the SPIView layout on the user.
   *
   * @param {object} req - The request object.
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {string} name - The name of the new layout (if successful).
   * @returns {object} user - The updated user object (if successful).
   */
  static #setSPIViewLayout (req) {
    const result = UserAPIs.#validateSPIViewLayout(req);
    if (!result.success) {
      return result;
    }

    const user = req.settingUser;
    user.spiviewFieldConfigs ??= [];

    // don't let user use duplicate names
    for (const config of user.spiviewFieldConfigs) {
      if (result.layoutName === config.name) {
        return { success: false, text: 'Duplicate name' };
      }
    }

    user.spiviewFieldConfigs.push({
      name: result.layoutName,
      fields: req.body.fields
    });

    return { user, layoutName: result.layoutName, success: true };
  }

  /**
   * Validate and update the sessions column layout on the user.
   *
   * @param {object} req - The request object.
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static #updateSessionColumnLayout (req) {
    const result = UserAPIs.#validateSessionColumnLayout(req);
    if (!result.success) {
      return result;
    }

    const user = req.settingUser;
    user.columnConfigs = user.columnConfigs ?? [];

    // find the custom column configuration to update
    for (let i = 0, len = user.columnConfigs.length; i < len; i++) {
      if (result.layoutName === user.columnConfigs[i].name) {
        user.columnConfigs[i] = req.body;
        return { success: true, layout: req.body, user };
      }
    }

    return { success: false, text: 'Layout not found' };
  };

  /**
   * Validate and update the info field layout on the user.
   *
   * @param {object} req - The request object.
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static #updateInfoFieldLayout (req, res) {
    const result = UserAPIs.#validateInfoFieldLayout(req);
    if (!result.success) {
      return result;
    }

    const user = req.settingUser;
    user.infoFieldConfigs = user.infoFieldConfigs ?? [];

    // find the custom column configuration to update
    for (let i = 0, len = user.infoFieldConfigs.length; i < len; i++) {
      if (result.layoutName === user.infoFieldConfigs[i].name) {
        user.infoFieldConfigs[i] = req.body;
        return { success: true, layout: req.body, user };
      }
    }

    return { success: false, text: 'Layout not found' };
  };

  /**
   * Validate and update the SPIView layout on the user.
   *
   * @param {object} req - The request object.
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {object} layout - The updated layout (if successful).
   */
  static #updateSPIViewLayout (req) {
    const result = UserAPIs.#validateSPIViewLayout(req);
    if (!result.success) {
      return result;
    }

    const user = req.settingUser;
    user.spiviewFieldConfigs = user.spiviewFieldConfigs ?? [];

    // find the custom spiview layout to update
    for (let i = 0, len = user.spiviewFieldConfigs.length; i < len; i++) {
      if (result.layoutName === user.spiviewFieldConfigs[i].name) {
        user.spiviewFieldConfigs[i] = req.body;
        return { success: true, layout: req.body, user };
      }
    }

    return { success: false, text: 'Layout not found' };
  }

  /**
   * Delete a specified layout from the user object.
   *
   * @param {string} layoutKey - The key on the user of the layout type to delete.
   * @param {object} req - The request object.
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {object} user - The updated user object (if successful).
   */
  static #deleteLayout (layoutKey, req) {
    const user = req.settingUser;
    user[layoutKey] = user[layoutKey] ?? [];

    for (let i = 0, ilen = user[layoutKey].length; i < ilen; ++i) {
      if (req.params.name === user[layoutKey][i].name) {
        user[layoutKey].splice(i, 1);
        return { success: true, user };
      }
    }

    return { success: false, text: 'Layout not found' };
  }

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
   * @param {object} notifiers - A list of notifiers that the user can use.
   * @param {object} columnConfigs - A list of sessions table column configurations that a user has created.
   * @param {object} spiviewFieldConfigs - A list of SPIView page field configurations that a user has created.
   * @param {object} tableStates - A list of table states used to render Arkime tables as the user has configured them.
   * @param {number} welcomeMsgNum=0 - The message number that a user is on. Gets incremented when a user dismisses a message.
   * @param {number} lastUsed - The date that the user last used Arkime. Format is milliseconds since Unix EPOC.
   * @param {number} timeLimit - Limits the time range a user can query for.
   * @param {array} roles - The list of Arkime roles assigned to this user.
   * @param {array} roleAssigners - The list of userIds that can manage who has this (ROLE)
   */

  /**
   * The settings object.
   *
   * @typedef ArkimeSettings
   * @type {object}
   * @param {string} timezone=local - The timezone applied to timestamps within the UI.
   * @param {string} detailFormat=last - The format to display the session packets. Options include: last used, natural, ascii, utf-8, hex.
   * @param {string} showTimestamps=last - Whether to display timestamps at the top of each packet.
   * @param {string} sortColumn=firstPacket - Which column to sort the sessions table by default. Default is start time.
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
   * @typedef ArkimeColumnLayout
   * @type {object}
   * @param {string} name - The name of the column configuration.
   * @param {Array[]} order=[["firstPacket","desc"]] - What to sort the Sessions table by. The table is sorted by the first item in the array first, then the second, and so on. Each element in the array includes first the sort field followed by whether to sort descending (["firstPacket", "desc"]).
   * @param {Array} visibleHeaders=["firstPacket","lastPacket","src","source.port","dst","destination.port","network.packets","dbby","node"] - The list of Sessions table columns.
   */

  /**
   * A sessions info view that can be applied.
   *
   * @typedef ArkimeInfoColumnLayout
   * @type {object}
   * @param {string} name - The name of the info column configuration.
   * @param {Array} fields=["firstPacket","lastPacket","src","source.port","dst","destination.port","network.packets","dbby","node"] - The list of Sessions table columns.
   */

  // --------------------------------------------------------------------------
  /**
   * GET - /api/user/css OR /api/user.css
   *
   * Retrieves custom user css for the user's custom theme.
   * @name /user/css
   * @returns {css} css - The css file that includes user configured styles.
   */
  static getUserCSS (req, res) {
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

  // USER SETTINGS --------------------------------------------------------------------------
  /**
   * GET - /api/user/settings
   *
   * Retrieves an Arkime user's settings.
   * @name /user/settings
   * @returns {ArkimeSettings} settings - The user's configured settings
   */
  static getUserSettings (req, res) {
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
  static updateUserSettings (req, res) {
    req.settingUser.settings = (
      ({ // only allow these properties in the settings
        logo, theme, timezone, spiGraph, numPackets, infoFields, manualQuery, detailFormat,
        connSrcField, connDstField, sortDirection, showTimestamps, connNodeFields,
        connLinkFields, timelineDataFilters
      }) => ({ logo, theme, timezone, spiGraph, numPackets, infoFields, manualQuery, detailFormat, connSrcField, connDstField, sortDirection, showTimestamps, connNodeFields, connLinkFields, timelineDataFilters })
    )(req.body);

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

  // LAYOUTS --------------------------------------------------------------------------
  /**
   * GET - /api/user/layouts/:type
   *
   * Retrieves a user configured layouts.
   * Valid layouts are: sessionstable, sessionsinfofields, spiview
   * @name /user/layouts/:type
   * @returns {Array} layout - The user configured layout
   */
  static getUserLayouts (req, res) {
    if (req.params.type === 'sessionstable') {
      return res.send(UserAPIs.#userColumns(req.settingUser));
    }
    if (req.params.type === 'sessionsinfofields') {
      return res.send(UserAPIs.#userInfoFields(req.settingUser));
    }
    if (req.params.type === 'spiview') {
      return res.send(UserAPIs.#userSpiview(req.settingUser));
    }
    return res.send([]);
  }

  /**
   * POST - /api/user/layouts/:type
   *
   * Creates a new user configured layout.
   * Valid layouts are: sessionstable, sessionsinfofields, spiview
   * @name /user/layouts/:type
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {object} layout - The new layout configuration.
   */
  static createUserLayout (req, res) {
    let result;

    switch (req.params.type) {
    case 'sessionstable':
      result = UserAPIs.#setSessionColumnLayout(req);
      break;
    case 'sessionsinfofields':
      result = UserAPIs.#setInfoFieldLayout(req);
      break;
    case 'spiview':
      result = UserAPIs.#setSPIViewLayout(req);
      break;
    default:
      res.serverError(403, 'Invalid layout type');
    }

    if (!result.success) {
      return res.serverError(403, result.text);
    }

    User.setUser(result.user.userId, result.user, (err, info) => {
      if (err) {
        console.log(`ERROR - ${req.method} /api/user/layouts/${req.params.type}`, util.inspect(err, false, 50), info);
        return res.serverError(500, 'Create layout failed');
      }

      return res.send(JSON.stringify({
        success: true,
        name: result.layoutName,
        text: 'Created layout successfully'
      }));
    });
  }

  /**
   * PUT - /api/user/layouts/:type
   *
   * Updates a user configured layout.
   * Valid layouts are: sessionstable, sessionsinfofields, spiview
   * @name /user/layouts/:type
   * @returns {boolean} success - Whether the update layout operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   * @returns {object} layout - The updated layout configuration.
   */
  static updateUserLayout (req, res) {
    let result;

    switch (req.params.type) {
    case 'sessionstable':
      result = UserAPIs.#updateSessionColumnLayout(req);
      break;
    case 'sessionsinfofields':
      result = UserAPIs.#updateInfoFieldLayout(req);
      break;
    case 'spiview':
      result = UserAPIs.#updateSPIViewLayout(req);
      break;
    default:
      res.serverError(403, 'Invalid layout type');
    }

    if (!result.success) {
      return res.serverError(403, result.text);
    }

    User.setUser(result.user.userId, result.user, (err, info) => {
      if (err) {
        console.log(`ERROR - ${req.method} /api/user/info/${req.params.type}`, util.inspect(err, false, 50), info);
        return res.serverError(500, 'Update layout failed');
      }

      return res.send(JSON.stringify({
        success: true,
        layout: req.body,
        text: 'Updated layout'
      }));
    });
  }

  /**
   * DELETE - /api/user/layouts/:type/:name
   *
   * Deletes a user configured layout.
   * Valid layouts are: sessionstable, sessionsinfofields, spiview
   * @name /user/layouts/:type/:name
   * @returns {boolean} success - Whether the delete layout operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static deleteUserLayout (req, res) {
    let layoutKey;

    switch (req.params.type) {
    case 'sessionstable':
      layoutKey = 'columnConfigs';
      break;
    case 'sessionsinfofields':
      layoutKey = 'infoFieldConfigs';
      break;
    case 'spiview':
      layoutKey = 'spiviewFieldConfigs';
      break;
    default:
      res.serverError(403, 'Invalid layout type');
    }

    const result = UserAPIs.#deleteLayout(layoutKey, req);

    if (!result.success) {
      return res.serverError(403, result.text);
    }

    User.setUser(result.user.userId, result.user, (err, info) => {
      if (err) {
        console.log(`ERROR - ${req.method} /api/user/layouts/${req.params.type}/${req.params.name}`, util.inspect(err, false, 50), info);
        return res.serverError(500, 'Delete layout failed');
      }

      return res.send(JSON.stringify({
        success: true,
        text: 'Deleted layout successfully'
      }));
    });
  }

  // --------------------------------------------------------------------------
  /**
   * PUT - /api/user/:userId/acknowledge
   *
   * Acknowledges a UI message for a user. Used to display help popups.
   * @name /user/:userId/acknowledge
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static acknowledgeMsg (req, res) {
    if (!req.body.msgNum) {
      return res.serverError(403, 'Message number required');
    }

    if (req.params.userId !== req.user.userId) {
      return res.serverError(403, 'Can not change other users msg');
    }

    User.getUser(req.params.userId, (err, user) => {
      if (err || !user) {
        console.log(`ERROR - ${req.method} /api/user/%s/acknowledge (getUser)`, ArkimeUtil.sanitizeStr(req.params.userId), util.inspect(err, false, 50), user);
        return res.serverError(403, 'User not found');
      }

      user.welcomeMsgNum = parseInt(req.body.msgNum);
      if (!Number.isInteger(user.welcomeMsgNum)) {
        return res.serverError(403, 'welcomeMsgNum is not integer');
      }

      User.setUser(req.params.userId, user, (err, info) => {
        if (Config.debug) {
          console.log(`ERROR - ${req.method} /api/user/%s/acknowledge (setUser)`, ArkimeUtil.sanitizeStr(req.params.userId), util.inspect(err, false, 50), user, info);
        }

        return res.send(JSON.stringify({
          success: true,
          text: `User, ${req.user.userId}, dismissed message ${user.welcomeMsgNum}`
        }));
      });
    });
  };

  // USER STATE --------------------------------------------------------------------------
  /**
   * GET - /api/user/state/:name
   *
   * Retrieves a user table state object. These are used to save the states of tables within the UI (sessions, files, stats, etc).
   * @name /user/state/:name
   * @returns {object} tableState - The table state requested.
   */
  static getUserState (req, res) {
    return res.send(UserAPIs.findUserState(req.params.name, req.user));
  };

  /**
   * POST - /api/user/state/:name
   *
   * Updates or creates a user table state object. These are used to save the states of tables within the UI (sessions, files, stats, etc).
   * @name /user/state/:name
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static updateUserState (req, res) {
    User.getUser(req.user.userId, (err, user) => {
      if (err || !user) {
        console.log(`ERROR - ${req.method} /api/user/state/%s (getUser)`, ArkimeUtil.sanitizeStr(req.params.name), util.inspect(err, false, 50), user);
        return res.serverError(403, 'Unknown user');
      }

      if (!user.tableStates) {
        user.tableStates = {};
      }

      user.tableStates[req.params.name] = req.body;

      User.setUser(user.userId, user, (err, info) => {
        if (err) {
          console.log(`ERROR - ${req.method} /api/user/state/%s (setUser)`, ArkimeUtil.sanitizeStr(req.params.name), util.inspect(err, false, 50), info);
          return res.serverError(403, 'state update failed');
        }

        return res.send(JSON.stringify({
          success: true,
          text: 'updated state successfully'
        }));
      });
    });
  };

  // --------------------------------------------------------------------------
  /**
   * GET - /api/user/config/:page
   *
   * Fetches the configuration/layout information for a UI page for a user.
   * @name /user/config/:page
   * @returns {object} config The configuration data for the page
   */
  static getPageConfig (req, res) {
    switch (req.params.page) {
    case 'sessions': {
      const colConfigs = UserAPIs.#userColumns(req.settingUser);
      const infoConfigs = UserAPIs.#userInfoFields(req.settingUser);
      const tableState = UserAPIs.findUserState('sessionsNew', req.user);
      const colWidths = UserAPIs.findUserState('sessionsColWidths', req.user);
      return res.send({ colWidths, tableState, colConfigs, infoConfigs });
    }
    case 'spiview': {
      const fieldConfigs = UserAPIs.#userSpiview(req.settingUser);
      const spiviewFields = UserAPIs.findUserState('spiview', req.user);
      return res.send({ fieldConfigs, spiviewFields });
    }
    case 'connections': {
      const fieldHistoryConnectionsSrc = UserAPIs.findUserState('fieldHistoryConnectionsSrc', req.user);
      const fieldHistoryConnectionsDst = UserAPIs.findUserState('fieldHistoryConnectionsDst', req.user);
      return res.send({ fieldHistoryConnectionsSrc, fieldHistoryConnectionsDst });
    }
    case 'files': {
      const tableState = UserAPIs.findUserState('fieldsCols', req.user);
      const columnWidths = UserAPIs.findUserState('filesColWidths', req.user);
      return res.send({ tableState, columnWidths });
    }
    case 'captureStats': {
      const tableState = UserAPIs.findUserState('captureStatsCols', req.user);
      const columnWidths = UserAPIs.findUserState('captureStatsColWidths', req.user);
      return res.send({ tableState, columnWidths });
    }
    case 'esIndices': {
      const tableState = UserAPIs.findUserState('esIndicesCols', req.user);
      const columnWidths = UserAPIs.findUserState('esIndicesColWidths', req.user);
      return res.send({ tableState, columnWidths });
    }
    case 'esNodes': {
      const tableState = UserAPIs.findUserState('esNodesCols', req.user);
      const columnWidths = UserAPIs.findUserState('esNodesColWidths', req.user);
      return res.send({ tableState, columnWidths });
    }
    case 'esRecovery': {
      const tableState = UserAPIs.findUserState('esRecoveryCols', req.user);
      const columnWidths = UserAPIs.findUserState('esRecoveryColWidths', req.user);
      return res.send({ tableState, columnWidths });
    }
    case 'esTasks': {
      const tableState = UserAPIs.findUserState('esTasksCols', req.user);
      const columnWidths = UserAPIs.findUserState('esTasksColWidths', req.user);
      return res.send({ tableState, columnWidths });
    }
    default:
      return res.serverError(501, 'Requested page is not supported');
    }
  };
};

module.exports = UserAPIs;
