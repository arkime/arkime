/******************************************************************************/
/* apiUsers.js -- api calls for users tab
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const Config = require('./config.js');
const util = require('util');
const User = require('../common/user');
const ArkimeUtil = require('../common/arkimeUtil');
const internals = require('./internals');
const ViewerUtils = require('./viewerUtils');

class UserAPIs {
  static getCurrentUserCB (user, clone) {
    clone.canUpload = internals.allowUploads && user.hasRole(internals.uploadRoles);

    // If no settings, use defaults
    if (clone.settings === undefined) { clone.settings = internals.settingDefaults; }

    // Use settingsDefaults for any settings that are missing
    for (const item in internals.settingDefaults) {
      if (clone.settings[item] === undefined) {
        clone.settings[item] = internals.settingDefaults[item];
      }
    }

    return clone;
  }

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
  }
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
  }

  // Allowlist of viewer settings keys persisted via POST /api/user/settings.
  // customTheme + vuetifyCustomTheme are the structured { dark, colors }
  // records; customTheme stays in the list for legacy arkime compat (older
  // versions still write it), while vuetifyTheme / vuetifyCustomTheme is
  // where v7+ persists its preference -- the two stay independent across
  // version round-trips.
  static #settingsAllowlist = ['ms', 'logo', 'theme', 'customTheme', ...User.USER_SETTINGS_KEYS, 'timezone', 'spiGraph', 'numPackets', 'infoFields', 'manualQuery', 'detailFormat',
    'connSrcField', 'connDstField', 'sortColumn', 'sortDirection', 'showTimestamps', 'connNodeFields',
    'connLinkFields', 'timelineDataFilters', 'hideTags', 'shiftyEyes', 'defaultDashboardId'];

  /**
   * POST - /api/user/settings
   *
   * Updates an Arkime user's settings.
   * @name /user/settings
   * @returns {boolean} success - Whether the update user settings operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static updateUserSettings = User.apiUpdateSettingsHandler(
    UserAPIs.#settingsAllowlist,
    ['customTheme', ...User.USER_SETTINGS_OBJECT_KEYS]
  );
  static acknowledgeMsg (req, res) {
    const { noteId } = req.body;

    if (noteId === undefined && !req.body.msgNum) {
      return res.serverError(403, 'Message number required', 'api.users.messageNumberRequired');
    }

    if (noteId !== undefined && (!ArkimeUtil.isString(noteId) || !/^[a-z0-9]{1,32}$/.test(noteId))) {
      return res.serverError(403, 'Invalid note id', 'api.users.invalidNoteId');
    }

    if (req.params.userId !== req.user.userId) {
      return res.serverError(403, 'Can not change other users msg', 'api.users.cannotChangeOtherUsersMsg');
    }

    User.getUser(req.params.userId, (err, user) => {
      if (err || !user) {
        console.log(`ERROR - ${req.method} /api/user/%s/acknowledge (getUser)`, ArkimeUtil.sanitizeStr(req.params.userId), util.inspect(err, false, 50), user);
        return res.serverError(403, 'User not found', 'api.users.userNotFound');
      }

      if (noteId !== undefined) {
        // dismissedHelpNotes needs users db schema >= 87; fail open when the version is unknown
        const schemaVersion = User.getSchemaVersion();
        if (schemaVersion !== undefined && schemaVersion < 87) {
          return res.serverError(503, 'Users database needs an upgrade, run db.pl upgrade', 'api.users.dbNeedsUpgrade');
        }
        user.dismissedHelpNotes ??= [];
        if (!user.dismissedHelpNotes.includes(noteId)) {
          // 'all' supersedes everything, so it must never be blocked by the cap
          if (noteId !== 'all' && user.dismissedHelpNotes.length >= 50) {
            return res.serverError(403, 'Too many dismissed notes', 'api.users.tooManyDismissedNotes');
          }
          user.dismissedHelpNotes.push(noteId);
        }
      } else {
        user.welcomeMsgNum = parseInt(req.body.msgNum);
        if (!Number.isInteger(user.welcomeMsgNum)) {
          return res.serverError(403, 'welcomeMsgNum is not integer', 'api.users.welcomeMsgNumNotInteger');
        }
      }

      User.setUser(req.params.userId, user, (err, info) => {
        if (Config.debug) {
          console.log(`${req.method} /api/user/%s/acknowledge (setUser)`, ArkimeUtil.sanitizeStr(req.params.userId), util.inspect(err, false, 50), user, info);
        }

        if (noteId !== undefined) {
          if (err) {
            console.log(`ERROR - ${req.method} /api/user/%s/acknowledge (setUser)`, ArkimeUtil.sanitizeStr(req.params.userId), util.inspect(err, false, 50));
            return res.serverError(500, 'Note dismissal failed', 'api.users.dismissNoteFailed');
          }

          return res.json({
            success: true,
            text: `User, ${req.user.userId}, dismissed help note ${noteId}`,
            i18n: 'api.users.dismissedNote',
            i18nParams: { userId: req.user.userId, noteId }
          });
        }

        if (err) {
          console.log(`ERROR - ${req.method} /api/user/%s/acknowledge (setUser)`, ArkimeUtil.sanitizeStr(req.params.userId), util.inspect(err, false, 50));
          return res.serverError(500, 'Error dismissing message');
        }

        return res.json({
          success: true,
          text: `User, ${req.user.userId}, dismissed message ${user.welcomeMsgNum}`,
          i18n: 'api.users.dismissedMessage',
          i18nParams: { userId: req.user.userId, msgNum: user.welcomeMsgNum }
        });
      });
    });
  }

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
  }

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
        return res.serverError(403, 'Unknown user', 'api.users.unknownUser');
      }

      if (ArkimeUtil.isPP(req.params.name)) {
        return res.serverError(400, 'Invalid state name', 'api.users.invalidStateName');
      }

      if (!user.tableStates) {
        user.tableStates = {};
      }

      user.tableStates[req.params.name] = req.body;

      User.setUser(user.userId, user, (err, info) => {
        if (err) {
          console.log(`ERROR - ${req.method} /api/user/state/%s (setUser)`, ArkimeUtil.sanitizeStr(req.params.name), util.inspect(err, false, 50), info);
          return res.serverError(403, 'state update failed', 'api.users.stateUpdateFailed');
        }

        return res.json({
          success: true,
          text: 'updated state successfully',
          i18n: 'api.users.stateUpdated'
        });
      });
    });
  }

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
      const tableState = UserAPIs.findUserState('sessionsNew', req.user);
      const colWidths = UserAPIs.findUserState('sessionsColWidths', req.user);
      return res.send({ colWidths, tableState });
    }
    case 'spiview': {
      const spiviewFields = UserAPIs.findUserState('spiview', req.user);
      return res.send({ spiviewFields });
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
    case 'summary': {
      const summaryConfig = UserAPIs.findUserState('summary', req.user);
      return res.send({ summaryConfig });
    }
    default:
      return res.serverError(501, 'Requested page is not supported', 'api.users.unsupportedPage');
    }
  }
}

module.exports = UserAPIs;
